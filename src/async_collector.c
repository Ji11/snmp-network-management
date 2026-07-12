#include "collector.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

// 每个设备的异步会话状态，每个设备都有一个 sess，存储 snmp 会话指针
typedef struct {
    netsnmp_session *sess;
    device_t *device;
    int current_metric;  // 当前正在采集第几个 OID
} device_session_t;

// 回调函数可访问的全局上下文
static MYSQL *db_conn = NULL;
static int active_hosts = 0;

// 收到 SNMP 响应后将结果写入数据库
int save_result(netsnmp_pdu *pdu, char *device_name, char *metric_name, char *oid_text) {
    netsnmp_variable_list *v = pdu->variables;
    if (pdu->errstat == SNMP_ERR_NOERROR) {
        // 检查 PDU 里的错误状态，没有错误则逐个变量存入数据库
        while (v) {
            if (db_conn != NULL) {
                char value[MAX_VALUE_LEN];
                // snprint_value 只格式化值部分为 "类型: 值"（不含 OID 文本）
                snprint_value(value, sizeof(value), v->name, v->name_length, v);
                db_insert(db_conn, device_name, metric_name, oid_text, value);
            }
            v = v->next_variable;
        }
    }
    return 1;
}

// 成功 → 存储+推进 current_metric → snmp_send 下一个 OID
// 失败 → 递减 active_hosts，设备本轮结束
int response_callback(int operation, netsnmp_session *session, int reqid, netsnmp_pdu *pdu, void *magic) {
    // 先拿出 magic
    device_session_t *device_session = (device_session_t *)magic;
    device_t *device = device_session->device;

    if (operation == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
        // 移动到 current_metric
        metric_t *metric = &device->metrics[device_session->current_metric];

        save_result(pdu, device->name, metric->name, metric->oid_text);
        // 推进到下一个 OID
        device_session->current_metric++;
        if (device_session->current_metric < device->metric_count) {
            metric_t *next = &device->metrics[device_session->current_metric];
            // 创建 GET 类型的 SNMP PDU
            netsnmp_pdu *req = snmp_pdu_create(SNMP_MSG_GET);
            // 向 PDU 添加下一个 null 的 OID
            snmp_add_null_var(req, next->oid, next->oid_len);
            // 发送请求
            if (snmp_send(device_session->sess, req)) return 1;
            snmp_free_pdu(req);
        }
    }
    // 超时或发送失败，直接 active_hosts--

    active_hosts--;
    return 1;
}

// 一轮异步 SNMP 采集
// 初始化会话并发送首个 GET → select → 清理
int collect_async(collector_t *config) {
    if (config == NULL) return -1;

    // 连接数据库
    db_conn = db_connect(&config->db);
    db_cleanup(db_conn);

    int n = config->device_count;
    device_session_t *sessions = malloc(n * sizeof(device_session_t));

    active_hosts = 0;

    // 为每个设备打开 snmp 会话，发送第一个 GET
    for (int i = 0; i < n; i++) {
        device_t *device = &config->devices[i];

        // 临时的 netsnmp_session 结构体，填好参数后传给 snmp_open() 打开，然后 free 掉
        netsnmp_session sess;
        // 初始化 SNMP 会话结构体
        snmp_sess_init(&sess);
        sess.peername = strdup(device->peer);
        sess.community = (u_char *)strdup(device->community);
        sess.community_len = strlen(device->community);
        sess.version = SNMP_VERSION_2c;
        sess.retries = 1;
        sess.timeout = 1000000L;  // 1 秒超时
        sess.callback = response_callback; // 绑定回调，snmp_read 收到应答就调用回调
        sess.callback_magic = &sessions[i]; // 由 snmp_read 将 magic 传给回调，作为最后一个参数

        sessions[i].device = device;
        sessions[i].current_metric = 0;

        // snmp_open() 打开 snmp 会话，返回会话指针存到 sessions[i].sess
        // sessions[i].sess 专门用来存 snmp_open 返回的会话指针
        if (!(sessions[i].sess = snmp_open(&sess))) {
            snmp_perror("snmp_open");
            free(sess.peername);
            free(sess.community);
            continue;
        }
        free(sess.peername);
        free(sess.community);

        // 发送第一个 GET 请求，对应当前这个 device 的第一个 metric
        metric_t *metric = &device->metrics[0];
        netsnmp_pdu *req = snmp_pdu_create(SNMP_MSG_GET);
        // 向 GET 请求的 pdu 里添加一个 null 的 OID，发送端不用填值，等待对方在响应 pdu 里填回来
        snmp_add_null_var(req, metric->oid, metric->oid_len);
        if (snmp_send(sessions[i].sess, req)) active_hosts++;
        else {
            snmp_perror("snmp_send");
            snmp_free_pdu(req);
        }
    }

    // 如果所有设备 snmp_open 都失败，或者全部 snmp_send 都失败，active_hosts 就是 0，select() 没有任何 fd 可等，永久阻塞
    // 所以先检查一下
    if (active_hosts == 0) {
        fprintf(stderr, "[async] no active hosts, exiting\n");
        free(sessions);
        if (db_conn) db_close(db_conn);
        return 0;
    }

    // select 等待异步响应
    while (active_hosts > 0) {
        int numfds = 0, block = 1;
        fd_set fdset;
        struct timeval timeout;

        FD_ZERO(&fdset);
        // 获取需要监听的 fd 集合和超时时间
        // int snmp_select_info(numfds, fdset, timeout, block)
        // 如果还有设备没有回复第一轮发送的请求，block 会被设置为 1；如果没有了，snmp_select_info 会置为 0
        // block = 1 时，timeout 为 null，一直等；否则等到 timeout
        snmp_select_info(&numfds, &fdset, &timeout, &block);
        numfds = select(numfds, &fdset, NULL, NULL, block ? NULL : &timeout); // 返回就绪的 fd 数量
        if (numfds)
            // 处理就绪的 SNMP 响应（会触发回调）
            snmp_read(&fdset);
        else
            // 超时的会逐个调回调，回调里的 else 分支会命中
            snmp_timeout();
    }

    // 清理
    for (int i = 0; i < n; i++) {
        if (sessions[i].sess) snmp_close(sessions[i].sess);
    }
    free(sessions);

    if (db_conn) db_close(db_conn);
    return 0;
}

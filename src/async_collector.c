#include "collector.h"
#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

// 每个设备的异步会话状态，对应 asynchapp.c 的 struct session
typedef struct {
    netsnmp_session *sess;
    const target_config_t *target;
    int current_metric;  // 当前正在采集第几个 OID
} device_session_t;

// 回调函数可访问的全局上下文
static MYSQL *db_conn = NULL;
static int active_hosts = 0;

// 将 SNMP 变量值格式化为可读字符串
void format_value(const netsnmp_variable_list *vars, char *buffer, size_t size) {
    // snprint_value(): Net-SNMP 函数，将 OID+值 格式化为 "OID: value" 字符串
    snprint_value(buffer, size, vars->name, vars->name_length, vars);
}

// 处理单个 SNMP 响应，打印结果并写入数据库
// 对应 asynchapp.c 的 print_result 函数
int print_result(int status, netsnmp_session *sp, netsnmp_pdu *pdu,
                 const char *device_name, const char *metric_name,
                 const char *oid_text) {
    char buf[1024];
    struct timeval now;
    struct timezone tz;

    // gettimeofday(): 获取当前时间，精确到微秒
    gettimeofday(&now, &tz);
    struct tm *tm = localtime(&now.tv_sec);
    fprintf(stdout, "%.2d:%.2d:%.2d.%.6ld ",
            tm->tm_hour, tm->tm_min, tm->tm_sec, (long)now.tv_usec);

    switch (status) {
    case STAT_SUCCESS: {
        netsnmp_variable_list *vp = pdu->variables;
        if (pdu->errstat == SNMP_ERR_NOERROR) {
            while (vp) {
                // snprint_variable(): Net-SNMP 函数，格式化单个变量为字符串
                snprint_variable(buf, sizeof(buf), vp->name, vp->name_length, vp);
                fprintf(stdout, "%s: %s\n", sp->peername, buf);

                if (db_conn != NULL) {
                    char value[MAX_VALUE_LEN];
                    format_value(vp, value, sizeof(value));
                    db_insert(db_conn, device_name, metric_name, oid_text, value);
                }
                vp = vp->next_variable;
            }
        } else {
            int ix;
            for (ix = 1, vp = pdu->variables;
                 vp && ix != pdu->errindex;
                 vp = vp->next_variable, ix++);
            if (vp) snprint_objid(buf, sizeof(buf), vp->name, vp->name_length);
            else strcpy(buf, "(none)");
            fprintf(stdout, "%s: %s: %s\n",
                    sp->peername, buf, snmp_errstring(pdu->errstat));
        }
        return 1;
    }
    case STAT_TIMEOUT:
        fprintf(stdout, "%s: Timeout\n", sp->peername);
        return 0;
    case STAT_ERROR:
        snmp_perror(sp->peername);
        return 0;
    }
    return 0;
}

// 异步 SNMP 响应回调
// 对应 asynchapp.c 的回调模式：
//   成功 → 打印+存储 → 推进 current_metric → snmp_send 下一个 OID
//   失败 → 递减 active_hosts，设备本轮结束
int asynch_response(int operation, netsnmp_session *sp, int reqid,
                    netsnmp_pdu *pdu, void *magic) {
    (void)sp;
    (void)reqid;
    device_session_t *host = (device_session_t *)magic;
    const target_config_t *target = host->target;

    if (operation == NETSNMP_CALLBACK_OP_RECEIVED_MESSAGE) {
        const metric_config_t *metric = &target->metrics[host->current_metric];

        if (print_result(STAT_SUCCESS, host->sess, pdu,
                         target->name, metric->name, metric->oid_text)) {
            // 推进到下一个 OID
            host->current_metric++;
            if (host->current_metric < target->metric_count) {
                const metric_config_t *next = &target->metrics[host->current_metric];
                // snmp_pdu_create(): 创建指定类型的 SNMP PDU
                netsnmp_pdu *req = snmp_pdu_create(SNMP_MSG_GET);
                // snmp_add_null_var(): 向 PDU 添加一个 OID（值为 NULL，GET 请求用）
                snmp_add_null_var(req, next->oid, next->oid_len);
                // snmp_send(): 异步发送 SNMP 请求
                if (snmp_send(host->sess, req))
                    return 1;
                snmp_perror("snmp_send");
                snmp_free_pdu(req);
            }
        }
    } else {
        // 超时或发送失败
        const metric_config_t *metric = NULL;
        if (host->current_metric >= 0 && host->current_metric < target->metric_count)
            metric = &target->metrics[host->current_metric];
        print_result(STAT_TIMEOUT, host->sess, pdu,
                     target->name,
                     metric ? metric->name : "unknown",
                     metric ? metric->oid_text : "unknown");
    }

    // 本设备本轮采集结束
    active_hosts--;
    return 1;
}

// 版本字符串 → Net-SNMP 版本常量
long version_from_text(const char *version) {
    if (strcmp(version, "1") == 0 || strcmp(version, "v1") == 0)
        return SNMP_VERSION_1;
    if (strcmp(version, "2c") == 0 || strcmp(version, "v2c") == 0)
        return SNMP_VERSION_2c;
    return SNMP_VERSION_2c;
}

// 一轮异步 SNMP 采集
// 三阶段：初始化会话并发送首个 GET → select 事件循环 → 清理
int run_async_collection(const collector_config_t *config) {
    if (config == NULL) return -1;

    // 连接数据库
    db_conn = db_connect(&config->db);
    if (db_conn == NULL)
        fprintf(stderr, "[async] warning: database not available\n");

    int n = config->target_count;
    // calloc(): malloc + 自动清零，n 个元素每个大小为 sizeof(device_session_t)
    device_session_t *sessions = calloc(n, sizeof(device_session_t));
    if (sessions == NULL) {
        perror("calloc");
        if (db_conn) db_close(db_conn);
        return -1;
    }

    active_hosts = 0;

    // === 阶段 1: 为每个设备打开 SNMP 会话，发送第一个 GET ===
    for (int i = 0; i < n; i++) {
        const target_config_t *target = &config->targets[i];
        device_session_t *hs = &sessions[i];

        netsnmp_session sess;
        // snmp_sess_init(): 用默认值初始化 SNMP 会话结构体
        snmp_sess_init(&sess);
        sess.peername = strdup(target->peer);
        sess.community = (u_char *)strdup(target->community);
        sess.community_len = strlen(target->community);
        sess.version = version_from_text(target->version);
        sess.retries = target->retries;
        sess.timeout = target->timeout_ms * 1000L;
        sess.callback = asynch_response;
        sess.callback_magic = hs;

        hs->target = target;
        hs->current_metric = 0;

        // snmp_open(): 打开 SNMP 会话，返回会话指针
        if (!(hs->sess = snmp_open(&sess))) {
            snmp_perror("snmp_open");
            free(sess.peername);
            free(sess.community);
            continue;
        }
        free(sess.peername);
        free(sess.community);

        const metric_config_t *metric = &target->metrics[0];
        netsnmp_pdu *req = snmp_pdu_create(SNMP_MSG_GET);
        snmp_add_null_var(req, metric->oid, metric->oid_len);
        if (snmp_send(hs->sess, req))
            active_hosts++;
        else {
            snmp_perror("snmp_send");
            snmp_free_pdu(req);
        }
    }

    if (active_hosts == 0) {
        fprintf(stderr, "[async] no active hosts, exiting\n");
        free(sessions);
        if (db_conn) db_close(db_conn);
        return 0;
    }

    // === 阶段 2: select 事件循环，等待异步响应 ===
    // 对应 asynchapp.c 的标准 select 模式
    while (active_hosts > 0) {
        int fds = 0, block = 1;
        fd_set fdset;
        struct timeval timeout;

        FD_ZERO(&fdset);
        // snmp_select_info(): 获取需要监听的 fd 集合和超时时间
        snmp_select_info(&fds, &fdset, &timeout, &block);
        // select(): 多路复用 IO，等待 socket 可读或超时
        fds = select(fds, &fdset, NULL, NULL, block ? NULL : &timeout);
        if (fds < 0) {
            perror("select failed");
            break;
        }
        if (fds)
            // snmp_read(): 处理就绪的 SNMP 响应（会触发回调）
            snmp_read(&fdset);
        else
            // snmp_timeout(): 处理超时的 SNMP 请求
            snmp_timeout();
    }

    // === 阶段 3: 清理 ===
    for (int i = 0; i < n; i++) {
        if (sessions[i].sess)
            // snmp_close(): 关闭 SNMP 会话
            snmp_close(sessions[i].sess);
    }
    free(sessions);

    if (db_conn) db_close(db_conn);
    return 0;
}

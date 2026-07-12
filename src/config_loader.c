#include "collector.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 解析一个设备的所有 metric 条目
int load_device_metrics(config_setting_t *device_entry, device_t *device) {
    // config_setting_get_member() 获取 setting 的指定子成员
    config_setting_t *metrics = config_setting_get_member(device_entry, "metrics");
    int count = config_setting_length(metrics);

    device->metric_count = count;
    for (int i = 0; i < count; ++i) {
        config_setting_t *item = config_setting_get_elem(metrics, i);
        metric_t *metric = &device->metrics[i];

        const char *name = NULL, *oid = NULL;
        config_setting_lookup_string(item, "name", &name);
        config_setting_lookup_string(item, "oid", &oid);
        memcpy(metric->name, name, strlen(name) + 1);
        memcpy(metric->oid_text, oid, strlen(oid) + 1);

        memset(metric->oid, 0, sizeof(metric->oid));
        metric->oid_len = MAX_OID_LEN;
        // snmp_parse_oid() 将 "1.3.6.1..." 字符串解析为 netsnmp 的 oid 数组
        snmp_parse_oid(metric->oid_text, metric->oid, &metric->oid_len);
    }

    return 0;
}

// 解析 devices 节点
int load_devices(config_t *cfg, collector_t *config) {
    // 找到 devices 节点
    config_setting_t *devices = config_lookup(cfg, "devices");
    int count = config_setting_length(devices);
    config->device_count = count;

    for (int i = 0; i < count; ++i) {
        config_setting_t *item = config_setting_get_elem(devices, i);

        // 写入 device 中
        const char *name = NULL, *peer = NULL, *community = NULL;
        config_setting_lookup_string(item, "name", &name);
        config_setting_lookup_string(item, "peer", &peer);
        config_setting_lookup_string(item, "community", &community);
        memcpy(config->devices[i].name, name, strlen(name) + 1);
        memcpy(config->devices[i].peer, peer, strlen(peer) + 1);
        memcpy(config->devices[i].community, community, strlen(community) + 1);

        if (load_device_metrics(item, &config->devices[i]) != 0) return -1;
    }

    return 0;
}

// 加载完整配置文件，填充 collector_t 结构体
int load_collector_config(char *path, collector_t *config) {
    config_t cfg;
    if (path == NULL || config == NULL) return -1;

    memset(config, 0, sizeof(*config));
    config_init(&cfg);
    // 读配置文件
    if (!config_read_file(&cfg, path)) {
        fprintf(stderr, "config read error: %s:%d - %s\n",
                config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
        config_destroy(&cfg);
        return -1;
    }

    // [database] 节
    config_setting_t *db = config_lookup(&cfg, "database");
    if (db != NULL) {
        // config_setting_lookup_string 第三个参数要求 const
        const char *db_host = NULL, *db_user = NULL, *db_password = NULL, *db_name = NULL;
        
        config_setting_lookup_string(db, "host", &db_host);
        config_setting_lookup_string(db, "user", &db_user);
        config_setting_lookup_string(db, "password", &db_password);
        config_setting_lookup_string(db, "name", &db_name);
        // 读入数据库连接信息
        memcpy(config->db.host, db_host, strlen(db_host) + 1);
        memcpy(config->db.user, db_user, strlen(db_user) + 1);
        memcpy(config->db.password, db_password, strlen(db_password) + 1);
        memcpy(config->db.name, db_name, strlen(db_name) + 1);
    }

    if (load_devices(&cfg, config) != 0) {
        config_destroy(&cfg);
        return -1;
    }

    // config_destroy(): 释放 libconfig 配置对象
    config_destroy(&cfg);
    return 0;
}

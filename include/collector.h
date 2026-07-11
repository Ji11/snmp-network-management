#ifndef COLLECTOR_H
#define COLLECTOR_H

#include <libconfig.h>
#include <mysql/mysql.h>
#include <net-snmp/net-snmp-config.h>
#include <net-snmp/net-snmp-includes.h>

#define MAX_TARGETS 32
#define MAX_METRICS_PER_DEVICE 64
#define COLLECTOR_NAME_LEN 64
#define MAX_ADDR_LEN 128
#define MAX_COMMUNITY_LEN 64
#define MAX_VERSION_LEN 16
#define MAX_OID_TEXT_LEN 128
#define MAX_VALUE_LEN 256

// metrics 列表中的一项: { name = "..."; oid = "..."; }
typedef struct {
    char name[COLLECTOR_NAME_LEN];
    char oid_text[MAX_OID_TEXT_LEN];
    oid oid[MAX_OID_LEN];
    size_t oid_len;
} metric_t;

// devices 列表中的一项: { name, peer, community, version, metrics }
typedef struct {
    char name[COLLECTOR_NAME_LEN];
    char peer[MAX_ADDR_LEN];
    char community[MAX_COMMUNITY_LEN];
    char version[MAX_VERSION_LEN];
    int metric_count;
    metric_t metrics[MAX_METRICS_PER_DEVICE];
} target_t;

// 对应配置文件 database = { host, user, password, name }
typedef struct {
    char host[64];
    char user[64];
    char password[64];
    char name[64];
} db_t;

// 对应整个配置文件
typedef struct {
    db_t db;
    int target_count;
    target_t targets[MAX_TARGETS];
} collector_t;

int load_collector_config(char *path, collector_t *config);
int run_async_collection(collector_t *config);

#endif

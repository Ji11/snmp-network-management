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

typedef struct {
    char name[COLLECTOR_NAME_LEN];
    char oid_text[MAX_OID_TEXT_LEN];
    oid oid[MAX_OID_LEN];
    size_t oid_len;
} metric_config_t;

typedef struct {
    char name[COLLECTOR_NAME_LEN];
    char peer[MAX_ADDR_LEN];
    char community[MAX_COMMUNITY_LEN];
    char version[MAX_VERSION_LEN];
    int retries;
    int timeout_ms;
    int metric_count;
    metric_config_t metrics[MAX_METRICS_PER_DEVICE];
} target_config_t;

typedef struct {
    char host[64];
    char user[64];
    char password[64];
    char name[64];
} db_config_t;

typedef struct {
    char mode[16];
    int poll_interval_sec;
    db_config_t db;
    int target_count;
    target_config_t targets[MAX_TARGETS];
} collector_config_t;

int load_collector_config(const char *path, collector_config_t *config);
int run_async_collection(const collector_config_t *config);

#endif

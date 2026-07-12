#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

MYSQL *db_connect(db_t *config) {
    MYSQL *conn = mysql_init(NULL); // 初始化 MYSQL 连接句柄

    // 建立 mysql 连接 host/user/passwd/db/port/unix_socket/flags
    mysql_real_connect(conn, config->host, config->user, config->password, NULL, 0, NULL, 0);

    // 建库
    char sql[256];
    snprintf(sql, sizeof(sql),
             "CREATE DATABASE IF NOT EXISTS `%s` "
             "DEFAULT CHARACTER SET utf8",
             config->name);
    mysql_query(conn, sql);

    // 选库
    mysql_select_db(conn, config->name);

    // 建表
    char *create_table =
        "CREATE TABLE IF NOT EXISTS snmp_data ("
        "  id         INT AUTO_INCREMENT PRIMARY KEY,"
        "  device     VARCHAR(32)  NOT NULL,"
        "  metric     VARCHAR(64)  NOT NULL,"
        "  oid        VARCHAR(128) NOT NULL,"
        "  value      VARCHAR(256) NOT NULL DEFAULT '',"
        "  created_at DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8";

    mysql_query(conn, create_table);

    printf("[db] connected, database='%s'\n", config->name);
    return conn;
}

// 写入一条采集数据
int db_insert(MYSQL *conn, char *device_name, char *metric_name, char *oid, char *value) {
    if (conn == NULL || device_name == NULL || metric_name == NULL || oid == NULL || value == NULL) {
        return -1;
    }

    // mysql_real_escape_string() 转义 SQL 特殊字符
    char esc_device[128], esc_metric[128], esc_oid[256], esc_value[512];
    mysql_real_escape_string(conn, esc_device, device_name, strlen(device_name));
    mysql_real_escape_string(conn, esc_metric, metric_name, strlen(metric_name));
    mysql_real_escape_string(conn, esc_oid, oid, strlen(oid));
    mysql_real_escape_string(conn, esc_value, value, strlen(value));

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "INSERT INTO snmp_data (device, metric, oid, value) "
             "VALUES ('%s', '%s', '%s', '%s')",
             esc_device, esc_metric, esc_oid, esc_value);

    mysql_query(conn, sql);

    return 0;
}

void db_cleanup(MYSQL *conn) {
    if (conn == NULL) return;
    mysql_query(conn,
        "DELETE FROM snmp_data WHERE created_at < NOW() - INTERVAL 2 DAY");
}

void db_close(MYSQL *conn) {
    if (conn != NULL) mysql_close(conn);
}

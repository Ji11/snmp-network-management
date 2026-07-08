#include "db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

MYSQL *db_connect(const db_config_t *config) {
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init: alloc failed\n");
        return NULL;
    }

    /* 先不选库，连上后建库 */
    if (!mysql_real_connect(conn, config->host, config->user,
                            config->password, NULL, 0, NULL, 0)) {
        fprintf(stderr, "mysql_real_connect: %s\n", mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }

    /* 建库 */
    char sql[256];
    snprintf(sql, sizeof(sql),
             "CREATE DATABASE IF NOT EXISTS `%s` "
             "DEFAULT CHARACTER SET utf8",
             config->name);
    if (mysql_query(conn, sql)) {
        fprintf(stderr, "create database: %s\n", mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }

    /* 选库 */
    if (mysql_select_db(conn, config->name)) {
        fprintf(stderr, "mysql_select_db: %s\n", mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }

    /* 建表 */
    const char *create_table =
        "CREATE TABLE IF NOT EXISTS snmp_data ("
        "  id         INT AUTO_INCREMENT PRIMARY KEY,"
        "  device     VARCHAR(32)  NOT NULL,"
        "  metric     VARCHAR(64)  NOT NULL,"
        "  oid        VARCHAR(128) NOT NULL,"
        "  value      VARCHAR(256) NOT NULL DEFAULT '',"
        "  created_at DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8";

    if (mysql_query(conn, create_table)) {
        fprintf(stderr, "create table: %s\n", mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }

    printf("[db] connected, database='%s'\n", config->name);
    return conn;
}

int db_insert(MYSQL *conn, const char *device_name, const char *metric_name,
              const char *oid, const char *value) {
    if (conn == NULL || device_name == NULL || metric_name == NULL
        || oid == NULL || value == NULL) {
        return -1;
    }

    /* 转义单引号 */
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

    if (mysql_query(conn, sql)) {
        fprintf(stderr, "db insert: %s\n", mysql_error(conn));
        return -1;
    }

    return 0;
}

void db_close(MYSQL *conn) {
    if (conn != NULL) {
        mysql_close(conn);
    }
}

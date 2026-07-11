#ifndef DB_H
#define DB_H

#include "collector.h"

/* 初始化连接，不存在则创建数据库和表 */
MYSQL *db_connect(db_t *config);

/* 写入一条采集数据 */
int db_insert(MYSQL *conn, char *device_name, char *metric_name,
              char *oid, char *value);

/* 关闭连接 */
void db_close(MYSQL *conn);

#endif

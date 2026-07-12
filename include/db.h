#ifndef DB_H
#define DB_H

#include "collector.h"

MYSQL *db_connect(db_t *config);
int db_insert(MYSQL *conn, char *device_name, char *metric_name, char *oid, char *value);
void db_close(MYSQL *conn);

#endif

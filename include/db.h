#ifndef DB_H
#define DB_H

#include <mysql.h>
#include <stdbool.h>
#include <stdint.h>

bool db_init(void);
void db_close(void);
MYSQL* db_get_conn(void);

// Query execution
bool db_query(const char *sql);
MYSQL_RES* db_query_store(const char *sql);
int64_t db_insert_id(void);
int64_t db_affected_rows(void);
void db_free_result(MYSQL_RES *res);

// Transaction boundaries
bool db_begin(void);
bool db_commit(void);
bool db_rollback(void);

// SQL Escaping
void db_escape_string(const char *src, char *dst, size_t max_len);

// Audit Logging
void db_audit_log(const char *actor_type, int actor_id, const char *action,
                  const char *entity_type, int entity_id, const char *details,
                  const char *ip_address);

#endif // DB_H

#include "db.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static MYSQL *s_conn = NULL;

bool db_init(void) {
    if (s_conn) return true;

    s_conn = mysql_init(NULL);
    if (!s_conn) {
        fprintf(stderr, "[DB] mysql_init failed!\n");
        return false;
    }

    bool reconnect = true;
    mysql_options(s_conn, MYSQL_OPT_RECONNECT, &reconnect);
    mysql_options(s_conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    const char *pass = (strlen(g_config.db_pass) > 0) ? g_config.db_pass : NULL;
    if (!mysql_real_connect(s_conn, g_config.db_host, g_config.db_user, pass,
                            g_config.db_name, g_config.db_port, NULL, 0)) {
        fprintf(stderr, "[DB] Connection error: %s (errno %u)\n", mysql_error(s_conn), mysql_errno(s_conn));
        mysql_close(s_conn);
        s_conn = NULL;
        return false;
    }

    mysql_set_character_set(s_conn, "utf8mb4");

    printf("[DB] Successfully connected to MySQL 8.0 [%s@%s:%d/%s]\n",
           g_config.db_user, g_config.db_host, g_config.db_port, g_config.db_name);
    return true;
}

void db_close(void) {
    if (s_conn) {
        mysql_close(s_conn);
        s_conn = NULL;
        printf("[DB] MySQL connection closed\n");
    }
}

MYSQL* db_get_conn(void) {
    if (!s_conn) {
        db_init();
    }
    return s_conn;
}

bool db_query(const char *sql) {
    MYSQL *conn = db_get_conn();
    if (!conn) return false;
    if (mysql_query(conn, sql)) {
        fprintf(stderr, "[DB] Query error: %s\nSQL: %s\n", mysql_error(conn), sql);
        return false;
    }
    return true;
}

MYSQL_RES* db_query_store(const char *sql) {
    MYSQL *conn = db_get_conn();
    if (!conn) return NULL;
    if (mysql_query(conn, sql)) {
        fprintf(stderr, "[DB] Query store error: %s\nSQL: %s\n", mysql_error(conn), sql);
        return NULL;
    }
    return mysql_store_result(conn);
}

int64_t db_insert_id(void) {
    MYSQL *conn = db_get_conn();
    return conn ? (int64_t)mysql_insert_id(conn) : 0;
}

int64_t db_affected_rows(void) {
    MYSQL *conn = db_get_conn();
    return conn ? (int64_t)mysql_affected_rows(conn) : 0;
}

void db_free_result(MYSQL_RES *res) {
    if (res) {
        mysql_free_result(res);
    }
}

bool db_begin(void) {
    return db_query("START TRANSACTION;");
}

bool db_commit(void) {
    return db_query("COMMIT;");
}

bool db_rollback(void) {
    return db_query("ROLLBACK;");
}

void db_escape_string(const char *src, char *dst, size_t max_len) {
    if (!src || !dst || max_len == 0) return;
    MYSQL *conn = db_get_conn();
    if (conn) {
        unsigned long src_len = (unsigned long)strlen(src);
        char *temp = (char*)malloc(src_len * 2 + 1);
        if (temp) {
            unsigned long esc_len = mysql_real_escape_string(conn, temp, src, src_len);
            if (esc_len >= max_len) esc_len = (unsigned long)(max_len - 1);
            memcpy(dst, temp, esc_len);
            dst[esc_len] = '\0';
            free(temp);
        } else {
            snprintf(dst, max_len, "%s", src);
        }
    } else {
        // Fallback simple escape
        size_t d = 0;
        for (size_t s = 0; src[s] && d + 2 < max_len; s++) {
            if (src[s] == '\'' || src[s] == '\"' || src[s] == '\\') {
                dst[d++] = '\\';
            }
            dst[d++] = src[s];
        }
        dst[d] = '\0';
    }
}

void db_audit_log(const char *actor_type, int actor_id, const char *action,
                  const char *entity_type, int entity_id, const char *details,
                  const char *ip_address) {
    char esc_action[128], esc_details[1024], esc_ip[64];
    db_escape_string(action ? action : "", esc_action, sizeof(esc_action));
    db_escape_string(details ? details : "", esc_details, sizeof(esc_details));
    db_escape_string(ip_address ? ip_address : "127.0.0.1", esc_ip, sizeof(esc_ip));

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "INSERT INTO audit_logs (actor_type, actor_id, action, entity_type, entity_id, details, ip_address) "
             "VALUES ('%s', %d, '%s', '%s', %d, '%s', '%s')",
             actor_type ? actor_type : "SYSTEM", actor_id, esc_action,
             entity_type ? entity_type : "GENERAL", entity_id, esc_details, esc_ip);
    db_query(sql);
}

#include "auth.h"
#include "db.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <argon2.h>
#include <windows.h>

#define ARGON2_TIME_COST 2
#define ARGON2_MEMORY_COST 65536
#define ARGON2_PARALLELISM 1
#define ARGON2_HASH_LEN 32
#define ARGON2_SALT_LEN 16

static const char *s_default_salt = "smartbankingsalt";

bool auth_hash_password(const char *password, char *out_hash, size_t out_len) {
    if (!password || !out_hash || out_len == 0) return false;
    int res = argon2id_hash_encoded(
        ARGON2_TIME_COST, ARGON2_MEMORY_COST, ARGON2_PARALLELISM,
        password, strlen(password),
        s_default_salt, strlen(s_default_salt),
        ARGON2_HASH_LEN,
        out_hash, out_len
    );
    return (res == ARGON2_OK);
}

bool auth_verify_password(const char *password, const char *hash) {
    if (!password || !hash) return false;
    int res = argon2id_verify(hash, password, strlen(password));
    return (res == ARGON2_OK);
}

bool auth_set_pin(int customer_id, const char *pin) {
    if (customer_id <= 0 || !pin || strlen(pin) < 4) return false;
    char hash[MAX_HASH_LEN];
    if (!auth_hash_password(pin, hash, sizeof(hash))) return false;

    char esc_hash[MAX_HASH_LEN * 2];
    db_escape_string(hash, esc_hash, sizeof(esc_hash));

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "INSERT INTO transaction_pins (customer_id, pin_hash) VALUES (%d, '%s') "
             "ON DUPLICATE KEY UPDATE pin_hash = '%s'",
             customer_id, esc_hash, esc_hash);
    return db_query(sql);
}

bool auth_verify_pin(int customer_id, const char *pin) {
    if (customer_id <= 0 || !pin) return false;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT pin_hash FROM transaction_pins WHERE customer_id = %d LIMIT 1", customer_id);
    MYSQL_RES *res = db_query_store(sql);
    if (!res) return false;

    MYSQL_ROW row = mysql_fetch_row(res);
    bool valid = false;
    if (row && row[0]) {
        valid = auth_verify_password(pin, row[0]);
    }
    db_free_result(res);
    return valid;
}

bool auth_has_pin(int customer_id) {
    if (customer_id <= 0) return false;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT 1 FROM transaction_pins WHERE customer_id = %d LIMIT 1", customer_id);
    MYSQL_RES *res = db_query_store(sql);
    if (!res) return false;
    bool exists = (mysql_num_rows(res) > 0);
    db_free_result(res);
    return exists;
}

void auth_generate_token(char *out, size_t len) {
    if (!out || len == 0) return;
    static const char hex[] = "0123456789abcdef";
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ GetCurrentProcessId());
        seeded = true;
    }

    size_t half = (len - 1) / 2;
    for (size_t i = 0; i < half; i++) {
        unsigned char r = (unsigned char)(rand() % 256);
        out[i * 2]     = hex[(r >> 4) & 0x0F];
        out[i * 2 + 1] = hex[r & 0x0F];
    }
    out[half * 2] = '\0';
}

bool auth_create_session(const char *user_type, int user_id, const char *role,
                         const char *ip, const char *user_agent, Session *out_sess) {
    if (!user_type || user_id <= 0 || !out_sess) return false;

    char session_token[65];
    char csrf_token[33];
    auth_generate_token(session_token, sizeof(session_token));
    auth_generate_token(csrf_token, sizeof(csrf_token));

    char esc_ip[64], esc_ua[256];
    db_escape_string(ip ? ip : "127.0.0.1", esc_ip, sizeof(esc_ip));
    db_escape_string(user_agent ? user_agent : "Unknown", esc_ua, sizeof(esc_ua));

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "INSERT INTO sessions (session_token, user_type, user_id, role, csrf_token, ip_address, user_agent, expires_at) "
             "VALUES ('%s', '%s', %d, '%s', '%s', '%s', '%s', DATE_ADD(NOW(), INTERVAL 24 HOUR))",
             session_token, user_type, user_id, role ? role : "CUSTOMER", csrf_token, esc_ip, esc_ua);

    if (!db_query(sql)) return false;

    snprintf(out_sess->session_token, sizeof(out_sess->session_token), "%s", session_token);
    snprintf(out_sess->user_type, sizeof(out_sess->user_type), "%s", user_type);
    out_sess->user_id = user_id;
    snprintf(out_sess->role, sizeof(out_sess->role), "%s", role ? role : "CUSTOMER");
    snprintf(out_sess->csrf_token, sizeof(out_sess->csrf_token), "%s", csrf_token);
    snprintf(out_sess->ip_address, sizeof(out_sess->ip_address), "%s", esc_ip);
    snprintf(out_sess->user_agent, sizeof(out_sess->user_agent), "%s", esc_ua);
    return true;
}

bool auth_validate_session(const char *token, Session *out_sess) {
    if (!token || strlen(token) < 10) return false;
    char esc_tok[256];
    db_escape_string(token, esc_tok, sizeof(esc_tok));

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, session_token, user_type, user_id, role, csrf_token, ip_address, user_agent, expires_at "
             "FROM sessions WHERE session_token = '%s' AND expires_at > NOW() LIMIT 1", esc_tok);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return false;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        db_free_result(res);
        return false;
    }

    if (out_sess) {
        out_sess->id = atoi(row[0]);
        strncpy(out_sess->session_token, row[1] ? row[1] : "", sizeof(out_sess->session_token) - 1);
        strncpy(out_sess->user_type, row[2] ? row[2] : "", sizeof(out_sess->user_type) - 1);
        out_sess->user_id = atoi(row[3]);
        strncpy(out_sess->role, row[4] ? row[4] : "CUSTOMER", sizeof(out_sess->role) - 1);
        strncpy(out_sess->csrf_token, row[5] ? row[5] : "", sizeof(out_sess->csrf_token) - 1);
        strncpy(out_sess->ip_address, row[6] ? row[6] : "", sizeof(out_sess->ip_address) - 1);
        strncpy(out_sess->user_agent, row[7] ? row[7] : "", sizeof(out_sess->user_agent) - 1);
        strncpy(out_sess->expires_at, row[8] ? row[8] : "", sizeof(out_sess->expires_at) - 1);
    }
    db_free_result(res);
    return true;
}

bool auth_destroy_session(const char *token) {
    if (!token) return false;
    char esc_tok[128];
    db_escape_string(token, esc_tok, sizeof(esc_tok));
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM sessions WHERE session_token = '%s'", esc_tok);
    return db_query(sql);
}

bool auth_validate_csrf(const Session *sess, const char *csrf_token) {
    if (!sess || !csrf_token) return false;
    return (strcmp(sess->csrf_token, csrf_token) == 0);
}

bool auth_check_permission(const char *user_role, const char *required_role) {
    if (!user_role || !required_role) return false;
    if (strcmp(user_role, "SUPER_ADMIN") == 0) return true; // Super admin has all permissions
    if (strcmp(required_role, user_role) == 0) return true;

    // ADMIN hierarchy
    if (strcmp(user_role, "ADMIN") == 0) {
        if (strcmp(required_role, "SUPPORT") == 0 || strcmp(required_role, "AUDITOR") == 0) {
            return true;
        }
    }
    return false;
}

void auth_record_login(const char *user_type, int user_id, const char *ip,
                       const char *user_agent, bool success, const char *reason) {
    char esc_ip[64], esc_ua[256], esc_reason[128];
    db_escape_string(ip ? ip : "127.0.0.1", esc_ip, sizeof(esc_ip));
    db_escape_string(user_agent ? user_agent : "Unknown", esc_ua, sizeof(esc_ua));
    db_escape_string(reason ? reason : "", esc_reason, sizeof(esc_reason));

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "INSERT INTO login_history (user_type, user_id, ip_address, user_agent, status, failure_reason) "
             "VALUES ('%s', %d, '%s', '%s', '%s', '%s')",
             user_type ? user_type : "CUSTOMER", user_id, esc_ip, esc_ua,
             success ? "SUCCESS" : "FAILED", esc_reason);
    db_query(sql);
}

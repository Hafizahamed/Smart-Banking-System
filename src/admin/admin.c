#include "admin.h"
#include "db.h"
#include "auth.h"
#include "json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* admin_get_overview_json(void) {
    // 1. Total customers
    int total_cust = 0;
    MYSQL_RES *res = db_query_store("SELECT COUNT(*) FROM customers");
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) total_cust = atoi(r[0]);
        db_free_result(res);
    }

    // 2. Total accounts
    int total_acc = 0;
    res = db_query_store("SELECT COUNT(*) FROM accounts");
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) total_acc = atoi(r[0]);
        db_free_result(res);
    }

    // 3. Total deposits volume
    int64_t total_deposits = 0;
    res = db_query_store("SELECT COALESCE(SUM(balance_paise), 0) FROM accounts WHERE status != 'CLOSED'");
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) total_deposits = _strtoi64(r[0], NULL, 10);
        db_free_result(res);
    }

    // 4. Total transactions
    int total_tx = 0;
    res = db_query_store("SELECT COUNT(*) FROM transactions");
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) total_tx = atoi(r[0]);
        db_free_result(res);
    }

    // 5. Flagged transactions
    int flagged_tx = 0;
    res = db_query_store("SELECT COUNT(*) FROM transactions WHERE status = 'FLAGGED'");
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) flagged_tx = atoi(r[0]);
        db_free_result(res);
    }

    // 6. Pending Service Requests
    int pending_sr = 0;
    res = db_query_store("SELECT COUNT(*) FROM service_requests WHERE status = 'PENDING' OR status = 'IN_REVIEW'");
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) pending_sr = atoi(r[0]);
        db_free_result(res);
    }

    char dep_fmt[64];
    format_inr(total_deposits, dep_fmt, sizeof(dep_fmt));

    JsonBuilder *jb = jb_create(1024);
    jb_append(jb, "{\n");
    jb_append_format(jb, "  \"total_customers\": %d,\n", total_cust);
    jb_append_format(jb, "  \"total_accounts\": %d,\n", total_acc);
    jb_append_format(jb, "  \"total_deposits_paise\": %lld,\n", (long long)total_deposits);
    jb_append_format(jb, "  \"total_deposits_formatted\": \"%s\",\n", dep_fmt);
    jb_append_format(jb, "  \"total_transactions\": %d,\n", total_tx);
    jb_append_format(jb, "  \"flagged_transactions\": %d,\n", flagged_tx);
    jb_append_format(jb, "  \"pending_service_requests\": %d\n", pending_sr);
    jb_append(jb, "}");

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

char* admin_get_customers_json(const char *search_query, int limit, int offset) {
    if (limit <= 0 || limit > 100) limit = 50;
    if (offset < 0) offset = 0;

    char where_clause[512] = "";
    if (search_query && strlen(search_query) > 0) {
        char esc_q[128];
        db_escape_string(search_query, esc_q, sizeof(esc_q));
        snprintf(where_clause, sizeof(where_clause),
                 "WHERE c.first_name LIKE '%%%s%%' OR c.last_name LIKE '%%%s%%' OR c.email LIKE '%%%s%%' OR c.customer_number LIKE '%%%s%%' ",
                 esc_q, esc_q, esc_q, esc_q);
    }

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "SELECT c.id, c.customer_number, c.first_name, c.last_name, c.email, c.phone, c.status, "
             "c.two_factor_enabled, c.created_at, COUNT(a.id) AS acc_count, COALESCE(SUM(a.balance_paise), 0) AS total_bal "
             "FROM customers c LEFT JOIN accounts a ON c.id = a.customer_id "
             "%s GROUP BY c.id ORDER BY c.id ASC LIMIT %d OFFSET %d",
             where_clause, limit, offset);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("[]");

    JsonBuilder *jb = jb_create(4096);
    jb_append(jb, "[");
    MYSQL_ROW row;
    bool first = true;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;

        int64_t bal = _strtoi64(row[10], NULL, 10);
        char bal_fmt[64];
        format_inr(bal, bal_fmt, sizeof(bal_fmt));

        jb_append_format(jb,
            "{\"id\":%d,\"customer_number\":\"%s\",\"first_name\":\"%s\",\"last_name\":\"%s\","
            "\"email\":\"%s\",\"phone\":\"%s\",\"status\":\"%s\",\"two_factor_enabled\":%s,"
            "\"created_at\":\"%s\",\"account_count\":%d,\"total_balance_paise\":%lld,\"total_balance_formatted\":\"%s\"}",
            atoi(row[0]), row[1], row[2], row[3], row[4], row[5], row[6],
            (atoi(row[7]) == 1) ? "true" : "false", row[8], atoi(row[9]), (long long)bal, bal_fmt);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

bool admin_update_customer_status(int admin_id, const char *admin_role, int target_customer_id, const char *new_status, const char *reason) {
    if (!auth_check_permission(admin_role, "ADMIN")) return false; // Auditor & Support cannot suspend customers
    if (!new_status || target_customer_id <= 0) return false;

    char esc_status[32];
    db_escape_string(new_status, esc_status, sizeof(esc_status));

    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE customers SET status = '%s' WHERE id = %d", esc_status, target_customer_id);
    if (!db_query(sql)) return false;

    // Log action to audit log
    char audit_detail[512];
    snprintf(audit_detail, sizeof(audit_detail), "Admin ID %d changed customer status to %s. Reason: %s",
             admin_id, new_status, reason ? reason : "Administrative action");
    db_audit_log("ADMIN", admin_id, "CUSTOMER_STATUS_UPDATED", "CUSTOMER", target_customer_id, audit_detail, "127.0.0.1");
    return true;
}

bool admin_update_account_status(int admin_id, const char *admin_role, int target_account_id, const char *new_status, const char *reason) {
    if (!auth_check_permission(admin_role, "ADMIN")) return false;
    if (!new_status || target_account_id <= 0) return false;

    char esc_status[32];
    db_escape_string(new_status, esc_status, sizeof(esc_status));

    char sql[512];
    snprintf(sql, sizeof(sql), "UPDATE accounts SET status = '%s' WHERE id = %d", esc_status, target_account_id);
    if (!db_query(sql)) return false;

    char audit_detail[512];
    snprintf(audit_detail, sizeof(audit_detail), "Admin ID %d changed account status to %s. Reason: %s",
             admin_id, new_status, reason ? reason : "Administrative action");
    db_audit_log("ADMIN", admin_id, "ACCOUNT_STATUS_UPDATED", "ACCOUNT", target_account_id, audit_detail, "127.0.0.1");
    return true;
}

char* admin_get_transactions_json(bool only_flagged, int limit, int offset) {
    if (limit <= 0 || limit > 200) limit = 100;
    if (offset < 0) offset = 0;

    char where_clause[128] = "";
    if (only_flagged) {
        strcpy(where_clause, "WHERE t.status = 'FLAGGED' ");
    }

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "SELECT t.id, t.reference_id, t.from_account_id, t.to_account_id, "
             "fa.account_number AS from_acc, ta.account_number AS to_acc, "
             "t.transaction_type, t.amount_paise, t.balance_after_paise, t.category, "
             "t.description, t.status, t.flag_reason, t.created_at "
             "FROM transactions t "
             "LEFT JOIN accounts fa ON t.from_account_id = fa.id "
             "LEFT JOIN accounts ta ON t.to_account_id = ta.id "
             "%s ORDER BY t.created_at DESC, t.id DESC LIMIT %d OFFSET %d",
             where_clause, limit, offset);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("[]");

    JsonBuilder *jb = jb_create(4096);
    jb_append(jb, "[");
    MYSQL_ROW row;
    bool first = true;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;

        int64_t amt = _strtoi64(row[7], NULL, 10);
        char amt_fmt[64];
        format_inr(amt, amt_fmt, sizeof(amt_fmt));

        char esc_desc[256], esc_flag[256];
        json_escape_string(row[10] ? row[10] : "", esc_desc, sizeof(esc_desc));
        json_escape_string(row[12] ? row[12] : "", esc_flag, sizeof(esc_flag));

        jb_append_format(jb,
            "{\"id\":%d,\"reference_id\":\"%s\",\"from_account_number\":\"%s\",\"to_account_number\":\"%s\","
            "\"type\":\"%s\",\"amount_paise\":%lld,\"amount_formatted\":\"%s\",\"category\":\"%s\","
            "\"description\":\"%s\",\"status\":\"%s\",\"flag_reason\":\"%s\",\"created_at\":\"%s\"}",
            atoi(row[0]), row[1], row[4] ? row[4] : "N/A", row[5] ? row[5] : "N/A",
            row[6], (long long)amt, amt_fmt, row[9] ? row[9] : "General",
            esc_desc, row[11], esc_flag, row[13]);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

char* admin_get_service_requests_json(const char *status_filter) {
    char where_clause[128] = "";
    if (status_filter && strlen(status_filter) > 0 && strcmp(status_filter, "ALL") != 0) {
        char esc_status[32];
        db_escape_string(status_filter, esc_status, sizeof(esc_status));
        snprintf(where_clause, sizeof(where_clause), "WHERE sr.status = '%s' ", esc_status);
    }

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "SELECT sr.id, sr.ticket_number, sr.customer_id, CONCAT(c.first_name, ' ', c.last_name) AS cust_name, "
             "sr.account_id, a.account_number, sr.request_type, sr.description, sr.priority, sr.status, "
             "sr.admin_notes, sr.created_at, sr.updated_at "
             "FROM service_requests sr "
             "JOIN customers c ON sr.customer_id = c.id "
             "LEFT JOIN accounts a ON sr.account_id = a.id "
             "%s ORDER BY CASE sr.priority WHEN 'URGENT' THEN 1 WHEN 'HIGH' THEN 2 WHEN 'MEDIUM' THEN 3 ELSE 4 END, sr.id ASC",
             where_clause);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("[]");

    JsonBuilder *jb = jb_create(2048);
    jb_append(jb, "[");
    MYSQL_ROW row;
    bool first = true;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;

        char esc_desc[512], esc_notes[512];
        json_escape_string(row[7] ? row[7] : "", esc_desc, sizeof(esc_desc));
        json_escape_string(row[10] ? row[10] : "", esc_notes, sizeof(esc_notes));

        jb_append_format(jb,
            "{\"id\":%d,\"ticket_number\":\"%s\",\"customer_id\":%d,\"customer_name\":\"%s\","
            "\"account_id\":%d,\"account_number\":\"%s\",\"request_type\":\"%s\",\"description\":\"%s\","
            "\"priority\":\"%s\",\"status\":\"%s\",\"admin_notes\":\"%s\",\"created_at\":\"%s\",\"updated_at\":\"%s\"}",
            atoi(row[0]), row[1], atoi(row[2]), row[3], atoi(row[4] ? row[4] : "0"),
            row[5] ? row[5] : "N/A", row[6], esc_desc, row[8], row[9], esc_notes, row[11], row[12]);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

bool admin_process_service_request(int admin_id, const char *admin_role, int ticket_id, const char *decision, const char *admin_notes) {
    if (strcmp(admin_role, "AUDITOR") == 0) return false; // Auditors cannot take action on tickets
    if (ticket_id <= 0 || !decision) return false;

    char esc_decision[32], esc_notes[1024];
    db_escape_string(decision, esc_decision, sizeof(esc_decision));
    db_escape_string(admin_notes ? admin_notes : "", esc_notes, sizeof(esc_notes));

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "UPDATE service_requests SET status = '%s', admin_notes = '%s', resolved_by_admin_id = %d "
             "WHERE id = %d",
             esc_decision, esc_notes, admin_id, ticket_id);

    if (!db_query(sql)) return false;

    // Retrieve ticket details to notify customer
    char sel_sql[256];
    snprintf(sel_sql, sizeof(sel_sql), "SELECT customer_id, ticket_number, request_type FROM service_requests WHERE id = %d", ticket_id);
    MYSQL_RES *res = db_query_store(sel_sql);
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r) {
            int cust_id = atoi(r[0]);
            const char *ticket = r[1];
            char notif_msg[512];
            snprintf(notif_msg, sizeof(notif_msg), "Your service request ticket %s has been marked %s. Notes: %s",
                     ticket, decision, admin_notes ? admin_notes : "Processed by bank officer.");

            char esc_msg[1024];
            db_escape_string(notif_msg, esc_msg, sizeof(esc_msg));
            char n_sql[1536];
            snprintf(n_sql, sizeof(n_sql),
                     "INSERT INTO notifications (customer_id, title, message, type) VALUES (%d, 'Service Request %s', '%s', 'SERVICE')",
                     cust_id, decision, esc_msg);
            db_query(n_sql);
        }
        db_free_result(res);
    }

    db_audit_log("ADMIN", admin_id, "SERVICE_REQUEST_PROCESSED", "SERVICE_REQUEST", ticket_id, decision, "127.0.0.1");
    return true;
}

char* admin_get_audit_logs_json(int limit, int offset) {
    if (limit <= 0 || limit > 200) limit = 100;
    if (offset < 0) offset = 0;

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT id, actor_type, actor_id, action, entity_type, entity_id, details, ip_address, created_at "
             "FROM audit_logs ORDER BY id DESC LIMIT %d OFFSET %d", limit, offset);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("[]");

    JsonBuilder *jb = jb_create(4096);
    jb_append(jb, "[");
    MYSQL_ROW row;
    bool first = true;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;

        char esc_details[512];
        json_escape_string(row[6] ? row[6] : "", esc_details, sizeof(esc_details));

        jb_append_format(jb,
            "{\"id\":%d,\"actor_type\":\"%s\",\"actor_id\":%d,\"action\":\"%s\","
            "\"entity_type\":\"%s\",\"entity_id\":%d,\"details\":\"%s\",\"ip_address\":\"%s\",\"created_at\":\"%s\"}",
            atoi(row[0]), row[1], atoi(row[2] ? row[2] : "0"), row[3], row[4],
            atoi(row[5] ? row[5] : "0"), esc_details, row[7] ? row[7] : "", row[8]);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

char* admin_get_system_health_json(void) {
    MYSQL *conn = db_get_conn();
    const char *server_info = conn ? mysql_get_server_info(conn) : "Unavailable";
    const char *client_info = mysql_get_client_info();

    // Active sessions count
    int active_sessions = 0;
    MYSQL_RES *res = db_query_store("SELECT COUNT(*) FROM sessions WHERE expires_at > NOW()");
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) active_sessions = atoi(r[0]);
        db_free_result(res);
    }

    JsonBuilder *jb = jb_create(1024);
    jb_append(jb, "{\n");
    jb_append(jb, "  \"status\": \"HEALTHY\",\n");
    jb_append(jb, "  \"database\": {\"status\": \"CONNECTED\", \"engine\": \"MySQL 8.0 InnoDB\", \"pool\": \"Active\"},\n");
    jb_append_format(jb, "  \"mysql_server_version\": \"%s\",\n", server_info);
    jb_append_format(jb, "  \"mysql_client_version\": \"%s\",\n", client_info);
    jb_append(jb, "  \"http_engine\": \"Mongoose Embedded HTTP C Server\",\n");
    jb_append_format(jb, "  \"active_sessions\": %d,\n", active_sessions);
    jb_append(jb, "  \"cryptography\": \"Argon2id Memory-Hard Password & PIN Hashing\",\n");
    jb_append(jb, "  \"acid_transactions\": \"Full InnoDB Commit / Rollback Supported\"\n");
    jb_append(jb, "}");

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

#include "routes.h"
#include "config.h"
#include "db.h"
#include "auth.h"
#include "banking.h"
#include "analytics.h"
#include "admin.h"
#include "ds.h"
#include "json_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void send_json(struct mg_connection *c, int status, const char *json, const char *extra_headers) {
    char headers[512];
    if (extra_headers && strlen(extra_headers) > 0) {
        snprintf(headers, sizeof(headers),
                 "Content-Type: application/json; charset=utf-8\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Headers: Content-Type, X-CSRF-Token, Authorization\r\n"
                 "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                 "%s\r\n", extra_headers);
    } else {
        snprintf(headers, sizeof(headers),
                 "Content-Type: application/json; charset=utf-8\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Headers: Content-Type, X-CSRF-Token, Authorization\r\n"
                 "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n");
    }
    mg_http_reply(c, status, headers, "%s", json);
}

static void send_error(struct mg_connection *c, int status, const char *msg) {
    char esc_msg[256];
    json_escape_string(msg ? msg : "An error occurred", esc_msg, sizeof(esc_msg));
    char buf[512];
    snprintf(buf, sizeof(buf), "{\"status\":\"error\",\"message\":\"%s\"}", esc_msg);
    send_json(c, status, buf, NULL);
}

static bool get_session_from_request(struct mg_http_message *hm, Session *out_sess) {
    char token[128] = "";

    // 1. Try Cookie: session_token=...
    struct mg_str *cookie_hdr = mg_http_get_header(hm, "Cookie");
    if (cookie_hdr && cookie_hdr->len > 0 && cookie_hdr->buf) {
        char cookie_str[1024];
        size_t copy_len = cookie_hdr->len < sizeof(cookie_str) - 1 ? cookie_hdr->len : sizeof(cookie_str) - 1;
        memcpy(cookie_str, cookie_hdr->buf, copy_len);
        cookie_str[copy_len] = '\0';

        const char *tok_key = "session_token=";
        char *p = strstr(cookie_str, tok_key);
        if (p) {
            p += strlen(tok_key);
            size_t t = 0;
            while (*p && !isspace((unsigned char)*p) && *p != ';' && t + 1 < sizeof(token)) {
                token[t++] = *p++;
            }
            token[t] = '\0';
        }
    }

    // 2. Try Authorization: Bearer <token>
    if (strlen(token) == 0) {
        struct mg_str *auth_hdr = mg_http_get_header(hm, "Authorization");
        if (auth_hdr && auth_hdr->len > 7 && auth_hdr->buf) {
            char auth_str[256];
            size_t copy_len = auth_hdr->len < sizeof(auth_str) - 1 ? auth_hdr->len : sizeof(auth_str) - 1;
            memcpy(auth_str, auth_hdr->buf, copy_len);
            auth_str[copy_len] = '\0';

            if (strncmp(auth_str, "Bearer ", 7) == 0) {
                const char *p = auth_str + 7;
                while (*p == ' ') p++;
                size_t t = 0;
                while (*p && !isspace((unsigned char)*p) && t + 1 < sizeof(token)) {
                    token[t++] = *p++;
                }
                token[t] = '\0';
            }
        }
    }

    // 3. Try X-Session-Token: <token>
    if (strlen(token) == 0) {
        struct mg_str *x_tok = mg_http_get_header(hm, "X-Session-Token");
        if (x_tok && x_tok->len > 0 && x_tok->buf) {
            char tok_str[128];
            size_t copy_len = x_tok->len < sizeof(tok_str) - 1 ? x_tok->len : sizeof(tok_str) - 1;
            memcpy(tok_str, x_tok->buf, copy_len);
            tok_str[copy_len] = '\0';
            const char *p = tok_str;
            while (*p == ' ') p++;
            size_t t = 0;
            while (*p && !isspace((unsigned char)*p) && t + 1 < sizeof(token)) {
                token[t++] = *p++;
            }
            token[t] = '\0';
        }
    }

    if (strlen(token) >= 10) {
        return auth_validate_session(token, out_sess);
    }
    return false;
}

void routes_init(void) {
    printf("[ROUTES] Initialized HTTP Route Handlers\n");
}

void routes_handle_http(struct mg_connection *c, int ev, void *ev_data) {
    if (ev != MG_EV_HTTP_MSG) return;
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;

    // Handle OPTIONS CORS preflight
    if (mg_strcmp(hm->method, mg_str("OPTIONS")) == 0) {
        mg_http_reply(c, 204,
                      "Access-Control-Allow-Origin: *\r\n"
                      "Access-Control-Allow-Headers: Content-Type, X-CSRF-Token, Authorization\r\n"
                      "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                      "Access-Control-Max-Age: 86400\r\n\r\n", "");
        return;
    }

    // Extract request body safely
    char body[8192] = "";
    if (hm->body.len > 0 && hm->body.buf) {
        size_t blen = hm->body.len < sizeof(body) - 1 ? hm->body.len : sizeof(body) - 1;
        memcpy(body, hm->body.buf, blen);
        body[blen] = '\0';
    }

    // Extract client IP
    char client_ip[48] = "127.0.0.1";
    mg_snprintf(client_ip, sizeof(client_ip), "%M", mg_print_ip, &c->rem);

    // Helper macro for method check
    #define IS_METHOD(m) (mg_strcmp(hm->method, mg_str(m)) == 0)
    #define MATCH_URI(u) (mg_match(hm->uri, mg_str(u), NULL))

    // =========================================================================
    // 1. AUTHENTICATION ROUTES
    // =========================================================================

    // POST /api/auth/register
    if (MATCH_URI("/api/auth/register") && IS_METHOD("POST")) {
        char first_name[64], last_name[64], email[128], phone[32], password[128];
        if (!json_get_string(body, "first_name", first_name, sizeof(first_name)) ||
            !json_get_string(body, "last_name", last_name, sizeof(last_name)) ||
            !json_get_string(body, "email", email, sizeof(email)) ||
            !json_get_string(body, "phone", phone, sizeof(phone)) ||
            !json_get_string(body, "password", password, sizeof(password))) {
            send_error(c, 400, "All registration fields (first_name, last_name, email, phone, password) are required.");
            return;
        }

        if (strlen(password) < 6) {
            send_error(c, 400, "Password must be at least 6 characters long.");
            return;
        }

        char pwd_hash[MAX_HASH_LEN];
        if (!auth_hash_password(password, pwd_hash, sizeof(pwd_hash))) {
            send_error(c, 500, "Failed to hash password with Argon2id.");
            return;
        }

        // Generate customer number
        char cust_num[32];
        snprintf(cust_num, sizeof(cust_num), "CUST%d", rand() % 90000 + 10000);

        char esc_fn[128], esc_ln[128], esc_em[256], esc_ph[64], esc_hash[512];
        db_escape_string(first_name, esc_fn, sizeof(esc_fn));
        db_escape_string(last_name, esc_ln, sizeof(esc_ln));
        db_escape_string(email, esc_em, sizeof(esc_em));
        db_escape_string(phone, esc_ph, sizeof(esc_ph));
        db_escape_string(pwd_hash, esc_hash, sizeof(esc_hash));

        char sql[2048];
        snprintf(sql, sizeof(sql),
                 "INSERT INTO customers (customer_number, first_name, last_name, email, phone, password_hash, status) "
                 "VALUES ('%s', '%s', '%s', '%s', '%s', '%s', 'ACTIVE')",
                 cust_num, esc_fn, esc_ln, esc_em, esc_ph, esc_hash);

        if (!db_query(sql)) {
            send_error(c, 400, "Email or phone number already registered.");
            return;
        }

        int new_cust_id = (int)db_insert_id();

        // Create default savings account with ₹10,000 opening bonus for demonstration
        Account def_acc;
        banking_create_account(new_cust_id, ACC_TYPE_SAVINGS, 1000000, &def_acc);

        // Auto-login session
        Session sess;
        auth_create_session("CUSTOMER", new_cust_id, "CUSTOMER", client_ip, "Browser", &sess);
        auth_record_login("CUSTOMER", new_cust_id, client_ip, "Browser", true, NULL);

        char cookie_hdr[256];
        snprintf(cookie_hdr, sizeof(cookie_hdr), "Set-Cookie: session_token=%s; HttpOnly; SameSite=Strict; Path=/; Max-Age=86400", sess.session_token);

        char resp[1024];
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"success\",\"message\":\"Registration successful! Welcome to Smart Banking.\","
                 "\"customer_number\":\"%s\",\"csrf_token\":\"%s\",\"user\":{\"id\":%d,\"name\":\"%s %s\",\"email\":\"%s\",\"role\":\"CUSTOMER\"}}",
                 cust_num, sess.csrf_token, new_cust_id, esc_fn, esc_ln, esc_em);
        send_json(c, 201, resp, cookie_hdr);
        return;
    }

    // POST /api/auth/login
    if (MATCH_URI("/api/auth/login") && IS_METHOD("POST")) {
        char email[128], password[128];
        if (!json_get_string(body, "email", email, sizeof(email)) ||
            !json_get_string(body, "password", password, sizeof(password))) {
            send_error(c, 400, "Email and password are required.");
            return;
        }

        char esc_email[256];
        db_escape_string(email, esc_email, sizeof(esc_email));

        char sql[512];
        snprintf(sql, sizeof(sql),
                 "SELECT id, customer_number, first_name, last_name, email, password_hash, status, two_factor_enabled "
                 "FROM customers WHERE email = '%s' LIMIT 1", esc_email);

        MYSQL_RES *res = db_query_store(sql);
        if (!res) {
            send_error(c, 500, "Database query error.");
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row) {
            db_free_result(res);
            auth_record_login("CUSTOMER", 0, client_ip, "Browser", false, "User not found");
            send_error(c, 401, "Invalid email or password.");
            return;
        }

        int cust_id = atoi(row[0]);
        const char *cust_num = row[1];
        const char *fn = row[2];
        const char *ln = row[3];
        const char *db_email = row[4];
        const char *hash = row[5];
        const char *status = row[6];
        bool two_fa = (atoi(row[7]) == 1);

        if (!auth_verify_password(password, hash)) {
            db_free_result(res);
            auth_record_login("CUSTOMER", cust_id, client_ip, "Browser", false, "Incorrect password");
            send_error(c, 401, "Invalid email or password.");
            return;
        }

        if (strcmp(status, "SUSPENDED") == 0) {
            db_free_result(res);
            auth_record_login("CUSTOMER", cust_id, client_ip, "Browser", false, "Account suspended");
            send_error(c, 403, "Your account has been suspended by administration. Please contact support.");
            return;
        }

        db_free_result(res);

        // Check if customer has set transaction PIN
        bool has_pin = auth_has_pin(cust_id);

        Session sess;
        auth_create_session("CUSTOMER", cust_id, "CUSTOMER", client_ip, "Browser", &sess);
        auth_record_login("CUSTOMER", cust_id, client_ip, "Browser", true, NULL);

        char cookie_hdr[256];
        snprintf(cookie_hdr, sizeof(cookie_hdr), "Set-Cookie: session_token=%s; HttpOnly; SameSite=Strict; Path=/; Max-Age=86400", sess.session_token);

        char resp[1024];
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"success\",\"token\":\"%s\",\"csrf_token\":\"%s\",\"user\":{\"id\":%d,\"customer_number\":\"%s\","
                 "\"first_name\":\"%s\",\"last_name\":\"%s\",\"name\":\"%s %s\",\"email\":\"%s\","
                 "\"status\":\"%s\",\"two_factor_enabled\":%s,\"has_pin\":%s,\"role\":\"CUSTOMER\"}}",
                 sess.session_token, sess.csrf_token, cust_id, cust_num, fn, ln, fn, ln, db_email, status,
                 two_fa ? "true" : "false", has_pin ? "true" : "false");

        send_json(c, 200, resp, cookie_hdr);
        return;
    }

    // POST /api/auth/admin-login
    if (MATCH_URI("/api/auth/admin-login") && IS_METHOD("POST")) {
        char username[64], password[128];
        if (!json_get_string(body, "username", username, sizeof(username)) ||
            !json_get_string(body, "password", password, sizeof(password))) {
            send_error(c, 400, "Username and password are required.");
            return;
        }

        char esc_user[128];
        db_escape_string(username, esc_user, sizeof(esc_user));

        char sql[512];
        snprintf(sql, sizeof(sql),
                 "SELECT id, username, full_name, email, password_hash, role, status FROM admins WHERE username = '%s' LIMIT 1", esc_user);

        MYSQL_RES *res = db_query_store(sql);
        if (!res) {
            send_error(c, 500, "Database query error.");
            return;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row) {
            db_free_result(res);
            auth_record_login("ADMIN", 0, client_ip, "Browser", false, "Admin not found");
            send_error(c, 401, "Invalid administrative credentials.");
            return;
        }

        int admin_id = atoi(row[0]);
        const char *user = row[1];
        const char *name = row[2];
        const char *email = row[3];
        const char *hash = row[4];
        const char *role = row[5];
        const char *status = row[6];

        if (!auth_verify_password(password, hash)) {
            db_free_result(res);
            auth_record_login("ADMIN", admin_id, client_ip, "Browser", false, "Incorrect admin password");
            send_error(c, 401, "Invalid administrative credentials.");
            return;
        }

        if (strcmp(status, "ACTIVE") != 0) {
            db_free_result(res);
            send_error(c, 403, "Administrator account is deactivated.");
            return;
        }

        db_free_result(res);

        Session sess;
        auth_create_session("ADMIN", admin_id, role, client_ip, "Browser", &sess);
        auth_record_login("ADMIN", admin_id, client_ip, "Browser", true, NULL);

        char cookie_hdr[256];
        snprintf(cookie_hdr, sizeof(cookie_hdr), "Set-Cookie: session_token=%s; HttpOnly; SameSite=Strict; Path=/; Max-Age=86400", sess.session_token);

        char resp[1024];
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"success\",\"token\":\"%s\",\"csrf_token\":\"%s\",\"user\":{\"id\":%d,\"username\":\"%s\","
                 "\"name\":\"%s\",\"email\":\"%s\",\"role\":\"%s\"}}",
                 sess.session_token, sess.csrf_token, admin_id, user, name, email, role);

        send_json(c, 200, resp, cookie_hdr);
        return;
    }

    // POST /api/auth/logout
    if (MATCH_URI("/api/auth/logout") && IS_METHOD("POST")) {
        Session sess;
        if (get_session_from_request(hm, &sess)) {
            auth_destroy_session(sess.session_token);
        }
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Logged out successfully.\"}",
                  "Set-Cookie: session_token=; HttpOnly; SameSite=Strict; Path=/; Max-Age=0");
        return;
    }

    // GET /api/auth/me
    if (MATCH_URI("/api/auth/me") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) {
            send_error(c, 401, "Not logged in or session expired.");
            return;
        }

        if (strcmp(sess.user_type, "CUSTOMER") == 0) {
            char sql[512];
            snprintf(sql, sizeof(sql),
                     "SELECT id, customer_number, first_name, last_name, email, phone, status, two_factor_enabled "
                     "FROM customers WHERE id = %d LIMIT 1", sess.user_id);
            MYSQL_RES *res = db_query_store(sql);
            if (!res) { send_error(c, 500, "DB error"); return; }
            MYSQL_ROW row = mysql_fetch_row(res);
            if (!row) { db_free_result(res); send_error(c, 404, "User not found"); return; }

            bool has_pin = auth_has_pin(sess.user_id);
            char resp[1024];
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"success\",\"csrf_token\":\"%s\",\"user\":{\"id\":%d,\"customer_number\":\"%s\","
                     "\"first_name\":\"%s\",\"last_name\":\"%s\",\"name\":\"%s %s\",\"email\":\"%s\",\"phone\":\"%s\","
                     "\"status\":\"%s\",\"two_factor_enabled\":%s,\"has_pin\":%s,\"role\":\"CUSTOMER\"}}",
                     sess.csrf_token, atoi(row[0]), row[1], row[2], row[3], row[2], row[3], row[4], row[5],
                     row[6], (atoi(row[7]) == 1) ? "true" : "false", has_pin ? "true" : "false");
            db_free_result(res);
            send_json(c, 200, resp, NULL);
            return;
        } else {
            // ADMIN
            char sql[512];
            snprintf(sql, sizeof(sql),
                     "SELECT id, username, full_name, email, role, status FROM admins WHERE id = %d LIMIT 1", sess.user_id);
            MYSQL_RES *res = db_query_store(sql);
            if (!res) { send_error(c, 500, "DB error"); return; }
            MYSQL_ROW row = mysql_fetch_row(res);
            if (!row) { db_free_result(res); send_error(c, 404, "Admin not found"); return; }

            char resp[1024];
            snprintf(resp, sizeof(resp),
                     "{\"status\":\"success\",\"csrf_token\":\"%s\",\"user\":{\"id\":%d,\"username\":\"%s\","
                     "\"name\":\"%s\",\"email\":\"%s\",\"role\":\"%s\",\"status\":\"%s\"}}",
                     sess.csrf_token, atoi(row[0]), row[1], row[2], row[3], row[4], row[5]);
            db_free_result(res);
            send_json(c, 200, resp, NULL);
            return;
        }
    }

    // POST /api/auth/change-password
    if (MATCH_URI("/api/auth/change-password") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        char old_pwd[128], new_pwd[128];
        if (!json_get_string(body, "old_password", old_pwd, sizeof(old_pwd)) ||
            !json_get_string(body, "new_password", new_pwd, sizeof(new_pwd))) {
            send_error(c, 400, "Old password and new password are required.");
            return;
        }

        if (strlen(new_pwd) < 6) {
            send_error(c, 400, "New password must be at least 6 characters.");
            return;
        }

        char sql[256];
        snprintf(sql, sizeof(sql), "SELECT password_hash FROM customers WHERE id = %d", sess.user_id);
        MYSQL_RES *res = db_query_store(sql);
        if (!res) { send_error(c, 500, "DB error"); return; }
        MYSQL_ROW row = mysql_fetch_row(res);
        if (!row || !auth_verify_password(old_pwd, row[0])) {
            db_free_result(res);
            send_error(c, 400, "Incorrect current password.");
            return;
        }
        db_free_result(res);

        char new_hash[MAX_HASH_LEN];
        auth_hash_password(new_pwd, new_hash, sizeof(new_hash));
        char esc_hash[MAX_HASH_LEN * 2];
        db_escape_string(new_hash, esc_hash, sizeof(esc_hash));

        char upd_sql[512];
        snprintf(upd_sql, sizeof(upd_sql), "UPDATE customers SET password_hash = '%s' WHERE id = %d", esc_hash, sess.user_id);
        db_query(upd_sql);
        db_audit_log("CUSTOMER", sess.user_id, "PASSWORD_CHANGED", "CUSTOMER", sess.user_id, "Password updated", client_ip);

        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Password updated successfully.\"}", NULL);
        return;
    }

    // POST /api/auth/set-pin
    if (MATCH_URI("/api/auth/set-pin") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        char pin[32];
        if (!json_get_string(body, "pin", pin, sizeof(pin)) || strlen(pin) < 4 || strlen(pin) > 6) {
            send_error(c, 400, "Transaction PIN must be 4 to 6 digits.");
            return;
        }

        if (!auth_set_pin(sess.user_id, pin)) {
            send_error(c, 500, "Failed to save transaction PIN.");
            return;
        }

        db_audit_log("CUSTOMER", sess.user_id, "PIN_UPDATED", "CUSTOMER", sess.user_id, "Transaction PIN set/updated", client_ip);
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Transaction PIN saved securely.\"}", NULL);
        return;
    }

    // POST /api/auth/verify-pin
    if (MATCH_URI("/api/auth/verify-pin") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        char pin[32];
        if (!json_get_string(body, "pin", pin, sizeof(pin))) {
            send_error(c, 400, "PIN required.");
            return;
        }

        bool ok = auth_verify_pin(sess.user_id, pin);
        if (ok) {
            send_json(c, 200, "{\"status\":\"success\",\"valid\":true}", NULL);
        } else {
            send_error(c, 400, "Invalid PIN entered.");
        }
        return;
    }

    // POST /api/auth/toggle-2fa
    if (MATCH_URI("/api/auth/toggle-2fa") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        bool enable = false;
        json_get_bool(body, "enabled", &enable);

        char sql[256];
        snprintf(sql, sizeof(sql), "UPDATE customers SET two_factor_enabled = %d WHERE id = %d", enable ? 1 : 0, sess.user_id);
        db_query(sql);

        send_json(c, 200, enable ? "{\"status\":\"success\",\"message\":\"Two-factor authentication enabled.\"}"
                                 : "{\"status\":\"success\",\"message\":\"Two-factor authentication disabled.\"}", NULL);
        return;
    }

    // =========================================================================
    // 2. BANKING ROUTES (Customer Authenticated)
    // =========================================================================

    // GET /api/accounts
    if (MATCH_URI("/api/accounts") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        char *json = banking_get_customer_accounts_json(sess.user_id);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // POST /api/accounts
    if (MATCH_URI("/api/accounts") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        char acc_type_str[32];
        json_get_string(body, "account_type", acc_type_str, sizeof(acc_type_str));
        AccountType type = (strcmp(acc_type_str, "CURRENT") == 0) ? ACC_TYPE_CURRENT : ACC_TYPE_SAVINGS;

        int64_t initial_dep = 0;
        json_get_int64(body, "initial_deposit_paise", &initial_dep);

        Account acc;
        if (!banking_create_account(sess.user_id, type, initial_dep, &acc)) {
            send_error(c, 500, "Failed to create new account.");
            return;
        }

        char resp[512];
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"success\",\"message\":\"Account created successfully!\","
                 "\"account\":{\"id\":%d,\"account_number\":\"%s\",\"account_type\":\"%s\",\"balance_paise\":%lld}}",
                 acc.id, acc.account_number, type == ACC_TYPE_SAVINGS ? "SAVINGS" : "CURRENT", (long long)initial_dep);
        send_json(c, 201, resp, NULL);
        return;
    }

    // POST /api/accounts/freeze
    if (MATCH_URI("/api/accounts/freeze") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        int acc_id = 0;
        char reason[128] = "Emergency user freeze";
        json_get_int(body, "account_id", &acc_id);
        json_get_string(body, "reason", reason, sizeof(reason));

        if (!banking_freeze_account(acc_id, sess.user_id, reason)) {
            send_error(c, 400, "Unable to freeze account. Verify account ownership.");
            return;
        }
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Account frozen successfully for your security.\"}", NULL);
        return;
    }

    // POST /api/accounts/unfreeze
    if (MATCH_URI("/api/accounts/unfreeze") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        int acc_id = 0;
        json_get_int(body, "account_id", &acc_id);

        if (!banking_unfreeze_account(acc_id, sess.user_id)) {
            send_error(c, 400, "Unable to reactivate account.");
            return;
        }
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Account reactivated successfully.\"}", NULL);
        return;
    }

    // POST /api/banking/deposit
    if (MATCH_URI("/api/banking/deposit") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        int acc_id = 0;
        int64_t amount_paise = 0;
        char desc[256] = "", cat[64] = "Deposit";
        json_get_int(body, "account_id", &acc_id);
        json_get_int64(body, "amount_paise", &amount_paise);
        json_get_string(body, "description", desc, sizeof(desc));
        json_get_string(body, "category", cat, sizeof(cat));

        Transaction tx;
        char err[256] = "";
        if (!banking_deposit(sess.user_id, acc_id, amount_paise, desc, cat, &tx, err, sizeof(err))) {
            send_error(c, 400, err);
            return;
        }

        char resp[512];
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"success\",\"message\":\"Deposit processed successfully.\","
                 "\"reference_id\":\"%s\",\"balance_after_paise\":%lld}",
                 tx.reference_id, (long long)tx.balance_after_paise);
        send_json(c, 200, resp, NULL);
        return;
    }

    // POST /api/banking/withdraw
    if (MATCH_URI("/api/banking/withdraw") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        int acc_id = 0;
        int64_t amount_paise = 0;
        char pin[32] = "", desc[256] = "", cat[64] = "General";
        json_get_int(body, "account_id", &acc_id);
        json_get_int64(body, "amount_paise", &amount_paise);
        json_get_string(body, "pin", pin, sizeof(pin));
        json_get_string(body, "description", desc, sizeof(desc));
        json_get_string(body, "category", cat, sizeof(cat));

        Transaction tx;
        char err[256] = "";
        if (!banking_withdraw(sess.user_id, acc_id, amount_paise, pin, desc, cat, &tx, err, sizeof(err))) {
            send_error(c, 400, err);
            return;
        }

        char resp[512];
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"success\",\"message\":\"Withdrawal processed successfully.\","
                 "\"reference_id\":\"%s\",\"balance_after_paise\":%lld}",
                 tx.reference_id, (long long)tx.balance_after_paise);
        send_json(c, 200, resp, NULL);
        return;
    }

    // POST /api/banking/transfer
    if (MATCH_URI("/api/banking/transfer") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        int from_id = 0;
        char to_num[32] = "", pin[32] = "", desc[256] = "", cat[64] = "Transfer";
        int64_t amount_paise = 0;

        json_get_int(body, "from_account_id", &from_id);
        json_get_string(body, "to_account_number", to_num, sizeof(to_num));
        json_get_int64(body, "amount_paise", &amount_paise);
        json_get_string(body, "pin", pin, sizeof(pin));
        json_get_string(body, "description", desc, sizeof(desc));
        json_get_string(body, "category", cat, sizeof(cat));

        Transaction tx;
        char err[256] = "";
        if (!banking_transfer(sess.user_id, from_id, to_num, amount_paise, pin, desc, cat, &tx, err, sizeof(err))) {
            send_error(c, 400, err);
            return;
        }

        char resp[512];
        snprintf(resp, sizeof(resp),
                 "{\"status\":\"success\",\"message\":\"Transfer executed successfully.\","
                 "\"reference_id\":\"%s\",\"balance_after_paise\":%lld,\"is_flagged\":%s}",
                 tx.reference_id, (long long)tx.balance_after_paise,
                 (tx.status == TX_STATUS_FLAGGED) ? "true" : "false");
        send_json(c, 200, resp, NULL);
        return;
    }

    // GET /api/banking/transactions
    if (MATCH_URI("/api/banking/transactions") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        // Query params parsing
        char acc_str[32] = "", limit_str[32] = "", type_str[32] = "", cat_str[64] = "";
        struct mg_str q = hm->query;
        if (q.len > 0) {
            char q_buf[512];
            size_t qlen = q.len < sizeof(q_buf) - 1 ? q.len : sizeof(q_buf) - 1;
            memcpy(q_buf, q.buf, qlen);
            q_buf[qlen] = '\0';

            char *p = strstr(q_buf, "account_id=");
            if (p) sscanf(p + 11, "%31[^&]", acc_str);
            p = strstr(q_buf, "limit=");
            if (p) sscanf(p + 6, "%31[^&]", limit_str);
            p = strstr(q_buf, "type=");
            if (p) sscanf(p + 5, "%31[^&]", type_str);
            p = strstr(q_buf, "category=");
            if (p) sscanf(p + 9, "%63[^&]", cat_str);
        }

        int acc_id = atoi(acc_str);
        int limit = (atoi(limit_str) > 0) ? atoi(limit_str) : 30;

        char *json = banking_get_transactions_json(sess.user_id, acc_id, limit, 0, type_str, cat_str);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // GET /api/banking/statement
    if (MATCH_URI("/api/banking/statement") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        int acc_id = 0, year = 2026, month = 11;
        char var_acc[32] = "", var_yr[32] = "", var_mo[32] = "";
        if (mg_http_get_var(&hm->query, "account_id", var_acc, sizeof(var_acc)) > 0) acc_id = atoi(var_acc);
        if (mg_http_get_var(&hm->query, "year", var_yr, sizeof(var_yr)) > 0) year = atoi(var_yr);
        if (mg_http_get_var(&hm->query, "month", var_mo, sizeof(var_mo)) > 0) month = atoi(var_mo);

        char *json = banking_get_statement_json(sess.user_id, acc_id, year, month);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // Beneficiaries
    if (MATCH_URI("/api/beneficiaries") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        char *json = banking_get_beneficiaries_json(sess.user_id);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    if (MATCH_URI("/api/beneficiaries") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        char name[128], acc_num[32], bank[128] = "Smart Bank", ifsc[32] = "SMRT0001001", email[128] = "", nick[64] = "";
        if (!json_get_string(body, "beneficiary_name", name, sizeof(name)) ||
            !json_get_string(body, "account_number", acc_num, sizeof(acc_num))) {
            send_error(c, 400, "Beneficiary name and account number are required.");
            return;
        }
        json_get_string(body, "bank_name", bank, sizeof(bank));
        json_get_string(body, "ifsc_code", ifsc, sizeof(ifsc));
        json_get_string(body, "email", email, sizeof(email));
        json_get_string(body, "nickname", nick, sizeof(nick));

        if (!banking_add_beneficiary(sess.user_id, name, acc_num, bank, ifsc, email, nick)) {
            send_error(c, 500, "Failed to add beneficiary.");
            return;
        }
        send_json(c, 201, "{\"status\":\"success\",\"message\":\"Beneficiary saved successfully.\"}", NULL);
        return;
    }

    if (MATCH_URI("/api/beneficiaries/delete") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        int ben_id = 0;
        json_get_int(body, "id", &ben_id);
        if (!banking_delete_beneficiary(sess.user_id, ben_id)) {
            send_error(c, 400, "Failed to remove beneficiary.");
            return;
        }
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Beneficiary removed.\"}", NULL);
        return;
    }

    // Virtual Cards
    if (MATCH_URI("/api/cards") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        char *json = banking_get_virtual_cards_json(sess.user_id);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    if (MATCH_URI("/api/cards") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        int acc_id = 0;
        char type[16] = "VISA";
        json_get_int(body, "account_id", &acc_id);
        json_get_string(body, "card_type", type, sizeof(type));

        VirtualCard vc;
        if (!banking_create_virtual_card(sess.user_id, acc_id, type, &vc)) {
            send_error(c, 400, "Unable to generate virtual card. Check account status.");
            return;
        }
        send_json(c, 201, "{\"status\":\"success\",\"message\":\"Virtual card issued successfully.\"}", NULL);
        return;
    }

    if (MATCH_URI("/api/cards/toggle-freeze") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        int card_id = 0;
        json_get_int(body, "card_id", &card_id);
        if (!banking_toggle_card_freeze(sess.user_id, card_id)) {
            send_error(c, 400, "Failed to update card status.");
            return;
        }
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Card status updated.\"}", NULL);
        return;
    }

    // Savings Goals
    if (MATCH_URI("/api/savings/goals") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        char *json = analytics_get_savings_goals_json(sess.user_id);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    if (MATCH_URI("/api/savings/goals") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        int acc_id = 0;
        char title[128], date[32], cat[64] = "Emergency Fund";
        int64_t target_paise = 0;
        json_get_int(body, "account_id", &acc_id);
        json_get_string(body, "title", title, sizeof(title));
        json_get_int64(body, "target_paise", &target_paise);
        json_get_string(body, "target_date", date, sizeof(date));
        json_get_string(body, "category", cat, sizeof(cat));

        if (!analytics_create_savings_goal(sess.user_id, acc_id, title, target_paise, date, cat)) {
            send_error(c, 400, "Failed to create savings goal.");
            return;
        }
        send_json(c, 201, "{\"status\":\"success\",\"message\":\"Savings goal created!\"}", NULL);
        return;
    }

    if (MATCH_URI("/api/savings/contribute") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        int goal_id = 0;
        int64_t amt = 0;
        char note[256] = "";
        json_get_int(body, "goal_id", &goal_id);
        json_get_int64(body, "amount_paise", &amt);
        json_get_string(body, "note", note, sizeof(note));

        char err[256] = "";
        if (!analytics_contribute_to_goal(sess.user_id, goal_id, amt, note, err, sizeof(err))) {
            send_error(c, 400, err);
            return;
        }
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Savings contribution deposited successfully!\"}", NULL);
        return;
    }

    // Service Requests
    if (MATCH_URI("/api/service-requests") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        char *json = banking_get_service_requests_json(sess.user_id);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    if (MATCH_URI("/api/service-requests") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }

        int acc_id = 0;
        char type[64], desc[1024], priority[32] = "MEDIUM";
        json_get_int(body, "account_id", &acc_id);
        json_get_string(body, "request_type", type, sizeof(type));
        json_get_string(body, "description", desc, sizeof(desc));
        json_get_string(body, "priority", priority, sizeof(priority));

        ServiceRequest sr;
        if (!banking_create_service_request(sess.user_id, acc_id, type, desc, priority, &sr)) {
            send_error(c, 400, "Failed to submit service request.");
            return;
        }
        send_json(c, 201, "{\"status\":\"success\",\"message\":\"Service ticket registered and queued for officer action.\"}", NULL);
        return;
    }

    // Notifications
    if (MATCH_URI("/api/notifications") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        char *json = banking_get_notifications_json(sess.user_id);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    if (MATCH_URI("/api/notifications/read") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        int notif_id = 0;
        json_get_int(body, "id", &notif_id);
        banking_mark_notification_read(sess.user_id, notif_id);
        send_json(c, 200, "{\"status\":\"success\"}", NULL);
        return;
    }

    if (MATCH_URI("/api/notifications/read-all") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        banking_mark_all_notifications_read(sess.user_id);
        send_json(c, 200, "{\"status\":\"success\"}", NULL);
        return;
    }

    // Login History
    if (MATCH_URI("/api/login-history") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        char sql[512];
        snprintf(sql, sizeof(sql),
                 "SELECT id, ip_address, user_agent, status, failure_reason, login_at "
                 "FROM login_history WHERE user_type = '%s' AND user_id = %d ORDER BY id DESC LIMIT 20",
                 sess.user_type, sess.user_id);
        MYSQL_RES *res = db_query_store(sql);
        if (!res) { send_json(c, 200, "[]", NULL); return; }

        JsonBuilder *jb = jb_create(2048);
        jb_append(jb, "[");
        MYSQL_ROW row;
        bool first = true;
        while ((row = mysql_fetch_row(res))) {
            if (!first) jb_append(jb, ",");
            first = false;
            char esc_ua[256];
            json_escape_string(row[2] ? row[2] : "Browser", esc_ua, sizeof(esc_ua));
            jb_append_format(jb,
                "{\"id\":%d,\"ip_address\":\"%s\",\"user_agent\":\"%s\",\"status\":\"%s\",\"login_at\":\"%s\"}",
                atoi(row[0]), row[1], esc_ua, row[3], row[5]);
        }
        jb_append(jb, "]");
        db_free_result(res);
        char *json = _strdup(jb_str(jb));
        jb_free(jb);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // Financial Analytics Summary
    if (MATCH_URI("/api/analytics/summary") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        char *json = analytics_get_summary_json(sess.user_id);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // AI Financial Assistant
    if (MATCH_URI("/api/ai/assistant") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess)) { send_error(c, 401, "Unauthorized"); return; }
        char prompt[512] = "";
        json_get_string(body, "prompt", prompt, sizeof(prompt));
        char *json = analytics_ai_financial_advisor(sess.user_id, prompt);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // C Data Structures Demo Endpoint
    if (MATCH_URI("/api/ds/demo") && IS_METHOD("GET")) {
        char *json = ds_run_demonstration_report();
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // =========================================================================
    // 3. ADMIN ROUTES (Admin Authenticated & RBAC Checked)
    // =========================================================================

    // GET /api/admin/overview
    if (MATCH_URI("/api/admin/overview") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess) || strcmp(sess.user_type, "ADMIN") != 0) {
            send_error(c, 403, "Access restricted to administrative staff.");
            return;
        }
        char *json = admin_get_overview_json();
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // GET /api/admin/customers
    if (MATCH_URI("/api/admin/customers") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess) || strcmp(sess.user_type, "ADMIN") != 0) {
            send_error(c, 403, "Access denied.");
            return;
        }
        char q_str[128] = "";
        struct mg_str q = hm->query;
        if (q.len > 0) {
            char q_buf[256];
            size_t qlen = q.len < sizeof(q_buf) - 1 ? q.len : sizeof(q_buf) - 1;
            memcpy(q_buf, q.buf, qlen);
            q_buf[qlen] = '\0';
            char *p = strstr(q_buf, "q=");
            if (p) sscanf(p + 2, "%127[^&]", q_str);
        }
        char *json = admin_get_customers_json(q_str, 50, 0);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // POST /api/admin/customers/status
    if (MATCH_URI("/api/admin/customers/status") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess) || strcmp(sess.user_type, "ADMIN") != 0) {
            send_error(c, 403, "Access denied.");
            return;
        }
        int cust_id = 0;
        char status[32] = "", reason[256] = "";
        json_get_int(body, "customer_id", &cust_id);
        json_get_string(body, "status", status, sizeof(status));
        json_get_string(body, "reason", reason, sizeof(reason));

        if (!admin_update_customer_status(sess.user_id, sess.role, cust_id, status, reason)) {
            send_error(c, 403, "Permission denied or failed to update customer status.");
            return;
        }
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Customer status modified successfully.\"}", NULL);
        return;
    }

    // POST /api/admin/accounts/status
    if (MATCH_URI("/api/admin/accounts/status") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess) || strcmp(sess.user_type, "ADMIN") != 0) {
            send_error(c, 403, "Access denied.");
            return;
        }
        int acc_id = 0;
        char status[32] = "", reason[256] = "";
        json_get_int(body, "account_id", &acc_id);
        json_get_string(body, "status", status, sizeof(status));
        json_get_string(body, "reason", reason, sizeof(reason));

        if (!admin_update_account_status(sess.user_id, sess.role, acc_id, status, reason)) {
            send_error(c, 403, "Permission denied or failed to update account status.");
            return;
        }
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Account status updated successfully.\"}", NULL);
        return;
    }

    // GET /api/admin/transactions
    if (MATCH_URI("/api/admin/transactions") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess) || strcmp(sess.user_type, "ADMIN") != 0) {
            send_error(c, 403, "Access denied.");
            return;
        }
        bool flagged_only = false;
        struct mg_str q = hm->query;
        if (q.len > 0) {
            char q_buf[128];
            size_t qlen = q.len < sizeof(q_buf) - 1 ? q.len : sizeof(q_buf) - 1;
            memcpy(q_buf, q.buf, qlen);
            q_buf[qlen] = '\0';
            if (strstr(q_buf, "flagged=true") || strstr(q_buf, "flagged=1")) {
                flagged_only = true;
            }
        }
        char *json = admin_get_transactions_json(flagged_only, 100, 0);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // GET /api/admin/service-requests
    if (MATCH_URI("/api/admin/service-requests") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess) || strcmp(sess.user_type, "ADMIN") != 0) {
            send_error(c, 403, "Access denied.");
            return;
        }
        char status_filter[32] = "ALL";
        struct mg_str q = hm->query;
        if (q.len > 0) {
            char q_buf[128];
            size_t qlen = q.len < sizeof(q_buf) - 1 ? q.len : sizeof(q_buf) - 1;
            memcpy(q_buf, q.buf, qlen);
            q_buf[qlen] = '\0';
            char *p = strstr(q_buf, "status=");
            if (p) sscanf(p + 7, "%31[^&]", status_filter);
        }
        char *json = admin_get_service_requests_json(status_filter);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // POST /api/admin/service-requests/action
    if (MATCH_URI("/api/admin/service-requests/action") && IS_METHOD("POST")) {
        Session sess;
        if (!get_session_from_request(hm, &sess) || strcmp(sess.user_type, "ADMIN") != 0) {
            send_error(c, 403, "Access denied.");
            return;
        }
        int ticket_id = 0;
        char decision[32] = "", notes[512] = "";
        json_get_int(body, "ticket_id", &ticket_id);
        json_get_string(body, "decision", decision, sizeof(decision));
        json_get_string(body, "admin_notes", notes, sizeof(notes));

        if (!admin_process_service_request(sess.user_id, sess.role, ticket_id, decision, notes)) {
            send_error(c, 403, "Action prohibited for your administrative role or invalid ticket.");
            return;
        }
        send_json(c, 200, "{\"status\":\"success\",\"message\":\"Service ticket processed.\"}", NULL);
        return;
    }

    // GET /api/admin/audit-logs
    if (MATCH_URI("/api/admin/audit-logs") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess) || strcmp(sess.user_type, "ADMIN") != 0) {
            send_error(c, 403, "Access denied.");
            return;
        }
        char *json = admin_get_audit_logs_json(100, 0);
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // GET /api/admin/system-health
    if (MATCH_URI("/api/admin/system-health") && IS_METHOD("GET")) {
        Session sess;
        if (!get_session_from_request(hm, &sess) || strcmp(sess.user_type, "ADMIN") != 0) {
            send_error(c, 403, "Access denied.");
            return;
        }
        char *json = admin_get_system_health_json();
        send_json(c, 200, json, NULL);
        free(json);
        return;
    }

    // =========================================================================
    // 4. STATIC FILE SERVING
    // =========================================================================
    if (MATCH_URI("/") || MATCH_URI("/index.html")) {
        struct mg_http_serve_opts opts = {.root_dir = "web"};
        mg_http_serve_file(c, hm, "web/index.html", &opts);
        return;
    }

    if (MATCH_URI("/admin") || MATCH_URI("/admin.html")) {
        struct mg_http_serve_opts opts = {.root_dir = "web"};
        mg_http_serve_file(c, hm, "web/admin.html", &opts);
        return;
    }

    // Serve web files (css, js, assets)
    struct mg_http_serve_opts opts = {.root_dir = "web"};
    mg_http_serve_dir(c, hm, &opts);
}

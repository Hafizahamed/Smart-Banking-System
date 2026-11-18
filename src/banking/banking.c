#include "banking.h"
#include "db.h"
#include "auth.h"
#include "json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void gen_ref_id(const char *prefix, char *out, size_t len) {
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_s(&tm_info, &now);
    char date_part[32];
    strftime(date_part, sizeof(date_part), "%Y%m%d%H%M%S", &tm_info);
    int r = rand() % 9000 + 1000;
    snprintf(out, len, "%s%s%d", prefix, date_part, r);
}

static void add_customer_notification(int customer_id, const char *title, const char *message, const char *type) {
    char esc_title[256], esc_msg[1024];
    db_escape_string(title, esc_title, sizeof(esc_title));
    db_escape_string(message, esc_msg, sizeof(esc_msg));
    char sql[2048];
    snprintf(sql, sizeof(sql),
             "INSERT INTO notifications (customer_id, title, message, type) VALUES (%d, '%s', '%s', '%s')",
             customer_id, esc_title, esc_msg, type ? type : "SYSTEM");
    db_query(sql);
}

bool banking_create_account(int customer_id, AccountType type, int64_t initial_deposit_paise, Account *out_acc) {
    if (customer_id <= 0) return false;

    // Generate unique account number: 1001 + 8 digits
    char acc_num[24];
    for (int retry = 0; retry < 5; retry++) {
        int r1 = rand() % 9000 + 1000;
        int r2 = rand() % 9000 + 1000;
        snprintf(acc_num, sizeof(acc_num), "1001%d%d", r1, r2);

        char check_sql[128];
        snprintf(check_sql, sizeof(check_sql), "SELECT 1 FROM accounts WHERE account_number = '%s'", acc_num);
        MYSQL_RES *res = db_query_store(check_sql);
        if (res) {
            bool exists = (mysql_num_rows(res) > 0);
            db_free_result(res);
            if (!exists) break;
        }
    }

    const char *type_str = (type == ACC_TYPE_CURRENT) ? "CURRENT" : "SAVINGS";
    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO accounts (customer_id, account_number, account_type, balance_paise, currency, status, branch_code) "
             "VALUES (%d, '%s', '%s', %lld, 'INR', 'ACTIVE', 'SMRT0001001')",
             customer_id, acc_num, type_str, (long long)initial_deposit_paise);

    if (!db_query(sql)) return false;

    int new_id = (int)db_insert_id();
    if (out_acc) {
        out_acc->id = new_id;
        out_acc->customer_id = customer_id;
        strncpy(out_acc->account_number, acc_num, sizeof(out_acc->account_number) - 1);
        out_acc->account_type = type;
        out_acc->balance_paise = initial_deposit_paise;
        strcpy(out_acc->currency, "INR");
        out_acc->status = STATUS_ACTIVE;
        strcpy(out_acc->branch_code, "SMRT0001001");
    }

    // If initial deposit, log transaction
    if (initial_deposit_paise > 0) {
        char ref[64];
        gen_ref_id("TXN", ref, sizeof(ref));
        char tx_sql[512];
        snprintf(tx_sql, sizeof(tx_sql),
                 "INSERT INTO transactions (reference_id, to_account_id, transaction_type, amount_paise, balance_after_paise, category, description, status) "
                 "VALUES ('%s', %d, 'DEPOSIT', %lld, %lld, 'Deposit', 'Initial Account Opening Deposit', 'SUCCESS')",
                 ref, new_id, (long long)initial_deposit_paise, (long long)initial_deposit_paise);
        db_query(tx_sql);
    }

    add_customer_notification(customer_id, "New Account Created", "Your new bank account has been activated successfully.", "SYSTEM");
    db_audit_log("CUSTOMER", customer_id, "ACCOUNT_CREATED", "ACCOUNT", new_id, acc_num, "127.0.0.1");
    return true;
}

char* banking_get_customer_accounts_json(int customer_id) {
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, account_number, account_type, balance_paise, currency, status, branch_code, created_at "
             "FROM accounts WHERE customer_id = %d ORDER BY id ASC", customer_id);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("[]");

    JsonBuilder *jb = jb_create(2048);
    jb_append(jb, "[");
    MYSQL_ROW row;
    bool first = true;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;

        int acc_id = atoi(row[0]);
        const char *acc_num = row[1];
        const char *acc_type = row[2];
        int64_t balance = _strtoi64(row[3], NULL, 10);
        const char *currency = row[4];
        const char *status = row[5];
        const char *branch = row[6];
        const char *created = row[7];

        char inr_fmt[64];
        format_inr(balance, inr_fmt, sizeof(inr_fmt));

        jb_append_format(jb,
            "{\"id\":%d,\"account_number\":\"%s\",\"account_type\":\"%s\",\"balance_paise\":%lld,"
            "\"balance_formatted\":\"%s\",\"currency\":\"%s\",\"status\":\"%s\",\"branch_code\":\"%s\",\"created_at\":\"%s\"}",
            acc_id, acc_num, acc_type, (long long)balance, inr_fmt, currency, status, branch, created);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

bool banking_get_account_by_id(int account_id, int customer_id, Account *out_acc) {
    if (account_id <= 0 || customer_id <= 0) return false;
    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT id, customer_id, account_number, account_type, balance_paise, currency, status, branch_code, created_at "
             "FROM accounts WHERE id = %d AND customer_id = %d LIMIT 1", account_id, customer_id);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return false;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        db_free_result(res);
        return false;
    }

    if (out_acc) {
        out_acc->id = atoi(row[0]);
        out_acc->customer_id = atoi(row[1]);
        strncpy(out_acc->account_number, row[2], sizeof(out_acc->account_number) - 1);
        out_acc->account_type = (strcmp(row[3], "CURRENT") == 0) ? ACC_TYPE_CURRENT : ACC_TYPE_SAVINGS;
        out_acc->balance_paise = _strtoi64(row[4], NULL, 10);
        strncpy(out_acc->currency, row[5], sizeof(out_acc->currency) - 1);
        out_acc->status = (strcmp(row[6], "FROZEN") == 0) ? STATUS_FROZEN :
                          (strcmp(row[6], "SUSPENDED") == 0) ? STATUS_SUSPENDED : STATUS_ACTIVE;
        strncpy(out_acc->branch_code, row[7], sizeof(out_acc->branch_code) - 1);
        strncpy(out_acc->created_at, row[8], sizeof(out_acc->created_at) - 1);
    }
    db_free_result(res);
    return true;
}

bool banking_freeze_account(int account_id, int customer_id, const char *reason) {
    Account acc;
    if (!banking_get_account_by_id(account_id, customer_id, &acc)) return false;

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE accounts SET status = 'FROZEN' WHERE id = %d AND customer_id = %d", account_id, customer_id);
    if (!db_query(sql)) return false;

    char msg[256];
    snprintf(msg, sizeof(msg), "Your account %s has been frozen. Reason: %s", acc.account_number, reason ? reason : "Emergency freeze requested.");
    add_customer_notification(customer_id, "Account Frozen", msg, "SECURITY");
    db_audit_log("CUSTOMER", customer_id, "ACCOUNT_FROZEN", "ACCOUNT", account_id, reason ? reason : "User request", "127.0.0.1");
    return true;
}

bool banking_unfreeze_account(int account_id, int customer_id) {
    Account acc;
    if (!banking_get_account_by_id(account_id, customer_id, &acc)) return false;

    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE accounts SET status = 'ACTIVE' WHERE id = %d AND customer_id = %d", account_id, customer_id);
    if (!db_query(sql)) return false;

    char msg[256];
    snprintf(msg, sizeof(msg), "Your account %s has been reactivated successfully.", acc.account_number);
    add_customer_notification(customer_id, "Account Reactivated", msg, "SECURITY");
    db_audit_log("CUSTOMER", customer_id, "ACCOUNT_UNFROZEN", "ACCOUNT", account_id, "User self unfreeze", "127.0.0.1");
    return true;
}

bool banking_deposit(int customer_id, int account_id, int64_t amount_paise, const char *description,
                     const char *category, Transaction *out_tx, char *err_msg, size_t err_len) {
    if (amount_paise <= 0) {
        snprintf(err_msg, err_len, "Deposit amount must be greater than zero.");
        return false;
    }

    Account acc;
    if (!banking_get_account_by_id(account_id, customer_id, &acc)) {
        snprintf(err_msg, err_len, "Invalid account or access unauthorized.");
        return false;
    }

    if (acc.status != STATUS_ACTIVE) {
        snprintf(err_msg, err_len, "Account is not active (currently %s).",
                 acc.status == STATUS_FROZEN ? "FROZEN" : "SUSPENDED");
        return false;
    }

    if (!db_begin()) {
        snprintf(err_msg, err_len, "Database transaction error.");
        return false;
    }

    int64_t new_balance = acc.balance_paise + amount_paise;
    char upd_sql[256];
    snprintf(upd_sql, sizeof(upd_sql),
             "UPDATE accounts SET balance_paise = %lld WHERE id = %d AND customer_id = %d",
             (long long)new_balance, account_id, customer_id);

    if (!db_query(upd_sql)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to update account balance.");
        return false;
    }

    char ref[64];
    gen_ref_id("TXN", ref, sizeof(ref));

    char esc_desc[256], esc_cat[64];
    db_escape_string(description && strlen(description) > 0 ? description : "Funds Deposit", esc_desc, sizeof(esc_desc));
    db_escape_string(category && strlen(category) > 0 ? category : "Deposit", esc_cat, sizeof(esc_cat));

    char tx_sql[1024];
    snprintf(tx_sql, sizeof(tx_sql),
             "INSERT INTO transactions (reference_id, to_account_id, transaction_type, amount_paise, balance_after_paise, category, description, status) "
             "VALUES ('%s', %d, 'DEPOSIT', %lld, %lld, '%s', '%s', 'SUCCESS')",
             ref, account_id, (long long)amount_paise, (long long)new_balance, esc_cat, esc_desc);

    if (!db_query(tx_sql)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to record transaction log.");
        return false;
    }

    if (!db_commit()) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to commit deposit transaction.");
        return false;
    }

    if (out_tx) {
        out_tx->id = (int)db_insert_id();
        strncpy(out_tx->reference_id, ref, sizeof(out_tx->reference_id) - 1);
        out_tx->from_account_id = 0;
        out_tx->to_account_id = account_id;
        out_tx->transaction_type = TX_DEPOSIT;
        out_tx->amount_paise = amount_paise;
        out_tx->balance_after_paise = new_balance;
        strncpy(out_tx->category, esc_cat, sizeof(out_tx->category) - 1);
        strncpy(out_tx->description, esc_desc, sizeof(out_tx->description) - 1);
        out_tx->status = TX_STATUS_SUCCESS;
    }

    char inr_fmt[64];
    format_inr(amount_paise, inr_fmt, sizeof(inr_fmt));
    char notif_msg[256];
    snprintf(notif_msg, sizeof(notif_msg), "%s has been credited to your account %s.", inr_fmt, acc.account_number);
    add_customer_notification(customer_id, "Deposit Successful", notif_msg, "TRANSACTION");

    db_audit_log("CUSTOMER", customer_id, "DEPOSIT_COMPLETED", "ACCOUNT", account_id, ref, "127.0.0.1");
    return true;
}

bool banking_withdraw(int customer_id, int account_id, int64_t amount_paise, const char *pin,
                      const char *description, const char *category, Transaction *out_tx, char *err_msg, size_t err_len) {
    if (amount_paise <= 0) {
        snprintf(err_msg, err_len, "Withdrawal amount must be greater than zero.");
        return false;
    }

    // Verify PIN if customer has one configured
    if (auth_has_pin(customer_id)) {
        if (!pin || strlen(pin) == 0 || !auth_verify_pin(customer_id, pin)) {
            snprintf(err_msg, err_len, "Invalid transaction PIN entered.");
            return false;
        }
    }

    Account acc;
    if (!banking_get_account_by_id(account_id, customer_id, &acc)) {
        snprintf(err_msg, err_len, "Invalid account or access unauthorized.");
        return false;
    }

    if (acc.status != STATUS_ACTIVE) {
        snprintf(err_msg, err_len, "Account is not active (currently %s).",
                 acc.status == STATUS_FROZEN ? "FROZEN" : "SUSPENDED");
        return false;
    }

    if (acc.balance_paise < amount_paise) {
        char bal_fmt[64], req_fmt[64];
        format_inr(acc.balance_paise, bal_fmt, sizeof(bal_fmt));
        format_inr(amount_paise, req_fmt, sizeof(req_fmt));
        snprintf(err_msg, err_len, "Insufficient balance. Available: %s, Requested: %s", bal_fmt, req_fmt);
        return false;
    }

    if (!db_begin()) {
        snprintf(err_msg, err_len, "Database transaction error.");
        return false;
    }

    int64_t new_balance = acc.balance_paise - amount_paise;
    char upd_sql[256];
    snprintf(upd_sql, sizeof(upd_sql),
             "UPDATE accounts SET balance_paise = %lld WHERE id = %d AND customer_id = %d",
             (long long)new_balance, account_id, customer_id);

    if (!db_query(upd_sql)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to update account balance.");
        return false;
    }

    char ref[64];
    gen_ref_id("TXN", ref, sizeof(ref));

    char esc_desc[256], esc_cat[64];
    db_escape_string(description && strlen(description) > 0 ? description : "Cash Withdrawal", esc_desc, sizeof(esc_desc));
    db_escape_string(category && strlen(category) > 0 ? category : "General", esc_cat, sizeof(esc_cat));

    char tx_sql[1024];
    snprintf(tx_sql, sizeof(tx_sql),
             "INSERT INTO transactions (reference_id, from_account_id, transaction_type, amount_paise, balance_after_paise, category, description, status) "
             "VALUES ('%s', %d, 'WITHDRAWAL', %lld, %lld, '%s', '%s', 'SUCCESS')",
             ref, account_id, (long long)amount_paise, (long long)new_balance, esc_cat, esc_desc);

    if (!db_query(tx_sql)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to record transaction log.");
        return false;
    }

    if (!db_commit()) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to commit withdrawal transaction.");
        return false;
    }

    if (out_tx) {
        out_tx->id = (int)db_insert_id();
        strncpy(out_tx->reference_id, ref, sizeof(out_tx->reference_id) - 1);
        out_tx->from_account_id = account_id;
        out_tx->to_account_id = 0;
        out_tx->transaction_type = TX_WITHDRAWAL;
        out_tx->amount_paise = amount_paise;
        out_tx->balance_after_paise = new_balance;
        strncpy(out_tx->category, esc_cat, sizeof(out_tx->category) - 1);
        strncpy(out_tx->description, esc_desc, sizeof(out_tx->description) - 1);
        out_tx->status = TX_STATUS_SUCCESS;
    }

    char inr_fmt[64];
    format_inr(amount_paise, inr_fmt, sizeof(inr_fmt));
    char notif_msg[256];
    snprintf(notif_msg, sizeof(notif_msg), "%s was debited from your account %s.", inr_fmt, acc.account_number);
    add_customer_notification(customer_id, "Withdrawal Alert", notif_msg, "TRANSACTION");

    db_audit_log("CUSTOMER", customer_id, "WITHDRAWAL_COMPLETED", "ACCOUNT", account_id, ref, "127.0.0.1");
    return true;
}

bool banking_transfer(int customer_id, int from_account_id, const char *to_account_number,
                      int64_t amount_paise, const char *pin, const char *description,
                      const char *category, Transaction *out_tx, char *err_msg, size_t err_len) {
    if (amount_paise <= 0) {
        snprintf(err_msg, err_len, "Transfer amount must be greater than zero.");
        return false;
    }
    if (!to_account_number || strlen(to_account_number) < 6) {
        snprintf(err_msg, err_len, "Valid destination account number is required.");
        return false;
    }

    // Verify PIN if customer has one configured
    if (auth_has_pin(customer_id)) {
        if (!pin || strlen(pin) == 0 || !auth_verify_pin(customer_id, pin)) {
            snprintf(err_msg, err_len, "Invalid transaction PIN entered.");
            return false;
        }
    }

    // Sender account validation
    Account sender_acc;
    if (!banking_get_account_by_id(from_account_id, customer_id, &sender_acc)) {
        snprintf(err_msg, err_len, "Source account invalid or unauthorized.");
        return false;
    }

    if (sender_acc.status != STATUS_ACTIVE) {
        snprintf(err_msg, err_len, "Source account is not active (currently %s).",
                 sender_acc.status == STATUS_FROZEN ? "FROZEN" : "SUSPENDED");
        return false;
    }

    if (strcmp(sender_acc.account_number, to_account_number) == 0) {
        snprintf(err_msg, err_len, "Cannot transfer funds to the identical account.");
        return false;
    }

    if (sender_acc.balance_paise < amount_paise) {
        char bal_fmt[64], req_fmt[64];
        format_inr(sender_acc.balance_paise, bal_fmt, sizeof(bal_fmt));
        format_inr(amount_paise, req_fmt, sizeof(req_fmt));
        snprintf(err_msg, err_len, "Insufficient funds. Available: %s, Required: %s", bal_fmt, req_fmt);
        return false;
    }

    // Recipient account lookup
    char esc_to_acc[32];
    db_escape_string(to_account_number, esc_to_acc, sizeof(esc_to_acc));
    char rcpt_sql[256];
    snprintf(rcpt_sql, sizeof(rcpt_sql),
             "SELECT id, customer_id, balance_paise, status FROM accounts WHERE account_number = '%s' LIMIT 1", esc_to_acc);

    MYSQL_RES *res = db_query_store(rcpt_sql);
    if (!res) {
        snprintf(err_msg, err_len, "Database error during recipient validation.");
        return false;
    }
    MYSQL_ROW rcpt_row = mysql_fetch_row(res);
    if (!rcpt_row) {
        db_free_result(res);
        snprintf(err_msg, err_len, "Destination account '%s' not found.", to_account_number);
        return false;
    }

    int rcpt_acc_id = atoi(rcpt_row[0]);
    int rcpt_cust_id = atoi(rcpt_row[1]);
    int64_t rcpt_balance = _strtoi64(rcpt_row[2], NULL, 10);
    const char *rcpt_status = rcpt_row[3];

    if (strcmp(rcpt_status, "ACTIVE") != 0) {
        db_free_result(res);
        snprintf(err_msg, err_len, "Destination account is currently %s and cannot accept incoming transfers.", rcpt_status);
        return false;
    }
    db_free_result(res);

    // Rule-Based Suspicious Transaction Detection
    // Flag if amount > ₹1,00,000 (10,000,000 paise)
    bool is_flagged = false;
    char flag_reason[256] = "";
    if (amount_paise > 10000000) {
        is_flagged = true;
        snprintf(flag_reason, sizeof(flag_reason), "Rule #FR-101: High value transfer exceeding ₹1,00,000 threshold.");
    }

    // Atomic Database Transaction
    if (!db_begin()) {
        snprintf(err_msg, err_len, "Failed to initiate ACID database transaction.");
        return false;
    }

    int64_t sender_new_balance = sender_acc.balance_paise - amount_paise;
    int64_t rcpt_new_balance = rcpt_balance + amount_paise;

    // 1. Debit sender
    char debit_sql[256];
    snprintf(debit_sql, sizeof(debit_sql),
             "UPDATE accounts SET balance_paise = %lld WHERE id = %d AND customer_id = %d",
             (long long)sender_new_balance, from_account_id, customer_id);
    if (!db_query(debit_sql)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to debit sender account.");
        return false;
    }

    // 2. Credit recipient
    char credit_sql[256];
    snprintf(credit_sql, sizeof(credit_sql),
             "UPDATE accounts SET balance_paise = %lld WHERE id = %d",
             (long long)rcpt_new_balance, rcpt_acc_id);
    if (!db_query(credit_sql)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to credit recipient account.");
        return false;
    }

    // 3. Insert transaction
    char ref[64];
    gen_ref_id("TXN", ref, sizeof(ref));
    char esc_desc[256], esc_cat[64], esc_flag[256];
    db_escape_string(description && strlen(description) > 0 ? description : "Fund Transfer", esc_desc, sizeof(esc_desc));
    db_escape_string(category && strlen(category) > 0 ? category : "Transfer", esc_cat, sizeof(esc_cat));
    db_escape_string(flag_reason, esc_flag, sizeof(esc_flag));

    char tx_sql[1024];
    snprintf(tx_sql, sizeof(tx_sql),
             "INSERT INTO transactions (reference_id, from_account_id, to_account_id, transaction_type, amount_paise, balance_after_paise, category, description, status, flag_reason) "
             "VALUES ('%s', %d, %d, 'TRANSFER', %lld, %lld, '%s', '%s', '%s', %s%s%s)",
             ref, from_account_id, rcpt_acc_id, (long long)amount_paise, (long long)sender_new_balance,
             esc_cat, esc_desc, is_flagged ? "FLAGGED" : "SUCCESS",
             is_flagged ? "'" : "", is_flagged ? esc_flag : "NULL", is_flagged ? "'" : "");

    if (!db_query(tx_sql)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to log transfer transaction.");
        return false;
    }

    // 4. Commit
    if (!db_commit()) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to commit transfer transaction.");
        return false;
    }

    if (out_tx) {
        out_tx->id = (int)db_insert_id();
        strncpy(out_tx->reference_id, ref, sizeof(out_tx->reference_id) - 1);
        out_tx->from_account_id = from_account_id;
        out_tx->to_account_id = rcpt_acc_id;
        out_tx->transaction_type = TX_TRANSFER;
        out_tx->amount_paise = amount_paise;
        out_tx->balance_after_paise = sender_new_balance;
        strncpy(out_tx->category, esc_cat, sizeof(out_tx->category) - 1);
        strncpy(out_tx->description, esc_desc, sizeof(out_tx->description) - 1);
        out_tx->status = is_flagged ? TX_STATUS_FLAGGED : TX_STATUS_SUCCESS;
    }

    char amt_fmt[64];
    format_inr(amount_paise, amt_fmt, sizeof(amt_fmt));

    // Notifications
    char sender_msg[256];
    snprintf(sender_msg, sizeof(sender_msg), "%s transferred to account %s. Ref: %s", amt_fmt, to_account_number, ref);
    add_customer_notification(customer_id, "Transfer Successful", sender_msg, "TRANSACTION");

    char rcpt_msg[256];
    snprintf(rcpt_msg, sizeof(rcpt_msg), "%s received from account %s. Ref: %s", amt_fmt, sender_acc.account_number, ref);
    add_customer_notification(rcpt_cust_id, "Funds Received", rcpt_msg, "TRANSACTION");

    if (is_flagged) {
        char flag_audit[512];
        snprintf(flag_audit, sizeof(flag_audit), "FLAGGED TRANSFER: %s, %s", ref, flag_reason);
        db_audit_log("SYSTEM", customer_id, "TRANSACTION_FLAGGED", "TRANSACTION", out_tx ? out_tx->id : 0, flag_audit, "127.0.0.1");
    }

    db_audit_log("CUSTOMER", customer_id, "TRANSFER_COMPLETED", "TRANSACTION", out_tx ? out_tx->id : 0, ref, "127.0.0.1");
    return true;
}

char* banking_get_transactions_json(int customer_id, int account_id, int limit, int offset,
                                    const char *type_filter, const char *category_filter) {
    if (limit <= 0 || limit > 100) limit = 30;
    if (offset < 0) offset = 0;

    char where_clause[1024];
    if (account_id > 0) {
        snprintf(where_clause, sizeof(where_clause),
                 "WHERE (t.from_account_id = %d OR t.to_account_id = %d) ", account_id, account_id);
    } else {
        snprintf(where_clause, sizeof(where_clause),
                 "WHERE (fa.customer_id = %d OR ta.customer_id = %d) ", customer_id, customer_id);
    }

    if (type_filter && strlen(type_filter) > 0 && strcmp(type_filter, "ALL") != 0) {
        char esc_type[32];
        db_escape_string(type_filter, esc_type, sizeof(esc_type));
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "AND t.transaction_type = '%s' ", esc_type);
        strcat_s(where_clause, sizeof(where_clause), tmp);
    }

    if (category_filter && strlen(category_filter) > 0 && strcmp(category_filter, "ALL") != 0) {
        char esc_cat[64];
        db_escape_string(category_filter, esc_cat, sizeof(esc_cat));
        char tmp[128];
        snprintf(tmp, sizeof(tmp), "AND t.category = '%s' ", esc_cat);
        strcat_s(where_clause, sizeof(where_clause), tmp);
    }

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "SELECT t.id, t.reference_id, t.from_account_id, t.to_account_id, "
             "fa.account_number AS from_acc_num, ta.account_number AS to_acc_num, "
             "t.transaction_type, t.amount_paise, t.balance_after_paise, t.category, "
             "t.description, t.status, t.flag_reason, t.created_at, "
             "fa.customer_id AS sender_cust_id, ta.customer_id AS rcpt_cust_id "
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

        int tx_id = atoi(row[0]);
        const char *ref = row[1];
        int from_id = row[2] ? atoi(row[2]) : 0;
        int to_id = row[3] ? atoi(row[3]) : 0;
        const char *from_num = row[4] ? row[4] : "";
        const char *to_num = row[5] ? row[5] : "";
        const char *type = row[6];
        int64_t amount = _strtoi64(row[7], NULL, 10);
        int64_t bal_after = _strtoi64(row[8], NULL, 10);
        const char *cat = row[9] ? row[9] : "General";
        const char *desc = row[10] ? row[10] : "";
        const char *status = row[11] ? row[11] : "SUCCESS";
        const char *flag_reason = row[12] ? row[12] : "";
        const char *created = row[13];
        int sender_cust = row[14] ? atoi(row[14]) : 0;

        // Is this a debit or credit for the active viewing customer?
        bool is_debit = (sender_cust == customer_id && strcmp(type, "DEPOSIT") != 0);

        char amt_fmt[64], bal_fmt[64];
        format_inr(amount, amt_fmt, sizeof(amt_fmt));
        format_inr(bal_after, bal_fmt, sizeof(bal_fmt));

        char esc_desc_out[256];
        json_escape_string(desc, esc_desc_out, sizeof(esc_desc_out));

        jb_append_format(jb,
            "{\"id\":%d,\"reference_id\":\"%s\",\"from_account_id\":%d,\"to_account_id\":%d,"
            "\"from_account_number\":\"%s\",\"to_account_number\":\"%s\",\"type\":\"%s\","
            "\"amount_paise\":%lld,\"amount_formatted\":\"%s\",\"balance_after_paise\":%lld,"
            "\"balance_after_formatted\":\"%s\",\"category\":\"%s\",\"description\":\"%s\","
            "\"status\":\"%s\",\"flag_reason\":\"%s\",\"created_at\":\"%s\",\"is_debit\":%s}",
            tx_id, ref, from_id, to_id, from_num, to_num, type, (long long)amount, amt_fmt,
            (long long)bal_after, bal_fmt, cat, esc_desc_out, status, flag_reason, created,
            is_debit ? "true" : "false");
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

char* banking_get_statement_json(int customer_id, int account_id, int year, int month) {
    Account acc;
    if (!banking_get_account_by_id(account_id, customer_id, &acc)) return _strdup("{\"error\":\"Account not found\"}");

    char date_start[32];
    snprintf(date_start, sizeof(date_start), "%04d-%02d-01 00:00:00", year, month);

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT t.reference_id, t.transaction_type, t.amount_paise, t.balance_after_paise, t.category, t.description, t.status, t.created_at, "
             "t.from_account_id, t.to_account_id "
             "FROM transactions t "
             "WHERE (t.from_account_id = %d OR t.to_account_id = %d) "
             "AND t.created_at >= '%s' AND t.created_at < DATE_ADD('%s', INTERVAL 1 MONTH) "
             "ORDER BY t.created_at ASC",
             account_id, account_id, date_start, date_start);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("{\"transactions\":[]}");

    JsonBuilder *jb = jb_create(4096);
    char cur_bal_fmt[64];
    format_inr(acc.balance_paise, cur_bal_fmt, sizeof(cur_bal_fmt));

    jb_append_format(jb,
        "{\"account_number\":\"%s\",\"account_type\":\"%s\",\"currency\":\"%s\",\"closing_balance\":\"%s\","
        "\"statement_period\":\"%04d-%02d\",\"transactions\":[",
        acc.account_number, acc.account_type == ACC_TYPE_SAVINGS ? "SAVINGS" : "CURRENT",
        acc.currency, cur_bal_fmt, year, month);

    MYSQL_ROW row;
    bool first = true;
    int64_t total_credit = 0, total_debit = 0;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;

        const char *ref = row[0];
        const char *type = row[1];
        int64_t amount = _strtoi64(row[2], NULL, 10);
        int64_t bal_after = _strtoi64(row[3], NULL, 10);
        const char *cat = row[4];
        const char *desc = row[5];
        const char *created = row[7];
        int from_id = row[8] ? atoi(row[8]) : 0;

        bool is_debit = (from_id == account_id && strcmp(type, "DEPOSIT") != 0);
        if (is_debit) total_debit += amount; else total_credit += amount;

        char amt_fmt[64], bal_fmt[64];
        format_inr(amount, amt_fmt, sizeof(amt_fmt));
        format_inr(bal_after, bal_fmt, sizeof(bal_fmt));
        char esc_desc[256];
        json_escape_string(desc ? desc : "", esc_desc, sizeof(esc_desc));

        jb_append_format(jb,
            "{\"ref\":\"%s\",\"type\":\"%s\",\"amount\":\"%s\",\"balance_after\":\"%s\",\"category\":\"%s\",\"desc\":\"%s\",\"date\":\"%s\",\"is_debit\":%s}",
            ref, type, amt_fmt, bal_fmt, cat, esc_desc, created, is_debit ? "true" : "false");
    }
    db_free_result(res);

    char cr_fmt[64], dr_fmt[64];
    format_inr(total_credit, cr_fmt, sizeof(cr_fmt));
    format_inr(total_debit, dr_fmt, sizeof(dr_fmt));

    jb_append_format(jb, "],\"total_deposits\":\"%s\",\"total_withdrawals\":\"%s\"}", cr_fmt, dr_fmt);
    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

char* banking_get_beneficiaries_json(int customer_id) {
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, beneficiary_name, account_number, bank_name, ifsc_code, email, nickname, created_at "
             "FROM beneficiaries WHERE customer_id = %d ORDER BY id DESC", customer_id);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("[]");

    JsonBuilder *jb = jb_create(1024);
    jb_append(jb, "[");
    MYSQL_ROW row;
    bool first = true;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;
        jb_append_format(jb,
            "{\"id\":%d,\"name\":\"%s\",\"account_number\":\"%s\",\"bank_name\":\"%s\",\"ifsc\":\"%s\",\"email\":\"%s\",\"nickname\":\"%s\",\"created_at\":\"%s\"}",
            atoi(row[0]), row[1], row[2], row[3], row[4], row[5] ? row[5] : "", row[6] ? row[6] : "", row[7]);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

bool banking_add_beneficiary(int customer_id, const char *name, const char *account_number,
                             const char *bank_name, const char *ifsc, const char *email, const char *nickname) {
    if (customer_id <= 0 || !name || !account_number) return false;
    char esc_name[128], esc_acc[32], esc_bank[128], esc_ifsc[32], esc_email[128], esc_nick[64];
    db_escape_string(name, esc_name, sizeof(esc_name));
    db_escape_string(account_number, esc_acc, sizeof(esc_acc));
    db_escape_string(bank_name ? bank_name : "Smart Bank", esc_bank, sizeof(esc_bank));
    db_escape_string(ifsc ? ifsc : "SMRT0001001", esc_ifsc, sizeof(esc_ifsc));
    db_escape_string(email ? email : "", esc_email, sizeof(esc_email));
    db_escape_string(nickname ? nickname : "", esc_nick, sizeof(esc_nick));

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "INSERT INTO beneficiaries (customer_id, beneficiary_name, account_number, bank_name, ifsc_code, email, nickname) "
             "VALUES (%d, '%s', '%s', '%s', '%s', '%s', '%s')",
             customer_id, esc_name, esc_acc, esc_bank, esc_ifsc, esc_email, esc_nick);
    return db_query(sql);
}

bool banking_delete_beneficiary(int customer_id, int beneficiary_id) {
    if (customer_id <= 0 || beneficiary_id <= 0) return false;
    char sql[256];
    snprintf(sql, sizeof(sql), "DELETE FROM beneficiaries WHERE id = %d AND customer_id = %d", beneficiary_id, customer_id);
    return db_query(sql);
}

char* banking_get_virtual_cards_json(int customer_id) {
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT vc.id, vc.account_id, a.account_number, vc.card_number_masked, vc.card_type, "
             "vc.expiry_month, vc.expiry_year, vc.spending_limit_paise, vc.status, vc.is_simulated, vc.created_at "
             "FROM virtual_cards vc JOIN accounts a ON vc.account_id = a.id "
             "WHERE vc.customer_id = %d ORDER BY vc.id ASC", customer_id);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("[]");

    JsonBuilder *jb = jb_create(2048);
    jb_append(jb, "[");
    MYSQL_ROW row;
    bool first = true;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;

        int64_t limit_paise = _strtoi64(row[7], NULL, 10);
        char limit_fmt[64];
        format_inr(limit_paise, limit_fmt, sizeof(limit_fmt));

        jb_append_format(jb,
            "{\"id\":%d,\"account_id\":%d,\"account_number\":\"%s\",\"card_number\":\"%s\",\"type\":\"%s\","
            "\"expiry_month\":%d,\"expiry_year\":%d,\"spending_limit_paise\":%lld,\"spending_limit_formatted\":\"%s\","
            "\"status\":\"%s\",\"is_simulated\":%s,\"created_at\":\"%s\"}",
            atoi(row[0]), atoi(row[1]), row[2], row[3], row[4],
            atoi(row[5]), atoi(row[6]), (long long)limit_paise, limit_fmt,
            row[8], (atoi(row[9]) == 1) ? "true" : "false", row[10]);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

bool banking_create_virtual_card(int customer_id, int account_id, const char *card_type, VirtualCard *out_card) {
    Account acc;
    if (!banking_get_account_by_id(account_id, customer_id, &acc)) return false;

    // Simulated card generator
    int r2 = rand() % 9000 + 1000;
    char masked[48];
    const char *prefix = (card_type && strcmp(card_type, "RUPAY") == 0) ? "6074" :
                         (card_type && strcmp(card_type, "MASTERCARD") == 0) ? "5241" : "4532";
    snprintf(masked, sizeof(masked), "%s **** **** %d", prefix, r2);

    time_t now = time(NULL);
    struct tm tm_info;
    localtime_s(&tm_info, &now);
    int exp_month = (tm_info.tm_mon + 1);
    int exp_year = (tm_info.tm_year + 1900 + 4); // 4-year validity

    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO virtual_cards (customer_id, account_id, card_number_masked, card_type, expiry_month, expiry_year, spending_limit_paise, status, is_simulated) "
             "VALUES (%d, %d, '%s', '%s', %d, %d, 5000000, 'ACTIVE', 1)",
             customer_id, account_id, masked, card_type ? card_type : "VISA", exp_month, exp_year);

    if (!db_query(sql)) return false;

    if (out_card) {
        out_card->id = (int)db_insert_id();
        out_card->customer_id = customer_id;
        out_card->account_id = account_id;
        snprintf(out_card->card_number_masked, sizeof(out_card->card_number_masked), "%s", masked);
        snprintf(out_card->card_type, sizeof(out_card->card_type), "%s", card_type ? card_type : "VISA");
        out_card->expiry_month = exp_month;
        out_card->expiry_year = exp_year;
        out_card->spending_limit_paise = 5000000;
        strcpy(out_card->status, "ACTIVE");
        out_card->is_simulated = true;
    }

    add_customer_notification(customer_id, "Virtual Card Issued", "A new simulated virtual card has been issued for your account.", "SYSTEM");
    db_audit_log("CUSTOMER", customer_id, "VIRTUAL_CARD_CREATED", "CARD", out_card ? out_card->id : 0, masked, "127.0.0.1");
    return true;
}

bool banking_toggle_card_freeze(int customer_id, int card_id) {
    char sql_check[256];
    snprintf(sql_check, sizeof(sql_check), "SELECT status FROM virtual_cards WHERE id = %d AND customer_id = %d LIMIT 1", card_id, customer_id);
    MYSQL_RES *res = db_query_store(sql_check);
    if (!res) return false;

    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        db_free_result(res);
        return false;
    }
    const char *cur_status = row[0];
    const char *new_status = (strcmp(cur_status, "ACTIVE") == 0) ? "FROZEN" : "ACTIVE";
    db_free_result(res);

    char upd_sql[256];
    snprintf(upd_sql, sizeof(upd_sql), "UPDATE virtual_cards SET status = '%s' WHERE id = %d AND customer_id = %d", new_status, card_id, customer_id);
    if (!db_query(upd_sql)) return false;

    char msg[128];
    snprintf(msg, sizeof(msg), "Your virtual card has been marked as %s.", new_status);
    add_customer_notification(customer_id, "Card Status Changed", msg, "SECURITY");
    return true;
}

char* banking_get_service_requests_json(int customer_id) {
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, ticket_number, account_id, request_type, description, priority, status, admin_notes, created_at, updated_at "
             "FROM service_requests WHERE customer_id = %d ORDER BY id DESC", customer_id);

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
        json_escape_string(row[4] ? row[4] : "", esc_desc, sizeof(esc_desc));
        json_escape_string(row[7] ? row[7] : "", esc_notes, sizeof(esc_notes));

        jb_append_format(jb,
            "{\"id\":%d,\"ticket_number\":\"%s\",\"account_id\":%d,\"request_type\":\"%s\","
            "\"description\":\"%s\",\"priority\":\"%s\",\"status\":\"%s\",\"admin_notes\":\"%s\","
            "\"created_at\":\"%s\",\"updated_at\":\"%s\"}",
            atoi(row[0]), row[1], row[2] ? atoi(row[2]) : 0, row[3],
            esc_desc, row[5], row[6], esc_notes, row[8], row[9]);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

bool banking_create_service_request(int customer_id, int account_id, const char *request_type,
                                    const char *description, const char *priority, ServiceRequest *out_req) {
    if (customer_id <= 0 || !request_type || !description) return false;

    char ticket[32];
    gen_ref_id("SR-", ticket, sizeof(ticket));

    char esc_type[64], esc_desc[1024], esc_pri[32];
    db_escape_string(request_type, esc_type, sizeof(esc_type));
    db_escape_string(description, esc_desc, sizeof(esc_desc));
    db_escape_string(priority ? priority : "MEDIUM", esc_pri, sizeof(esc_pri));

    char sql[2048];
    if (account_id > 0) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO service_requests (ticket_number, customer_id, account_id, request_type, description, priority, status) "
                 "VALUES ('%s', %d, %d, '%s', '%s', '%s', 'PENDING')",
                 ticket, customer_id, account_id, esc_type, esc_desc, esc_pri);
    } else {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO service_requests (ticket_number, customer_id, request_type, description, priority, status) "
                 "VALUES ('%s', %d, '%s', '%s', '%s', 'PENDING')",
                 ticket, customer_id, esc_type, esc_desc, esc_pri);
    }

    if (!db_query(sql)) return false;

    int new_id = (int)db_insert_id();
    if (out_req) {
        out_req->id = new_id;
        snprintf(out_req->ticket_number, sizeof(out_req->ticket_number), "%s", ticket);
        out_req->customer_id = customer_id;
        out_req->account_id = account_id;
        snprintf(out_req->request_type, sizeof(out_req->request_type), "%s", request_type);
        snprintf(out_req->description, sizeof(out_req->description), "%s", description);
        out_req->status = SR_PENDING;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Your service request ticket %s has been submitted and queued for review.", ticket);
    add_customer_notification(customer_id, "Service Request Submitted", msg, "SERVICE");
    db_audit_log("CUSTOMER", customer_id, "SERVICE_REQUEST_SUBMITTED", "SERVICE_REQUEST", new_id, ticket, "127.0.0.1");
    return true;
}

char* banking_get_notifications_json(int customer_id) {
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT id, title, message, type, is_read, created_at "
             "FROM notifications WHERE customer_id = %d ORDER BY created_at DESC LIMIT 50", customer_id);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("[]");

    JsonBuilder *jb = jb_create(2048);
    jb_append(jb, "[");
    MYSQL_ROW row;
    bool first = true;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;

        char esc_title[256], esc_msg[1024];
        json_escape_string(row[1], esc_title, sizeof(esc_title));
        json_escape_string(row[2], esc_msg, sizeof(esc_msg));

        jb_append_format(jb,
            "{\"id\":%d,\"title\":\"%s\",\"message\":\"%s\",\"type\":\"%s\",\"is_read\":%s,\"created_at\":\"%s\"}",
            atoi(row[0]), esc_title, esc_msg, row[3], (atoi(row[4]) == 1) ? "true" : "false", row[5]);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

bool banking_mark_notification_read(int customer_id, int notification_id) {
    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE notifications SET is_read = 1 WHERE id = %d AND customer_id = %d", notification_id, customer_id);
    return db_query(sql);
}

bool banking_mark_all_notifications_read(int customer_id) {
    char sql[256];
    snprintf(sql, sizeof(sql), "UPDATE notifications SET is_read = 1 WHERE customer_id = %d", customer_id);
    return db_query(sql);
}

#include "analytics.h"
#include "db.h"
#include "banking.h"
#include "json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

char* analytics_get_summary_json(int customer_id) {
    // 1. Total Balance across all customer accounts
    char bal_sql[256];
    snprintf(bal_sql, sizeof(bal_sql),
             "SELECT COALESCE(SUM(balance_paise), 0), COUNT(*) FROM accounts WHERE customer_id = %d AND status != 'CLOSED'",
             customer_id);
    MYSQL_RES *res = db_query_store(bal_sql);
    int64_t total_balance = 0;
    int total_accounts = 0;
    if (res) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row) {
            total_balance = _strtoi64(row[0], NULL, 10);
            total_accounts = atoi(row[1]);
        }
        db_free_result(res);
    }

    // 2. Current Month Income & Expenses
    // Income: DEPOSIT, INTEREST, or incoming TRANSFER where to_account belongs to customer
    char inc_sql[512];
    snprintf(inc_sql, sizeof(inc_sql),
             "SELECT COALESCE(SUM(t.amount_paise), 0) FROM transactions t "
             "JOIN accounts a ON t.to_account_id = a.id "
             "WHERE a.customer_id = %d AND (t.from_account_id IS NULL OR t.from_account_id NOT IN (SELECT id FROM accounts WHERE customer_id = %d)) "
             "AND t.status = 'SUCCESS' AND MONTH(t.created_at) = MONTH(CURRENT_DATE()) AND YEAR(t.created_at) = YEAR(CURRENT_DATE())",
             customer_id, customer_id);
    res = db_query_store(inc_sql);
    int64_t total_income = 0;
    if (res) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0]) total_income = _strtoi64(row[0], NULL, 10);
        db_free_result(res);
    }

    // Expenses: WITHDRAWAL, FEE, or outgoing TRANSFER where to_account does not belong to customer
    char exp_sql[512];
    snprintf(exp_sql, sizeof(exp_sql),
             "SELECT COALESCE(SUM(t.amount_paise), 0) FROM transactions t "
             "JOIN accounts a ON t.from_account_id = a.id "
             "WHERE a.customer_id = %d AND (t.to_account_id IS NULL OR t.to_account_id NOT IN (SELECT id FROM accounts WHERE customer_id = %d)) "
             "AND t.status = 'SUCCESS' AND MONTH(t.created_at) = MONTH(CURRENT_DATE()) AND YEAR(t.created_at) = YEAR(CURRENT_DATE())",
             customer_id, customer_id);
    res = db_query_store(exp_sql);
    int64_t total_expenses = 0;
    if (res) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0]) total_expenses = _strtoi64(row[0], NULL, 10);
        db_free_result(res);
    }

    int64_t net_savings = total_income - total_expenses;
    double savings_rate = (total_income > 0) ? ((double)net_savings / (double)total_income) * 100.0 : 0.0;
    if (savings_rate < 0) savings_rate = 0.0;

    char bal_fmt[64], inc_fmt[64], exp_fmt[64], sav_fmt[64];
    format_inr(total_balance, bal_fmt, sizeof(bal_fmt));
    format_inr(total_income, inc_fmt, sizeof(inc_fmt));
    format_inr(total_expenses, exp_fmt, sizeof(exp_fmt));
    format_inr(net_savings, sav_fmt, sizeof(sav_fmt));

    JsonBuilder *jb = jb_create(4096);
    jb_append(jb, "{\n");
    jb_append_format(jb, "  \"total_balance_paise\": %lld,\n  \"total_balance_formatted\": \"%s\",\n", (long long)total_balance, bal_fmt);
    jb_append_format(jb, "  \"total_income_paise\": %lld,\n  \"total_income_formatted\": \"%s\",\n", (long long)total_income, inc_fmt);
    jb_append_format(jb, "  \"total_expenses_paise\": %lld,\n  \"total_expenses_formatted\": \"%s\",\n", (long long)total_expenses, exp_fmt);
    jb_append_format(jb, "  \"net_savings_paise\": %lld,\n  \"net_savings_formatted\": \"%s\",\n", (long long)net_savings, sav_fmt);
    jb_append_format(jb, "  \"savings_rate_pct\": %.1f,\n  \"total_accounts\": %d,\n", savings_rate, total_accounts);

    // 3. Category Breakdown (Expenses by category)
    char cat_sql[1024];
    snprintf(cat_sql, sizeof(cat_sql),
             "SELECT t.category, COALESCE(SUM(t.amount_paise), 0) AS cat_total "
             "FROM transactions t JOIN accounts a ON t.from_account_id = a.id "
             "WHERE a.customer_id = %d AND t.status = 'SUCCESS' "
             "GROUP BY t.category ORDER BY cat_total DESC LIMIT 8",
             customer_id);

    jb_append(jb, "  \"category_breakdown\": [");
    res = db_query_store(cat_sql);
    if (res) {
        MYSQL_ROW row;
        bool first = true;
        while ((row = mysql_fetch_row(res))) {
            if (!first) jb_append(jb, ",");
            first = false;
            const char *cat_name = row[0] ? row[0] : "General";
            int64_t cat_amount = _strtoi64(row[1], NULL, 10);
            char cat_fmt[64];
            format_inr(cat_amount, cat_fmt, sizeof(cat_fmt));
            double pct = (total_expenses > 0) ? ((double)cat_amount / (double)total_expenses) * 100.0 : 0.0;
            jb_append_format(jb, "{\"category\":\"%s\",\"amount_paise\":%lld,\"amount_formatted\":\"%s\",\"percentage\":%.1f}",
                             cat_name, (long long)cat_amount, cat_fmt, pct);
        }
        db_free_result(res);
    }
    jb_append(jb, "],\n");

    // 4. Monthly Trend (Past 6 months)
    jb_append(jb, "  \"monthly_trends\": [\n");
    static const char *month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_s(&tm_info, &now);

    for (int m = 5; m >= 0; m--) {
        int target_mon = tm_info.tm_mon - m;
        int target_year = tm_info.tm_year + 1900;
        if (target_mon < 0) {
            target_mon += 12;
            target_year--;
        }

        // Query income for that month
        char m_inc_sql[512];
        snprintf(m_inc_sql, sizeof(m_inc_sql),
                 "SELECT COALESCE(SUM(t.amount_paise), 0) FROM transactions t JOIN accounts a ON t.to_account_id = a.id "
                 "WHERE a.customer_id = %d AND t.status = 'SUCCESS' AND MONTH(t.created_at) = %d AND YEAR(t.created_at) = %d",
                 customer_id, target_mon + 1, target_year);
        res = db_query_store(m_inc_sql);
        int64_t m_inc = 0;
        if (res) {
            MYSQL_ROW r = mysql_fetch_row(res);
            if (r && r[0]) m_inc = _strtoi64(r[0], NULL, 10);
            db_free_result(res);
        }

        // Query expense for that month
        char m_exp_sql[512];
        snprintf(m_exp_sql, sizeof(m_exp_sql),
                 "SELECT COALESCE(SUM(t.amount_paise), 0) FROM transactions t JOIN accounts a ON t.from_account_id = a.id "
                 "WHERE a.customer_id = %d AND t.status = 'SUCCESS' AND MONTH(t.created_at) = %d AND YEAR(t.created_at) = %d",
                 customer_id, target_mon + 1, target_year);
        res = db_query_store(m_exp_sql);
        int64_t m_exp = 0;
        if (res) {
            MYSQL_ROW r = mysql_fetch_row(res);
            if (r && r[0]) m_exp = _strtoi64(r[0], NULL, 10);
            db_free_result(res);
        }

        char m_inc_fmt[64], m_exp_fmt[64];
        format_inr(m_inc, m_inc_fmt, sizeof(m_inc_fmt));
        format_inr(m_exp, m_exp_fmt, sizeof(m_exp_fmt));

        jb_append_format(jb, "    {\"month\":\"%s %d\",\"income_paise\":%lld,\"income_formatted\":\"%s\",\"expense_paise\":%lld,\"expense_formatted\":\"%s\"}%s\n",
                         month_names[target_mon], target_year, (long long)m_inc, m_inc_fmt, (long long)m_exp, m_exp_fmt, (m == 0) ? "" : ",");
    }
    jb_append(jb, "  ]\n");
    jb_append(jb, "}");

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

char* analytics_get_savings_goals_json(int customer_id) {
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT sg.id, sg.account_id, a.account_number, sg.title, sg.target_amount_paise, "
             "sg.current_amount_paise, sg.target_date, sg.category, sg.status, sg.created_at "
             "FROM savings_goals sg JOIN accounts a ON sg.account_id = a.id "
             "WHERE sg.customer_id = %d ORDER BY sg.status ASC, sg.id DESC", customer_id);

    MYSQL_RES *res = db_query_store(sql);
    if (!res) return _strdup("[]");

    JsonBuilder *jb = jb_create(2048);
    jb_append(jb, "[");
    MYSQL_ROW row;
    bool first = true;
    while ((row = mysql_fetch_row(res))) {
        if (!first) jb_append(jb, ",");
        first = false;

        int64_t target = _strtoi64(row[4], NULL, 10);
        int64_t current = _strtoi64(row[5], NULL, 10);
        double progress = (target > 0) ? ((double)current / (double)target) * 100.0 : 0.0;
        if (progress > 100.0) progress = 100.0;

        char tgt_fmt[64], cur_fmt[64];
        format_inr(target, tgt_fmt, sizeof(tgt_fmt));
        format_inr(current, cur_fmt, sizeof(cur_fmt));

        jb_append_format(jb,
            "{\"id\":%d,\"account_id\":%d,\"account_number\":\"%s\",\"title\":\"%s\","
            "\"target_paise\":%lld,\"target_formatted\":\"%s\",\"current_paise\":%lld,\"current_formatted\":\"%s\","
            "\"progress_pct\":%.1f,\"target_date\":\"%s\",\"category\":\"%s\",\"status\":\"%s\",\"created_at\":\"%s\"}",
            atoi(row[0]), atoi(row[1]), row[2], row[3],
            (long long)target, tgt_fmt, (long long)current, cur_fmt,
            progress, row[6], row[7], row[8], row[9]);
    }
    jb_append(jb, "]");
    db_free_result(res);

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

bool analytics_create_savings_goal(int customer_id, int account_id, const char *title,
                                   int64_t target_paise, const char *target_date, const char *category) {
    if (customer_id <= 0 || !title || target_paise <= 0 || !target_date) return false;
    Account acc;
    if (!banking_get_account_by_id(account_id, customer_id, &acc)) return false;

    char esc_title[128], esc_date[32], esc_cat[64];
    db_escape_string(title, esc_title, sizeof(esc_title));
    db_escape_string(target_date, esc_date, sizeof(esc_date));
    db_escape_string(category ? category : "Emergency Fund", esc_cat, sizeof(esc_cat));

    char sql[512];
    snprintf(sql, sizeof(sql),
             "INSERT INTO savings_goals (customer_id, account_id, title, target_amount_paise, target_date, category, status) "
             "VALUES (%d, %d, '%s', %lld, '%s', '%s', 'IN_PROGRESS')",
             customer_id, account_id, esc_title, (long long)target_paise, esc_date, esc_cat);

    return db_query(sql);
}

bool analytics_contribute_to_goal(int customer_id, int goal_id, int64_t amount_paise, const char *note,
                                  char *err_msg, size_t err_len) {
    if (amount_paise <= 0) {
        snprintf(err_msg, err_len, "Contribution amount must be greater than zero.");
        return false;
    }

    char goal_sql[256];
    snprintf(goal_sql, sizeof(goal_sql),
             "SELECT account_id, current_amount_paise, target_amount_paise, title FROM savings_goals WHERE id = %d AND customer_id = %d LIMIT 1",
             goal_id, customer_id);

    MYSQL_RES *res = db_query_store(goal_sql);
    if (!res) {
        snprintf(err_msg, err_len, "Database lookup error.");
        return false;
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        db_free_result(res);
        snprintf(err_msg, err_len, "Savings goal not found.");
        return false;
    }

    int acc_id = atoi(row[0]);
    int64_t cur_goal_paise = _strtoi64(row[1], NULL, 10);
    int64_t tgt_goal_paise = _strtoi64(row[2], NULL, 10);
    char goal_title[128];
    strncpy(goal_title, row[3], sizeof(goal_title) - 1);
    db_free_result(res);

    Account acc;
    if (!banking_get_account_by_id(acc_id, customer_id, &acc)) {
        snprintf(err_msg, err_len, "Linked account not accessible.");
        return false;
    }

    if (acc.balance_paise < amount_paise) {
        char bal_fmt[64], req_fmt[64];
        format_inr(acc.balance_paise, bal_fmt, sizeof(bal_fmt));
        format_inr(amount_paise, req_fmt, sizeof(req_fmt));
        snprintf(err_msg, err_len, "Insufficient balance in linked account. Available: %s, Required: %s", bal_fmt, req_fmt);
        return false;
    }

    if (!db_begin()) {
        snprintf(err_msg, err_len, "Transaction failure.");
        return false;
    }

    // Debit account
    int64_t new_acc_bal = acc.balance_paise - amount_paise;
    char debit_sql[256];
    snprintf(debit_sql, sizeof(debit_sql),
             "UPDATE accounts SET balance_paise = %lld WHERE id = %d AND customer_id = %d",
             (long long)new_acc_bal, acc_id, customer_id);
    if (!db_query(debit_sql)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to deduct funds from account.");
        return false;
    }

    // Update goal
    int64_t new_goal_amt = cur_goal_paise + amount_paise;
    const char *new_status = (new_goal_amt >= tgt_goal_paise) ? "COMPLETED" : "IN_PROGRESS";
    char upd_goal[256];
    snprintf(upd_goal, sizeof(upd_goal),
             "UPDATE savings_goals SET current_amount_paise = %lld, status = '%s' WHERE id = %d",
             (long long)new_goal_amt, new_status, goal_id);
    if (!db_query(upd_goal)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to update savings goal.");
        return false;
    }

    // Insert contribution
    char esc_note[256];
    db_escape_string(note ? note : "Goal Contribution", esc_note, sizeof(esc_note));
    char contrib_sql[512];
    snprintf(contrib_sql, sizeof(contrib_sql),
             "INSERT INTO savings_contributions (goal_id, amount_paise, note) VALUES (%d, %lld, '%s')",
             goal_id, (long long)amount_paise, esc_note);
    if (!db_query(contrib_sql)) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to record contribution log.");
        return false;
    }

    if (!db_commit()) {
        db_rollback();
        snprintf(err_msg, err_len, "Failed to commit contribution.");
        return false;
    }

    return true;
}

char* analytics_ai_financial_advisor(int customer_id, const char *user_prompt) {
    // 1. Gather actual financial metrics for this user
    char bal_sql[256];
    snprintf(bal_sql, sizeof(bal_sql),
             "SELECT COALESCE(SUM(balance_paise), 0) FROM accounts WHERE customer_id = %d AND status = 'ACTIVE'", customer_id);
    MYSQL_RES *res = db_query_store(bal_sql);
    int64_t total_balance = 0;
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) total_balance = _strtoi64(r[0], NULL, 10);
        db_free_result(res);
    }

    // Top spending category
    char top_cat[64] = "General";
    int64_t top_cat_amt = 0;
    char cat_sql[512];
    snprintf(cat_sql, sizeof(cat_sql),
             "SELECT t.category, SUM(t.amount_paise) AS s FROM transactions t "
             "JOIN accounts a ON t.from_account_id = a.id WHERE a.customer_id = %d "
             "AND t.status = 'SUCCESS' GROUP BY t.category ORDER BY s DESC LIMIT 1", customer_id);
    res = db_query_store(cat_sql);
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) {
            strncpy(top_cat, r[0], sizeof(top_cat) - 1);
            top_cat_amt = _strtoi64(r[1], NULL, 10);
        }
        db_free_result(res);
    }

    // Goal count
    int active_goals = 0;
    char goal_sql[256];
    snprintf(goal_sql, sizeof(goal_sql),
             "SELECT COUNT(*) FROM savings_goals WHERE customer_id = %d AND status = 'IN_PROGRESS'", customer_id);
    res = db_query_store(goal_sql);
    if (res) {
        MYSQL_ROW r = mysql_fetch_row(res);
        if (r && r[0]) active_goals = atoi(r[0]);
        db_free_result(res);
    }

    char bal_fmt[64], top_fmt[64];
    format_inr(total_balance, bal_fmt, sizeof(bal_fmt));
    format_inr(top_cat_amt, top_fmt, sizeof(top_fmt));

    // Convert prompt to lowercase for deterministic intent matching
    char lower_prompt[512];
    strncpy(lower_prompt, user_prompt ? user_prompt : "", sizeof(lower_prompt) - 1);
    lower_prompt[sizeof(lower_prompt) - 1] = '\0';
    for (int i = 0; lower_prompt[i]; i++) {
        lower_prompt[i] = (char)tolower((unsigned char)lower_prompt[i]);
    }

    char reply[2048];
    char suggestion[512];

    if (strstr(lower_prompt, "spend") || strstr(lower_prompt, "expense") || strstr(lower_prompt, "category")) {
        snprintf(reply, sizeof(reply),
                 "Based on your latest account transactions, your primary outflow category is **%s**, totaling %s. "
                 "Overall, you have %s spread across active accounts.",
                 top_cat, top_fmt, bal_fmt);
        snprintf(suggestion, sizeof(suggestion),
                 "Try establishing a weekly cap on %s to free up at least ₹2,500 every month for your savings goals.",
                 top_cat);
    } else if (strstr(lower_prompt, "goal") || strstr(lower_prompt, "save") || strstr(lower_prompt, "saving")) {
        snprintf(reply, sizeof(reply),
                 "You currently have **%d active savings goal(s)** in progress. Your total liquidity across all accounts is **%s**.",
                 active_goals, bal_fmt);
        snprintf(suggestion, sizeof(suggestion),
                 "Automating a ₹5,000 monthly standing transfer right after salary credit will increase your goal completion velocity by 25%%.");
    } else if (strstr(lower_prompt, "afford") || strstr(lower_prompt, "budget") || strstr(lower_prompt, "plan")) {
        snprintf(reply, sizeof(reply),
                 "Your current net financial reserve is **%s**. Using the standard 50/30/20 banking rule (50%% needs, 30%% wants, 20%% savings), "
                 "your discretionary budget is in a healthy range.",
                 bal_fmt);
        snprintf(suggestion, sizeof(suggestion),
                 "Maintain a minimum 3-month emergency buffer of ₹1,50,000 before taking on major non-essential capital purchases.");
    } else {
        snprintf(reply, sizeof(reply),
                 "Hello! I am your Smart Bank Financial Advisor. Analyzing your live accounts: your total net worth in Smart Bank is **%s**, "
                 "with your highest expenditure concentrated in **%s** (%s). You have **%d active savings goal(s)**.",
                 bal_fmt, top_cat, top_fmt, active_goals);
        snprintf(suggestion, sizeof(suggestion),
                 "Ask me: 'Where did I spend the most?', 'How are my savings goals?', or 'How can I optimize my monthly budget?'");
    }

    char esc_reply[2048], esc_sug[1024];
    json_escape_string(reply, esc_reply, sizeof(esc_reply));
    json_escape_string(suggestion, esc_sug, sizeof(esc_sug));

    JsonBuilder *jb = jb_create(3072);
    jb_append(jb, "{\n");
    jb_append(jb, "  \"status\": \"success\",\n");
    jb_append(jb, "  \"engine\": \"Deterministic Rule-Based Intelligence Engine\",\n");
    jb_append_format(jb, "  \"message\": \"%s\",\n", esc_reply);
    jb_append_format(jb, "  \"actionable_tip\": \"%s\",\n", esc_sug);
    jb_append_format(jb, "  \"metrics\": {\"net_balance\": \"%s\", \"top_category\": \"%s\", \"active_goals\": %d}\n",
                     bal_fmt, top_cat, active_goals);
    jb_append(jb, "}");

    char *json = _strdup(jb_str(jb));
    jb_free(jb);
    return json;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "config.h"
#include "db.h"
#include "auth.h"
#include "banking.h"
#include "analytics.h"
#include "admin.h"
#include "ds.h"
#include "json_utils.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define TEST_ASSERT(cond, name) do { \
    g_tests_run++; \
    if (cond) { \
        printf("  [PASS] %s\n", name); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", name, __LINE__); \
    } \
} while(0)

int main() {
    printf("====================================================================\n");
    printf("     SMART BANKING MANAGEMENT SYSTEM - AUTOMATED TEST SUITE        \n");
    printf("====================================================================\n");

    // Configure test environment using test database
    config_init(".env");
    strncpy(g_config.db_name, "smart_banking_test", sizeof(g_config.db_name) - 1);

    printf("[TEST] Connecting to test database: %s...\n", g_config.db_name);
    bool db_ok = db_init();
    TEST_ASSERT(db_ok, "MySQL 8.0 Test Database Connectivity");
    if (!db_ok) {
        printf("[FATAL] Cannot proceed without test database.\n");
        return 1;
    }

    // 1. Argon2id Cryptography Tests
    printf("\n--- Test Suite 1: Argon2id Cryptography & Authentication ---\n");
    char hash[MAX_HASH_LEN];
    bool hash_ok = auth_hash_password("SecretPass#2026", hash, sizeof(hash));
    TEST_ASSERT(hash_ok && strstr(hash, "$argon2id$"), "Argon2id Password Hashing");
    bool verify_ok = auth_verify_password("SecretPass#2026", hash);
    TEST_ASSERT(verify_ok, "Argon2id Correct Password Verification");
    bool verify_fail = auth_verify_password("WrongPassword", hash);
    TEST_ASSERT(!verify_fail, "Argon2id Wrong Password Rejection");

    // 2. Transaction PIN Tests
    printf("\n--- Test Suite 2: Transaction PIN Operations ---\n");
    bool pin_set = auth_set_pin(1, "889900");
    TEST_ASSERT(pin_set, "Set Customer Transaction PIN");
    bool pin_check_true = auth_verify_pin(1, "889900");
    TEST_ASSERT(pin_check_true, "Verify Correct Transaction PIN");
    bool pin_check_false = auth_verify_pin(1, "000000");
    TEST_ASSERT(!pin_check_false, "Reject Incorrect Transaction PIN");

    // 3. Core Banking: Account Operations & Paise Calculations
    printf("\n--- Test Suite 3: Account Creation & Currency Formatting ---\n");
    Account new_acc;
    // ₹50,000 opening deposit = 50,00,000 paise
    bool acc_created = banking_create_account(1, ACC_TYPE_SAVINGS, 5000000, &new_acc);
    TEST_ASSERT(acc_created && new_acc.id > 0, "Create Savings Account with Opening Deposit");
    TEST_ASSERT(new_acc.balance_paise == 5000000, "Opening Balance in Integer Paise");

    char inr_str[64];
    format_inr(5000000, inr_str, sizeof(inr_str));
    TEST_ASSERT(strcmp(inr_str, "₹50,000.00") == 0, "Indian Currency Formatting (50,00,000 paise -> ₹50,000.00)");

    // 4. Core Banking: Deposits & Withdrawals
    printf("\n--- Test Suite 4: ACID Deposits & Withdrawals ---\n");
    Transaction dep_tx;
    char err[256];
    // Deposit ₹25,000 = 25,00,000 paise
    bool dep_ok = banking_deposit(1, new_acc.id, 2500000, "Test Bonus Deposit", "Bonus", &dep_tx, err, sizeof(err));
    TEST_ASSERT(dep_ok, "Execute ACID Deposit");
    TEST_ASSERT(dep_tx.balance_after_paise == 7500000, "Balance After Deposit Updated to ₹75,000.00");

    // Withdrawal ₹15,000 with PIN
    Transaction wdr_tx;
    bool wdr_ok = banking_withdraw(1, new_acc.id, 1500000, "889900", "ATM Cash Out", "General", &wdr_tx, err, sizeof(err));
    TEST_ASSERT(wdr_ok, "Execute ACID Withdrawal with Valid PIN");
    TEST_ASSERT(wdr_tx.balance_after_paise == 6000000, "Balance After Withdrawal Updated to ₹60,000.00");

    // Withdrawal with Wrong PIN
    bool wdr_wrong_pin = banking_withdraw(1, new_acc.id, 100000, "111111", "Fail", "General", &wdr_tx, err, sizeof(err));
    TEST_ASSERT(!wdr_wrong_pin, "Reject Withdrawal with Invalid PIN");

    // Insufficient Balance Withdrawal (Requested ₹1,00,000 on balance of ₹60,000)
    bool wdr_overdraw = banking_withdraw(1, new_acc.id, 10000000, "889900", "Overdraw", "General", &wdr_tx, err, sizeof(err));
    TEST_ASSERT(!wdr_overdraw && strstr(err, "Insufficient"), "Reject Withdrawal Exceeding Available Balance");

    // 5. Internal Account Transfers & Fraud Detection
    printf("\n--- Test Suite 5: Internal Transfers & Rule-Based Fraud Detection ---\n");
    // Transfer ₹10,000 from new_acc to Priya (100100200401)
    Transaction tx_transfer;
    bool xfer_ok = banking_transfer(1, new_acc.id, "100100200401", 1000000, "889900", "Rent Payment", "Transfer", &tx_transfer, err, sizeof(err));
    TEST_ASSERT(xfer_ok, "Execute Internal Account Transfer");
    TEST_ASSERT(tx_transfer.status == TX_STATUS_SUCCESS, "Standard Transfer Status SUCCESS");

    // High Value Transfer > ₹1,00,000 (Should trigger Rule #FR-101)
    // Deposit ₹2,00,000 first so account has enough balance
    banking_deposit(1, new_acc.id, 20000000, "Large Liquidity", "General", &dep_tx, err, sizeof(err));
    Transaction tx_large;
    bool large_xfer_ok = banking_transfer(1, new_acc.id, "100100200401", 12000000, "889900", "Car Purchase", "Transfer", &tx_large, err, sizeof(err));
    TEST_ASSERT(large_xfer_ok, "Process High-Value Transfer");
    TEST_ASSERT(tx_large.status == TX_STATUS_FLAGGED, "Rule-based Suspicious Transfer Triggered FLAGGED Status");

    // 6. Security: Account Freezing Protection
    printf("\n--- Test Suite 6: Account Freeze Protection ---\n");
    bool freeze_ok = banking_freeze_account(new_acc.id, 1, "Suspicious activity suspected");
    TEST_ASSERT(freeze_ok, "Emergency Freeze Account");
    // Attempt withdrawal on frozen account
    bool frozen_wdr = banking_withdraw(1, new_acc.id, 50000, "889900", "Attempt", "General", &wdr_tx, err, sizeof(err));
    TEST_ASSERT(!frozen_wdr && strstr(err, "FROZEN"), "Block Withdrawal on Frozen Account");
    bool unfreeze_ok = banking_unfreeze_account(new_acc.id, 1);
    TEST_ASSERT(unfreeze_ok, "Reactivate / Unfreeze Account");

    // 7. Role-Based Access Control (RBAC)
    printf("\n--- Test Suite 7: Role-Based Access Control (RBAC) ---\n");
    TEST_ASSERT(auth_check_permission("SUPER_ADMIN", "ADMIN"), "SUPER_ADMIN inherits ADMIN authority");
    TEST_ASSERT(auth_check_permission("ADMIN", "SUPPORT"), "ADMIN inherits SUPPORT authority");
    TEST_ASSERT(!auth_check_permission("AUDITOR", "ADMIN"), "AUDITOR denied ADMIN write access");
    TEST_ASSERT(!auth_check_permission("CUSTOMER", "ADMIN"), "CUSTOMER denied ADMIN privileges");

    // 8. C Data Structures & Algorithms
    printf("\n--- Test Suite 8: Core C Data Structures & Algorithms ---\n");
    CustomerList *cl = customer_list_create();
    Customer c_demo = { .id = 99, .first_name = "Vikram", .last_name = "Malhotra", .email = "vikram@test.com" };
    customer_list_insert(cl, &c_demo);
    Customer *found = customer_list_find_by_email(cl, "vikram@test.com");
    TEST_ASSERT(found && strcmp(found->first_name, "Vikram") == 0, "Linked List Insert & Lookup");
    customer_list_free(cl);

    ServiceRequestQueue *q = sr_queue_create();
    ServiceRequest req_in = { .id = 1, .ticket_number = "TICKET-1" };
    sr_queue_enqueue(q, &req_in);
    ServiceRequest req_out;
    bool deq_ok = sr_queue_dequeue(q, &req_out);
    TEST_ASSERT(deq_ok && strcmp(req_out.ticket_number, "TICKET-1") == 0, "FIFO Service Request Queue Processing");
    sr_queue_free(q);

    OperationStack *stk = op_stack_create();
    OperationEntry op_in = { .amount_paise = 450000 };
    strcpy(op_in.action, "TRANSFER");
    op_stack_push(stk, &op_in);
    OperationEntry op_out;
    bool pop_ok = op_stack_pop(stk, &op_out);
    TEST_ASSERT(pop_ok && strcmp(op_out.action, "TRANSFER") == 0, "LIFO Operation Undo/Audit Stack");
    op_stack_free(stk);

    Account sorted_arr[3] = {
        { .id = 1, .account_number = "1001001" },
        { .id = 2, .account_number = "1001002" },
        { .id = 3, .account_number = "1001003" }
    };
    int bidx = binary_search_accounts_by_number(sorted_arr, 3, "1001002");
    TEST_ASSERT(bidx == 1, "Binary Search on Sorted Accounts O(log n)");

    printf("\n====================================================================\n");
    printf("TEST RESULTS: %d of %d tests passed (%.1f%%)\n",
           g_tests_passed, g_tests_run, ((double)g_tests_passed / (double)g_tests_run) * 100.0);
    printf("====================================================================\n");

    db_close();
    return (g_tests_passed == g_tests_run) ? 0 : 1;
}

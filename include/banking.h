#ifndef BANKING_H
#define BANKING_H

#include "models.h"
#include <stdbool.h>
#include <stdint.h>

// Account Operations
bool banking_create_account(int customer_id, AccountType type, int64_t initial_deposit_paise, Account *out_acc);
char* banking_get_customer_accounts_json(int customer_id);
bool banking_get_account_by_id(int account_id, int customer_id, Account *out_acc);
bool banking_freeze_account(int account_id, int customer_id, const char *reason);
bool banking_unfreeze_account(int account_id, int customer_id);

// Financial Operations (ACID with rollback on error)
bool banking_deposit(int customer_id, int account_id, int64_t amount_paise, const char *description,
                     const char *category, Transaction *out_tx, char *err_msg, size_t err_len);

bool banking_withdraw(int customer_id, int account_id, int64_t amount_paise, const char *pin,
                      const char *description, const char *category, Transaction *out_tx, char *err_msg, size_t err_len);

bool banking_transfer(int customer_id, int from_account_id, const char *to_account_number,
                      int64_t amount_paise, const char *pin, const char *description,
                      const char *category, Transaction *out_tx, char *err_msg, size_t err_len);

// Transactions & Statements
char* banking_get_transactions_json(int customer_id, int account_id, int limit, int offset,
                                    const char *type_filter, const char *category_filter);
char* banking_get_statement_json(int customer_id, int account_id, int year, int month);

// Beneficiaries
char* banking_get_beneficiaries_json(int customer_id);
bool banking_add_beneficiary(int customer_id, const char *name, const char *account_number,
                             const char *bank_name, const char *ifsc, const char *email, const char *nickname);
bool banking_delete_beneficiary(int customer_id, int beneficiary_id);

// Virtual Cards (Simulated)
char* banking_get_virtual_cards_json(int customer_id);
bool banking_create_virtual_card(int customer_id, int account_id, const char *card_type, VirtualCard *out_card);
bool banking_toggle_card_freeze(int customer_id, int card_id);

// Service Requests
char* banking_get_service_requests_json(int customer_id);
bool banking_create_service_request(int customer_id, int account_id, const char *request_type,
                                    const char *description, const char *priority, ServiceRequest *out_req);

// Notifications
char* banking_get_notifications_json(int customer_id);
bool banking_mark_notification_read(int customer_id, int notification_id);
bool banking_mark_all_notifications_read(int customer_id);

#endif // BANKING_H

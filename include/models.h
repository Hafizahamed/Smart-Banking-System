#ifndef MODELS_H
#define MODELS_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define MAX_STR_LEN 256
#define MAX_TEXT_LEN 2048
#define MAX_TOKEN_LEN 128
#define MAX_HASH_LEN 256

// User roles
typedef enum {
    ROLE_CUSTOMER = 0,
    ROLE_SUPER_ADMIN,
    ROLE_ADMIN,
    ROLE_SUPPORT,
    ROLE_AUDITOR
} UserRole;

// Status Enums
typedef enum {
    STATUS_ACTIVE = 0,
    STATUS_FROZEN,
    STATUS_SUSPENDED,
    STATUS_CLOSED
} AccountStatus;

typedef enum {
    ACC_TYPE_SAVINGS = 0,
    ACC_TYPE_CURRENT
} AccountType;

typedef enum {
    TX_DEPOSIT = 0,
    TX_WITHDRAWAL,
    TX_TRANSFER,
    TX_INTEREST,
    TX_FEE
} TransactionType;

typedef enum {
    TX_STATUS_SUCCESS = 0,
    TX_STATUS_FAILED,
    TX_STATUS_FLAGGED,
    TX_STATUS_PENDING
} TransactionStatus;

typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_MEDIUM,
    PRIORITY_HIGH,
    PRIORITY_URGENT
} RequestPriority;

typedef enum {
    SR_PENDING = 0,
    SR_IN_REVIEW,
    SR_APPROVED,
    SR_REJECTED,
    SR_RESOLVED
} ServiceRequestStatus;

// Customer Model
typedef struct Customer {
    int id;
    char customer_number[36];
    char first_name[64];
    char last_name[64];
    char email[128];
    char phone[24];
    char password_hash[MAX_HASH_LEN];
    AccountStatus status;
    bool two_factor_enabled;
    char two_factor_secret[64];
    char created_at[32];
    char updated_at[32];
} Customer;

// Account Model
typedef struct Account {
    int id;
    int customer_id;
    char account_number[24];
    AccountType account_type;
    int64_t balance_paise; // Integer paise: 100 paise = 1 INR
    char currency[4];
    AccountStatus status;
    char branch_code[24];
    char created_at[32];
} Account;

// Transaction Model
typedef struct Transaction {
    int id;
    char reference_id[64];
    int from_account_id;
    int to_account_id;
    char from_account_number[24];
    char to_account_number[24];
    TransactionType transaction_type;
    int64_t amount_paise;
    int64_t balance_after_paise;
    char category[64];
    char description[MAX_STR_LEN];
    TransactionStatus status;
    char flag_reason[MAX_STR_LEN];
    char created_at[32];
} Transaction;

// Beneficiary Model
typedef struct Beneficiary {
    int id;
    int customer_id;
    char beneficiary_name[128];
    char account_number[24];
    char bank_name[128];
    char ifsc_code[24];
    char email[128];
    char nickname[64];
    char created_at[32];
} Beneficiary;

// Savings Goal Model
typedef struct SavingsGoal {
    int id;
    int customer_id;
    int account_id;
    char title[128];
    int64_t target_amount_paise;
    int64_t current_amount_paise;
    char target_date[16];
    char category[64];
    char status[32];
    char created_at[32];
} SavingsGoal;

// Virtual Card Model
typedef struct VirtualCard {
    int id;
    int customer_id;
    int account_id;
    char card_number_masked[32];
    char card_type[16];
    int expiry_month;
    int expiry_year;
    int64_t spending_limit_paise;
    char status[16];
    bool is_simulated;
    char created_at[32];
} VirtualCard;

// Service Request Model
typedef struct ServiceRequest {
    int id;
    char ticket_number[36];
    int customer_id;
    int account_id;
    char customer_name[128];
    char request_type[32];
    char description[MAX_TEXT_LEN];
    RequestPriority priority;
    ServiceRequestStatus status;
    char admin_notes[MAX_TEXT_LEN];
    int resolved_by_admin_id;
    char created_at[32];
    char updated_at[32];
} ServiceRequest;

// Session Model
typedef struct Session {
    int id;
    char session_token[MAX_TOKEN_LEN];
    char user_type[16]; // CUSTOMER or ADMIN
    int user_id;
    char role[32];
    char csrf_token[64];
    char ip_address[48];
    char user_agent[MAX_STR_LEN];
    char expires_at[32];
} Session;

// Notification Model
typedef struct Notification {
    int id;
    int customer_id;
    char title[160];
    char message[MAX_TEXT_LEN];
    char type[32];
    bool is_read;
    char created_at[32];
} Notification;

#endif // MODELS_H

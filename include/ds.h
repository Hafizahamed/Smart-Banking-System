#ifndef DS_H
#define DS_H

#include "models.h"
#include <stdbool.h>
#include <stddef.h>

// ==============================================================================
// 1. LINKED LIST: Customer in-memory cache & traversal
// ==============================================================================
typedef struct CustomerNode {
    Customer data;
    struct CustomerNode *next;
} CustomerNode;

typedef struct {
    CustomerNode *head;
    int size;
} CustomerList;

CustomerList* customer_list_create(void);
void customer_list_insert(CustomerList *list, const Customer *customer);
Customer* customer_list_find_by_id(CustomerList *list, int id);
Customer* customer_list_find_by_email(CustomerList *list, const char *email);
void customer_list_free(CustomerList *list);

// ==============================================================================
// 2. QUEUE: FIFO Service Request Processing Queue
// ==============================================================================
typedef struct SRQueueNode {
    ServiceRequest data;
    struct SRQueueNode *next;
} SRQueueNode;

typedef struct {
    SRQueueNode *front;
    SRQueueNode *rear;
    int size;
} ServiceRequestQueue;

ServiceRequestQueue* sr_queue_create(void);
bool sr_queue_enqueue(ServiceRequestQueue *q, const ServiceRequest *req);
bool sr_queue_dequeue(ServiceRequestQueue *q, ServiceRequest *out_req);
ServiceRequest* sr_queue_peek(ServiceRequestQueue *q);
int sr_queue_size(ServiceRequestQueue *q);
void sr_queue_free(ServiceRequestQueue *q);

// ==============================================================================
// 3. STACK: LIFO Banking Operations History / Undo Audit Trail
// ==============================================================================
typedef struct {
    char action[64];
    char reference_id[64];
    int account_id;
    int64_t amount_paise;
    char timestamp[32];
} OperationEntry;

typedef struct OpStackNode {
    OperationEntry data;
    struct OpStackNode *next;
} OpStackNode;

typedef struct {
    OpStackNode *top;
    int size;
} OperationStack;

OperationStack* op_stack_create(void);
bool op_stack_push(OperationStack *s, const OperationEntry *entry);
bool op_stack_pop(OperationStack *s, OperationEntry *out_entry);
OperationEntry* op_stack_peek(OperationStack *s);
int op_stack_size(OperationStack *s);
void op_stack_free(OperationStack *s);

// ==============================================================================
// 4. SEARCHING ALGORITHMS
// ==============================================================================
// Linear Search: O(n)
int linear_search_customers_by_number(Customer arr[], int n, const char *cust_number);

// Binary Search: O(log n) on sorted array of accounts
int binary_search_accounts_by_number(Account arr[], int n, const char *acc_number);

// ==============================================================================
// 5. SORTING ALGORITHMS
// ==============================================================================
// Quick Sort: O(n log n) average for transactions by amount (descending or ascending)
void quick_sort_transactions(Transaction arr[], int low, int high, bool ascending);

// Merge Sort: O(n log n) stable sort for customers by last name / first name
void merge_sort_customers(Customer arr[], int l, int r);

// Benchmark & Demonstration helper (generates JSON report)
char* ds_run_demonstration_report(void);

#endif // DS_H

#include "ds.h"
#include "json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==============================================================================
// 1. LINKED LIST IMPLEMENTATION
// ==============================================================================
CustomerList* customer_list_create(void) {
    CustomerList *list = (CustomerList*)malloc(sizeof(CustomerList));
    if (!list) return NULL;
    list->head = NULL;
    list->size = 0;
    return list;
}

void customer_list_insert(CustomerList *list, const Customer *customer) {
    if (!list || !customer) return;
    CustomerNode *node = (CustomerNode*)malloc(sizeof(CustomerNode));
    if (!node) return;
    node->data = *customer;
    node->next = list->head;
    list->head = node;
    list->size++;
}

Customer* customer_list_find_by_id(CustomerList *list, int id) {
    if (!list) return NULL;
    CustomerNode *cur = list->head;
    while (cur) {
        if (cur->data.id == id) {
            return &cur->data;
        }
        cur = cur->next;
    }
    return NULL;
}

Customer* customer_list_find_by_email(CustomerList *list, const char *email) {
    if (!list || !email) return NULL;
    CustomerNode *cur = list->head;
    while (cur) {
        if (_stricmp(cur->data.email, email) == 0) {
            return &cur->data;
        }
        cur = cur->next;
    }
    return NULL;
}

void customer_list_free(CustomerList *list) {
    if (!list) return;
    CustomerNode *cur = list->head;
    while (cur) {
        CustomerNode *temp = cur;
        cur = cur->next;
        free(temp);
    }
    free(list);
}

// ==============================================================================
// 2. QUEUE IMPLEMENTATION
// ==============================================================================
ServiceRequestQueue* sr_queue_create(void) {
    ServiceRequestQueue *q = (ServiceRequestQueue*)malloc(sizeof(ServiceRequestQueue));
    if (!q) return NULL;
    q->front = NULL;
    q->rear = NULL;
    q->size = 0;
    return q;
}

bool sr_queue_enqueue(ServiceRequestQueue *q, const ServiceRequest *req) {
    if (!q || !req) return false;
    SRQueueNode *node = (SRQueueNode*)malloc(sizeof(SRQueueNode));
    if (!node) return false;
    node->data = *req;
    node->next = NULL;
    if (q->rear == NULL) {
        q->front = q->rear = node;
    } else {
        q->rear->next = node;
        q->rear = node;
    }
    q->size++;
    return true;
}

bool sr_queue_dequeue(ServiceRequestQueue *q, ServiceRequest *out_req) {
    if (!q || !q->front) return false;
    SRQueueNode *temp = q->front;
    if (out_req) {
        *out_req = temp->data;
    }
    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }
    free(temp);
    q->size--;
    return true;
}

ServiceRequest* sr_queue_peek(ServiceRequestQueue *q) {
    if (!q || !q->front) return NULL;
    return &q->front->data;
}

int sr_queue_size(ServiceRequestQueue *q) {
    return q ? q->size : 0;
}

void sr_queue_free(ServiceRequestQueue *q) {
    if (!q) return;
    SRQueueNode *cur = q->front;
    while (cur) {
        SRQueueNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    free(q);
}

// ==============================================================================
// 3. STACK IMPLEMENTATION
// ==============================================================================
OperationStack* op_stack_create(void) {
    OperationStack *s = (OperationStack*)malloc(sizeof(OperationStack));
    if (!s) return NULL;
    s->top = NULL;
    s->size = 0;
    return s;
}

bool op_stack_push(OperationStack *s, const OperationEntry *entry) {
    if (!s || !entry) return false;
    OpStackNode *node = (OpStackNode*)malloc(sizeof(OpStackNode));
    if (!node) return false;
    node->data = *entry;
    node->next = s->top;
    s->top = node;
    s->size++;
    return true;
}

bool op_stack_pop(OperationStack *s, OperationEntry *out_entry) {
    if (!s || !s->top) return false;
    OpStackNode *temp = s->top;
    if (out_entry) {
        *out_entry = temp->data;
    }
    s->top = s->top->next;
    free(temp);
    s->size--;
    return true;
}

OperationEntry* op_stack_peek(OperationStack *s) {
    if (!s || !s->top) return NULL;
    return &s->top->data;
}

int op_stack_size(OperationStack *s) {
    return s ? s->size : 0;
}

void op_stack_free(OperationStack *s) {
    if (!s) return;
    OpStackNode *cur = s->top;
    while (cur) {
        OpStackNode *tmp = cur;
        cur = cur->next;
        free(tmp);
    }
    free(s);
}

// ==============================================================================
// 4. SEARCHING ALGORITHMS
// ==============================================================================
int linear_search_customers_by_number(Customer arr[], int n, const char *cust_number) {
    if (!arr || !cust_number) return -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(arr[i].customer_number, cust_number) == 0) {
            return i;
        }
    }
    return -1;
}

int binary_search_accounts_by_number(Account arr[], int n, const char *acc_number) {
    if (!arr || !acc_number) return -1;
    int low = 0, high = n - 1;
    while (low <= high) {
        int mid = low + (high - low) / 2;
        int cmp = strcmp(arr[mid].account_number, acc_number);
        if (cmp == 0) {
            return mid;
        } else if (cmp < 0) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return -1;
}

// ==============================================================================
// 5. SORTING ALGORITHMS
// ==============================================================================
static int partition(Transaction arr[], int low, int high, bool ascending) {
    int64_t pivot = arr[high].amount_paise;
    int i = (low - 1);
    for (int j = low; j <= high - 1; j++) {
        bool condition = ascending ? (arr[j].amount_paise < pivot) : (arr[j].amount_paise > pivot);
        if (condition) {
            i++;
            Transaction temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }
    Transaction temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;
    return (i + 1);
}

void quick_sort_transactions(Transaction arr[], int low, int high, bool ascending) {
    if (low < high) {
        int pi = partition(arr, low, high, ascending);
        quick_sort_transactions(arr, low, pi - 1, ascending);
        quick_sort_transactions(arr, pi + 1, high, ascending);
    }
}

static void merge(Customer arr[], int l, int m, int r) {
    int n1 = m - l + 1;
    int n2 = r - m;

    Customer *L = (Customer*)malloc(n1 * sizeof(Customer));
    Customer *R = (Customer*)malloc(n2 * sizeof(Customer));

    for (int i = 0; i < n1; i++) L[i] = arr[l + i];
    for (int j = 0; j < n2; j++) R[j] = arr[m + 1 + j];

    int i = 0, j = 0, k = l;
    while (i < n1 && j < n2) {
        if (strcmp(L[i].last_name, R[j].last_name) <= 0) {
            arr[k] = L[i];
            i++;
        } else {
            arr[k] = R[j];
            j++;
        }
        k++;
    }

    while (i < n1) {
        arr[k] = L[i];
        i++;
        k++;
    }
    while (j < n2) {
        arr[k] = R[j];
        j++;
        k++;
    }

    free(L);
    free(R);
}

void merge_sort_customers(Customer arr[], int l, int r) {
    if (l < r) {
        int m = l + (r - l) / 2;
        merge_sort_customers(arr, l, m);
        merge_sort_customers(arr, m + 1, r);
        merge(arr, l, m, r);
    }
}

char* ds_run_demonstration_report(void) {
    JsonBuilder *jb = jb_create(2048);
    jb_append(jb, "{\n");
    jb_append(jb, "  \"status\": \"success\",\n");
    jb_append(jb, "  \"title\": \"C Core Data Structures & Algorithms Benchmark\",\n");

    // 1. Linked List Demo
    CustomerList *cl = customer_list_create();
    Customer c1 = { .id = 1, .first_name = "Rajesh", .last_name = "Sharma", .email = "rajesh@example.com" };
    Customer c2 = { .id = 2, .first_name = "Priya", .last_name = "Patel", .email = "priya@example.com" };
    Customer c3 = { .id = 3, .first_name = "Amit", .last_name = "Kumar", .email = "amit@example.com" };
    customer_list_insert(cl, &c1);
    customer_list_insert(cl, &c2);
    customer_list_insert(cl, &c3);
    Customer *found_cust = customer_list_find_by_email(cl, "priya@example.com");

    jb_append_format(jb, "  \"linked_list\": {\n    \"type\": \"Singly Linked List\",\n    \"purpose\": \"In-memory customer traversal & O(1) prepend cache\",\n    \"node_count\": %d,\n    \"lookup_test\": {\"email\": \"priya@example.com\", \"found\": %s, \"name\": \"%s %s\"}\n  },\n",
        cl->size, found_cust ? "true" : "false",
        found_cust ? found_cust->first_name : "",
        found_cust ? found_cust->last_name : "");
    customer_list_free(cl);

    // 2. Queue Demo
    ServiceRequestQueue *srq = sr_queue_create();
    ServiceRequest r1 = { .id = 101, .ticket_number = "SR-DEMO-1", .priority = PRIORITY_MEDIUM };
    ServiceRequest r2 = { .id = 102, .ticket_number = "SR-DEMO-2", .priority = PRIORITY_HIGH };
    sr_queue_enqueue(srq, &r1);
    sr_queue_enqueue(srq, &r2);
    ServiceRequest dequeued;
    sr_queue_dequeue(srq, &dequeued);

    jb_append_format(jb, "  \"queue\": {\n    \"type\": \"FIFO Queue (Linked)\",\n    \"purpose\": \"Fair-order Service Request processing pipeline\",\n    \"enqueued\": 2,\n    \"dequeued_ticket\": \"%s\",\n    \"remaining_in_queue\": %d\n  },\n",
        dequeued.ticket_number, srq->size);
    sr_queue_free(srq);

    // 3. Stack Demo
    OperationStack *stk = op_stack_create();
    OperationEntry op1 = { .account_id = 1, .amount_paise = 500000 };
    strcpy(op1.action, "DEPOSIT");
    strcpy(op1.reference_id, "TXN-OP-1");
    OperationEntry op2 = { .account_id = 1, .amount_paise = 150000 };
    strcpy(op2.action, "WITHDRAWAL");
    strcpy(op2.reference_id, "TXN-OP-2");
    op_stack_push(stk, &op1);
    op_stack_push(stk, &op2);
    OperationEntry popped;
    op_stack_pop(stk, &popped);

    jb_append_format(jb, "  \"stack\": {\n    \"type\": \"LIFO Stack (Linked)\",\n    \"purpose\": \"Transaction undo stack & live audit trail\",\n    \"pushed\": 2,\n    \"popped_action\": \"%s\",\n    \"popped_ref\": \"%s\",\n    \"stack_depth\": %d\n  },\n",
        popped.action, popped.reference_id, stk->size);
    op_stack_free(stk);

    // 4. Searching Demo
    Account sorted_accounts[3] = {
        { .id = 1, .account_number = "100100200301" },
        { .id = 2, .account_number = "100100200302" },
        { .id = 3, .account_number = "100100200401" }
    };
    int bsearch_idx = binary_search_accounts_by_number(sorted_accounts, 3, "100100200302");
    jb_append_format(jb, "  \"searching\": {\n    \"algorithm\": \"Binary Search\",\n    \"complexity\": \"O(log n)\",\n    \"target\": \"100100200302\",\n    \"found_at_index\": %d\n  },\n",
        bsearch_idx);

    // 5. Sorting Demo
    Transaction txs[3] = {
        { .id = 1, .amount_paise = 500000 },
        { .id = 2, .amount_paise = 1500000 },
        { .id = 3, .amount_paise = 25000 }
    };
    quick_sort_transactions(txs, 0, 2, false); // Descending
    jb_append_format(jb, "  \"sorting\": {\n    \"algorithm\": \"Quick Sort\",\n    \"complexity\": \"O(n log n)\",\n    \"sorted_highest_paise\": %lld,\n    \"sorted_lowest_paise\": %lld\n  }\n",
        (long long)txs[0].amount_paise, (long long)txs[2].amount_paise);

    jb_append(jb, "}");
    char *result = _strdup(jb_str(jb));
    jb_free(jb);
    return result;
}

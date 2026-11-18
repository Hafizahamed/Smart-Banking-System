#ifndef ADMIN_H
#define ADMIN_H

#include "models.h"
#include <stdbool.h>

// Admin Overview Metrics
char* admin_get_overview_json(void);

// Customer & Account Administration
char* admin_get_customers_json(const char *search_query, int limit, int offset);
bool admin_update_customer_status(int admin_id, const char *admin_role, int target_customer_id, const char *new_status, const char *reason);
bool admin_update_account_status(int admin_id, const char *admin_role, int target_account_id, const char *new_status, const char *reason);

// Transaction Monitoring (including flagged suspicious transfers)
char* admin_get_transactions_json(bool only_flagged, int limit, int offset);

// Service Request Processing (Queue workflow)
char* admin_get_service_requests_json(const char *status_filter);
bool admin_process_service_request(int admin_id, const char *admin_role, int ticket_id, const char *decision, const char *admin_notes);

// Audit Logs & System Health
char* admin_get_audit_logs_json(int limit, int offset);
char* admin_get_system_health_json(void);

#endif // ADMIN_H

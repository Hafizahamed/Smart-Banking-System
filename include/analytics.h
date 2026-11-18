#ifndef ANALYTICS_H
#define ANALYTICS_H

#include "models.h"
#include <stdbool.h>
#include <stdint.h>

// Customer Financial Dashboard & Analytics Overview
char* analytics_get_summary_json(int customer_id);

// Savings Goals & Contributions
char* analytics_get_savings_goals_json(int customer_id);
bool analytics_create_savings_goal(int customer_id, int account_id, const char *title,
                                   int64_t target_paise, const char *target_date, const char *category);
bool analytics_contribute_to_goal(int customer_id, int goal_id, int64_t amount_paise, const char *note,
                                  char *err_msg, size_t err_len);

// AI Financial Assistant (Deterministic analysis engine with real financial data insights)
char* analytics_ai_financial_advisor(int customer_id, const char *user_prompt);

#endif // ANALYTICS_H

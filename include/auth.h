#ifndef AUTH_H
#define AUTH_H

#include "models.h"
#include <stdbool.h>
#include <stddef.h>

// Argon2id Hashing & Verification
bool auth_hash_password(const char *password, char *out_hash, size_t out_len);
bool auth_verify_password(const char *password, const char *hash);

// Transaction PIN Management
bool auth_set_pin(int customer_id, const char *pin);
bool auth_verify_pin(int customer_id, const char *pin);
bool auth_has_pin(int customer_id);

// Cryptographically secure token generator (hex)
void auth_generate_token(char *out, size_t len);

// Session Management
bool auth_create_session(const char *user_type, int user_id, const char *role,
                         const char *ip, const char *user_agent, Session *out_sess);
bool auth_validate_session(const char *token, Session *out_sess);
bool auth_destroy_session(const char *token);

// CSRF Validation
bool auth_validate_csrf(const Session *sess, const char *csrf_token);

// RBAC Permission Check
bool auth_check_permission(const char *user_role, const char *required_role);

// Login History Tracker
void auth_record_login(const char *user_type, int user_id, const char *ip,
                       const char *user_agent, bool success, const char *reason);

#endif // AUTH_H

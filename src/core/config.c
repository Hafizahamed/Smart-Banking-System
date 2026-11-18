#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

AppConfig g_config;

static void trim(char *s) {
    char *p = s;
    while (isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

void config_init(const char *env_path) {
    // Defaults
    strncpy(g_config.host, "127.0.0.1", sizeof(g_config.host) - 1);
    g_config.port = 8080;
    g_config.debug = true;

    strncpy(g_config.db_host, "127.0.0.1", sizeof(g_config.db_host) - 1);
    g_config.db_port = 3306;
    strncpy(g_config.db_user, "root", sizeof(g_config.db_user) - 1);
    g_config.db_pass[0] = '\0';
    strncpy(g_config.db_name, "smart_banking", sizeof(g_config.db_name) - 1);

    strncpy(g_config.session_secret, "smart_banking_default_sec_2026", sizeof(g_config.session_secret) - 1);
    g_config.session_expiry_hours = 24;
    strncpy(g_config.csrf_secret, "smart_banking_csrf_key_2026", sizeof(g_config.csrf_secret) - 1);
    g_config.ai_api_key[0] = '\0';

    FILE *f = fopen(env_path ? env_path : ".env", "r");
    if (!f) {
        printf("[CONFIG] .env file not found, using defaults / environment variables\n");
    } else {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            trim(line);
            if (line[0] == '#' || line[0] == '\0') continue;
            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *key = line;
            char *val = eq + 1;
            trim(key);
            trim(val);

            // Strip quotes if present
            size_t vlen = strlen(val);
            if (vlen >= 2 && ((val[0] == '"' && val[vlen - 1] == '"') || (val[0] == '\'' && val[vlen - 1] == '\''))) {
                val[vlen - 1] = '\0';
                val++;
            }

            if (strcmp(key, "HOST") == 0) strncpy(g_config.host, val, sizeof(g_config.host) - 1);
            else if (strcmp(key, "PORT") == 0) g_config.port = atoi(val);
            else if (strcmp(key, "DB_HOST") == 0) strncpy(g_config.db_host, val, sizeof(g_config.db_host) - 1);
            else if (strcmp(key, "DB_PORT") == 0) g_config.db_port = atoi(val);
            else if (strcmp(key, "DB_USER") == 0) strncpy(g_config.db_user, val, sizeof(g_config.db_user) - 1);
            else if (strcmp(key, "DB_PASSWORD") == 0) strncpy(g_config.db_pass, val, sizeof(g_config.db_pass) - 1);
            else if (strcmp(key, "DB_NAME") == 0) strncpy(g_config.db_name, val, sizeof(g_config.db_name) - 1);
            else if (strcmp(key, "SESSION_SECRET") == 0) strncpy(g_config.session_secret, val, sizeof(g_config.session_secret) - 1);
            else if (strcmp(key, "SESSION_EXPIRY_HOURS") == 0) g_config.session_expiry_hours = atoi(val);
            else if (strcmp(key, "CSRF_SECRET") == 0) strncpy(g_config.csrf_secret, val, sizeof(g_config.csrf_secret) - 1);
            else if (strcmp(key, "AI_API_KEY") == 0) strncpy(g_config.ai_api_key, val, sizeof(g_config.ai_api_key) - 1);
        }
        fclose(f);
    }
}

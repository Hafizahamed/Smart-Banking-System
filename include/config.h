#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>

typedef struct {
    char host[64];
    int port;
    bool debug;

    char db_host[64];
    int db_port;
    char db_user[64];
    char db_pass[128];
    char db_name[64];

    char session_secret[128];
    int session_expiry_hours;
    char csrf_secret[128];

    char ai_api_key[128];
} AppConfig;

extern AppConfig g_config;

void config_init(const char *env_path);
const char* config_get(const char *key, const char *default_val);

#endif // CONFIG_H

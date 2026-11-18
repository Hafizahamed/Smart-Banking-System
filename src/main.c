#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <mongoose.h>

#include "config.h"
#include "db.h"
#include "routes.h"

static bool s_running = true;

static void signal_handler(int sig) {
    (void)sig;
    s_running = false;
    printf("\n[SERVER] Shutdown signal received. Closing server gracefully...\n");
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    const char *env_file = (argc > 1) ? argv[1] : ".env";
    config_init(env_file);

    printf("====================================================================\n");
    printf("         SMART BANKING MANAGEMENT SYSTEM - CORE C SERVER           \n");
    printf("====================================================================\n");
    printf("[INIT] Starting banking core engine...\n");

    // Initialize Database
    if (!db_init()) {
        fprintf(stderr, "[ERROR] Could not connect to MySQL 8.0 database at %s:%d!\n", g_config.db_host, g_config.db_port);
        fprintf(stderr, "[HINT] Ensure MySQL 8.0 is running and credentials in .env are accurate.\n");
        // We will continue so static files and mock status can still be inspected, but log error
    }

    // Initialize Routes
    routes_init();

    // Initialize Mongoose Manager
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    char listen_addr[128];
    snprintf(listen_addr, sizeof(listen_addr), "http://%s:%d", g_config.host, g_config.port);

    struct mg_connection *c = mg_http_listen(&mgr, listen_addr, routes_handle_http, NULL);
    if (!c) {
        fprintf(stderr, "[FATAL] Failed to bind HTTP server to %s\n", listen_addr);
        db_close();
        mg_mgr_free(&mgr);
        return 1;
    }

    printf("[SERVER] Embedded Mongoose HTTP Server listening on %s\n", listen_addr);
    printf("[PORTAL] Customer Web Application: http://%s:%d/\n", g_config.host, g_config.port);
    printf("[PORTAL] Administrator Portal:     http://%s:%d/admin\n", g_config.host, g_config.port);
    printf("[CRYPTO] Argon2id (RFC 9106) Memory-Hard Hashing active\n");
    printf("[ACID]   MySQL 8.0 InnoDB Integer Paise Monetary Accounting\n");
    printf("--------------------------------------------------------------------\n");
    printf("System ready. Press Ctrl+C to terminate.\n");
    printf("====================================================================\n");

    while (s_running) {
        mg_mgr_poll(&mgr, 100);
    }

    printf("[SERVER] Cleaning up resources...\n");
    mg_mgr_free(&mgr);
    db_close();
    printf("[SERVER] Smart Banking System halted cleanly.\n");
    return 0;
}

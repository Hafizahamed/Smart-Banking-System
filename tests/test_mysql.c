#include <stdio.h>
#include <stdlib.h>
#include <mariadb/mysql.h>

int main(void) {
    const char *host = getenv("BANK_DB_HOST");
    const char *user = getenv("BANK_DB_USER");
    const char *pass = getenv("BANK_DB_PASS");

    if (!host || !user || !pass) {
        fprintf(stderr, "Set BANK_DB_HOST, BANK_DB_USER, BANK_DB_PASS first.\n");
        return 1;
    }

    MYSQL *conn = mysql_init(NULL);
    if (!conn) {
        fprintf(stderr, "mysql_init failed\n");
        return 1;
    }

    if (!mysql_real_connect(conn, host, user, pass, NULL, 3306, NULL, 0)) {
        fprintf(stderr, "Connection failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return 1;
    }

    printf("Connected! Server version: %s\n", mysql_get_server_info(conn));
    mysql_close(conn);
    return 0;
}
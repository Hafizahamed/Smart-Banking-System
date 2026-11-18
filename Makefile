<<<<<<< HEAD
# ==============================================================================
# SMART BANKING MANAGEMENT SYSTEM - MAKEFILE (Windows / MinGW-w64 UCRT64)
# ==============================================================================

CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude -I"C:/Program Files/MySQL/MySQL Server 8.0/include"
LDFLAGS = -L"C:/Program Files/MySQL/MySQL Server 8.0/lib" -L. -lmongoose -largon2 -llibmysql -lws2_32

SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin
TARGET = banking_server.exe

SRCS = $(SRC_DIR)/core/config.c \
       $(SRC_DIR)/core/ds.c \
       $(SRC_DIR)/core/json_utils.c \
       $(SRC_DIR)/database/db.c \
       $(SRC_DIR)/security/auth.c \
       $(SRC_DIR)/banking/banking.c \
       $(SRC_DIR)/banking/analytics.c \
       $(SRC_DIR)/admin/admin.c \
       $(SRC_DIR)/web/routes.c \
       $(SRC_DIR)/main.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

all: directories $(TARGET) copy_dll

directories:
	@powershell -Command "New-Item -ItemType Directory -Force -Path build/core, build/database, build/security, build/banking, build/admin, build/web, bin | Out-Null"

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@
	@echo [BUILD SUCCESS] Compiled $(TARGET)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

copy_dll:
	@powershell -Command "if (!(Test-Path libmysql.dll)) { Copy-Item 'C:/Program Files/MySQL/MySQL Server 8.0/lib/libmysql.dll' . }"

clean:
	@powershell -Command "Remove-Item -Recurse -Force build, $(TARGET) -ErrorAction SilentlyContinue"
	@echo [CLEAN] Removed build artifacts

test:
	$(CC) $(CFLAGS) tests/test_banking.c $(SRC_DIR)/core/config.c $(SRC_DIR)/core/ds.c $(SRC_DIR)/core/json_utils.c $(SRC_DIR)/database/db.c $(SRC_DIR)/security/auth.c $(SRC_DIR)/banking/banking.c $(SRC_DIR)/banking/analytics.c $(SRC_DIR)/admin/admin.c $(LDFLAGS) -o test_runner.exe
	./test_runner.exe
	@powershell -Command "Remove-Item test_runner.exe -ErrorAction SilentlyContinue"

.PHONY: all clean directories copy_dll test
=======
CC      = gcc
CFLAGS  = -Wall -Wextra -g -Iinclude
LDFLAGS = -lmariadb

SRC = $(wildcard src/*.c)
BIN = build/bank.exe

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

run: all
	$(BIN)

# Stage 1 environment checks
check-c:
	$(CC) $(CFLAGS) tests/test_env.c -o build/test_env.exe
	build/test_env.exe

check-db:
	$(CC) $(CFLAGS) tests/test_mysql.c -o build/test_mysql.exe $(LDFLAGS)
	build/test_mysql.exe

clean:
	del /Q build\*.exe 2>nul
>>>>>>> f4a6c70253e54e5b0edbdafc152f4de53d082ad4

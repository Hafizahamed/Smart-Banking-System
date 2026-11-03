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
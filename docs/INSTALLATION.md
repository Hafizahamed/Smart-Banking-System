# Smart Banking System - Installation & Setup Guide

## Prerequisites
1. **Windows 11** (64-bit).
2. **MinGW-w64 UCRT64** (GCC 16.x or GCC 13.x+).
3. **MySQL 8.0 Server** installed (e.g. `C:\Program Files\MySQL\MySQL Server 8.0`).
4. **Mongoose HTTP library** and **Argon2 library** in MinGW UCRT64 (`libmongoose.a`, `libargon2.a`).

---

## 1. Setting up MySQL 8.0 Database

Start MySQL Server and initialize the database schema and sample seed records:

```powershell
# Open PowerShell in the project directory
cd d:\Smart-Banking-System

# Run schema creation
& "C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe" -u root smart_banking -e "SOURCE database/schema.sql;"

# Seed realistic demo data (Customers, Accounts, Transactions, Admins, Cards, Goals)
& "C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe" -u root smart_banking -e "SOURCE database/seed.sql;"
```

---

## 2. Configuration (`.env`)

Copy `.env.example` to `.env` or verify the defaults:

```ini
PORT=8080
HOST=127.0.0.1
DEBUG=true

DB_HOST=127.0.0.1
DB_PORT=3306
DB_USER=root
DB_PASSWORD=
DB_NAME=smart_banking

SESSION_SECRET=dev_secret_banking_system_2026_antigravity
SESSION_EXPIRY_HOURS=24
CSRF_SECRET=dev_csrf_secret_banking_system_2026
```

---

## 3. Building the C Backend Application

Using `mingw32-make` or `gcc`:

```powershell
# Compile banking_server.exe
mingw32-make

# Or compile manually:
gcc -Wall -Wextra -O2 -Iinclude -I"C:/Program Files/MySQL/MySQL Server 8.0/include" src/core/*.c src/database/*.c src/security/*.c src/banking/*.c src/admin/*.c src/web/*.c src/main.c -L"C:/Program Files/MySQL/MySQL Server 8.0/lib" -L. -lmongoose -largon2 -llibmysql -lws2_32 -o banking_server.exe
```

---

## 4. Running the Automated Test Suite

Run the isolated test runner against `smart_banking_test`:

```powershell
mingw32-make test
```
*Expected Result: 31 of 31 tests passed (100.0%).*

---

## 5. Starting the Server

```powershell
.\banking_server.exe
```

Open your browser to:
- **Customer Application**: `http://127.0.0.1:8080/`
- **Administrative Portal**: `http://127.0.0.1:8080/admin`

---

## 6. Default Demo Credentials

### Customer Account
- **Email**: `rajesh@example.com`
- **Password**: `Password@123`
- **Transaction PIN**: `123456`

### Administrator Account
- **Username**: `superadmin`
- **Password**: `Admin@123`
- **Role**: `SUPER_ADMIN`

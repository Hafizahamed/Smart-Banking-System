# Smart Banking Management System
### Full-Stack Digital Banking Application Built in C and MySQL 8.0

A modern, production-grade fintech banking simulation developed from scratch with an embedded C backend, normalized MySQL 8.0 database, and a premium glassmorphic web interface.

---

## Key Highlights

- **Pure C Backend**: Core banking logic, transaction processing, and data structures implemented in C99.
- **Embedded Web Server**: High-concurrency, asynchronous HTTP request handling via **Mongoose 7.23**.
- **ACID Database Transactions**: MySQL 8.0 InnoDB engine with explicit `START TRANSACTION`, `COMMIT`, and `ROLLBACK` guarantees.
- **Integer Paise Arithmetic**: All balances and operations are computed internally in integer **paise** ($1\text{ INR} = 100\text{ paise}$), completely eliminating floating-point rounding errors.
- **Indian Number Formatting**: Native formatting for Lakhs and Crores (e.g. `₹2,54,320.50`).
- **Military-Grade Cryptography**: **Argon2id (RFC 9106)** memory-hard password and transaction PIN hashing.
- **Academic C Data Structures**: Concrete implementations of Singly Linked List, FIFO Queue, LIFO Stack, Binary Search ($O(\log n)$), and Quick Sort ($O(n \log n)$).
- **Rule-Based Fraud Detection**: Real-time AML rules flagging transfers $> \text{₹}1,00,000.00$.
- **AI Financial Advisor**: Built-in deterministic intelligence engine analyzing user cashflow, top spending categories, and savings velocity.
- **Dual User Portals**: Complete Customer Banking Application (18 views) and dedicated Administrative Command Center with RBAC (`SUPER_ADMIN`, `ADMIN`, `SUPPORT`, `AUDITOR`).

---

## Project Structure

```text
Smart-Banking-System/
├── Makefile                    # MinGW-w64 UCRT64 build script
├── .env.example                # Configuration template
├── .gitignore                  # Git ignore rules
├── README.md                   # Project documentation
├── include/
│   ├── config.h                # Environment and app configuration
│   ├── models.h                # C banking data models
│   ├── db.h                    # MySQL 8.0 database abstraction
│   ├── ds.h                    # College Data Structures & Algorithms
│   ├── auth.h                  # Argon2id hashing & session management
│   ├── banking.h               # Core accounts, deposits, withdrawals, transfers
│   ├── analytics.h             # Financial intelligence & AI advisor
│   ├── admin.h                 # Administrative controls & audit logs
│   ├── json_utils.h            # JSON parser & Indian currency formatter
│   └── routes.h                # Mongoose HTTP route dispatcher
├── src/
│   ├── core/                   # Config, DS algorithms, JSON utilities
│   ├── database/               # MySQL client integration
│   ├── security/               # Argon2id crypto & session security
│   ├── banking/                # ACID transactions & analytics engine
│   ├── admin/                  # RBAC admin operations
│   ├── web/                    # HTTP REST API & static file serving
│   └── main.c                  # Server entrypoint & lifecycle
├── database/
│   ├── schema.sql              # Normalized MySQL 8.0 schema
│   ├── seed.sql                # Authentic sample data with Argon2id hashes
│   ├── README.md               # Database architecture guide
│   └── migrations/             # Incremental migration files
├── web/
│   ├── index.html              # Customer Banking Portal (SPA)
│   ├── admin.html              # Dedicated Administrative Control Center
│   ├── css/                    # Main, Components, and Admin stylesheets
│   ├── js/                     # API client, state, UI, charts, app controllers
│   └── assets/                 # SVGs and logos
├── tests/
│   └── test_banking.c          # Automated test suite (31 comprehensive tests)
└── docs/
    ├── ARCHITECTURE.md         # System architecture specification
    ├── API.md                  # Complete REST endpoint reference
    ├── INSTALLATION.md         # Setup and compilation instructions
    └── FEATURES.md             # Feature matrix and completion status
```

---

## Quick Start (Windows 11)

### 1. Database Setup
Ensure MySQL 8.0 is running on port 3306:
```powershell
# Import Schema and Seed Data
& "C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe" -u root smart_banking -e "SOURCE database/schema.sql; SOURCE database/seed.sql;"
```

### 2. Build the Application
```powershell
mingw32-make
```

### 3. Run Automated Tests
```powershell
mingw32-make test
```
*Executes all 31 automated test cases verifying database connectivity, Argon2id, ACID debits/credits, PIN checks, transfers, AML flags, freeze protections, RBAC, and C data structures.*

### 4. Start the Banking Server
```powershell
.\banking_server.exe
```

Access the applications in your browser:
- **Customer Portal**: `http://127.0.0.1:8080/`
- **Admin Portal**: `http://127.0.0.1:8080/admin`

---

## Default Demonstration Credentials

### Customer Portal (`http://127.0.0.1:8080/`)
| Email | Password | Transaction PIN | Accounts |
|---|---|---|---|
| `rajesh@example.com` | `Password@123` | `123456` | Savings (`100100200301`), Current (`100100200302`) |
| `priya@example.com` | `Password@123` | `123456` | Savings (`100100200401`) |
| `amit@example.com` | `Password@123` | `123456` | Savings (`100100200501`) |

### Administrator Portal (`http://127.0.0.1:8080/admin`)
| Username | Password | Role | Responsibilities |
|---|---|---|---|
| `superadmin` | `Admin@123` | `SUPER_ADMIN` | Full root system access and overrides |
| `admin` | `Admin@123` | `ADMIN` | Account management, customer status, AML review |
| `support` | `Admin@123` | `SUPPORT` | Customer care and service request processing |
| `auditor` | `Admin@123` | `AUDITOR` | Read-only compliance and audit log review |

---

## Academic Demonstration of C Data Structures

The application features dedicated in-memory data structures:
1. **Singly Linked List (`CustomerList`)**: Dynamic customer caching with $O(1)$ head insertion and linear search.
2. **FIFO Queue (`ServiceRequestQueue`)**: Fair-order service request processing pipeline.
3. **LIFO Stack (`OperationStack`)**: Real-time transaction undo and audit backtrack stack.
4. **Binary Search (`binary_search_accounts_by_number`)**: $O(\log n)$ search over sorted account arrays.
5. **Quick Sort (`quick_sort_transactions`)**: $O(n \log n)$ partition-based sorting of transactions by amount.
6. **Merge Sort (`merge_sort_customers`)**: $O(n \log n)$ stable sort for customer directories.

Execute the live benchmark from your browser by navigating to `http://127.0.0.1:8080/#ds-demo` or making a `GET /api/ds/demo` request.

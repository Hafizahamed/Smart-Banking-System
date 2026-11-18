# Smart Banking Management System - Architectural Specification

## 1. Executive Summary
The **Smart Banking Management System** is an enterprise-grade digital banking simulation built strictly with **C (core backend logic and algorithms)**, an embedded **Mongoose 7.23 HTTP event-driven server**, and a normalized **MySQL 8.0** relational database engine with ACID transaction isolation. The frontend is engineered with responsive, glassmorphic **HTML5, CSS3, Vanilla JavaScript, Chart.js, and Lucide icons**, adhering to modern fintech aesthetic standards.

---

## 2. Architectural Blueprint

```
+-------------------------------------------------------------------------+
|                        Modern Web Browser UI                            |
|    - Customer Portal (18 Responsive Views, Dark & Light Mode, SPA)      |
|    - Administrator Command Center (Role-Based Access Control)           |
|    - Interactive Charts (Chart.js), Modern Tables, Modals, Toasts       |
+------------------------------------+------------------------------------+
                                     |
                                     | REST JSON API / HTTP 1.1 (Port 8080)
                                     v
+------------------------------------+------------------------------------+
|               Embedded Mongoose C Server (Mongoose 7.23)               |
|    - Non-blocking I/O Event Loop (mg_mgr_poll)                          |
|    - Request Dispatcher & Static Asset Serving                          |
|    - HttpOnly Cookie, Session & Bearer Token Middleware                 |
+------------------------------------+------------------------------------+
                                     |
              +----------------------+----------------------+
              |                                             |
              v                                             v
+-------------+---------------+               +-------------+---------------+
|     Security & Auth         |               |   Academic C Data Structs   |
|  - Argon2id Password Hash   |               |  - Linked List (Customer)   |
|  - Argon2id Transaction PIN |               |  - FIFO Queue (Service Req) |
|  - Session Token Generator  |               |  - LIFO Stack (Undo Trace)  |
|  - RBAC Permission Guard    |               |  - Binary Search (Accounts) |
+-------------+---------------+               |  - Quick & Merge Sort       |
              |                               +-------------+---------------+
              v                                             |
+-------------+---------------+                             |
|      Core Banking Engine    |                             |
|  - Integer Paise Arithmetic |                             |
|  - ACID Debit/Credit Atomic |                             |
|  - AML High-Value Flagging  |                             |
|  - Deterministic AI Advisor |                             |
+-------------+---------------+                             |
              |                                             |
              +----------------------+----------------------+
                                     |
                                     | MySQL C Client API (libmysql.dll)
                                     v
+------------------------------------+------------------------------------+
|                    MySQL 8.0 Database (InnoDB)                         |
|  - Foreign Key Constraints with CASCADE / SET NULL                     |
|  - Atomic Transactions: START TRANSACTION -> COMMIT / ROLLBACK          |
|  - Full Audit Trail Logging for compliance                             |
+-------------------------------------------------------------------------+
```

---

## 3. Financial Precision & Integer Paise Calculations
Floating point variables (`float`, `double`) introduce catastrophic precision errors in monetary systems (e.g., `0.1 + 0.2 = 0.30000000000000004`).
To guarantee 100% mathematical precision:
- All balances, transactions, and savings targets are represented as `int64_t` in integer **paise** ($1\text{ INR} = 100\text{ paise}$).
- Currency formatting implements the authentic **Indian Numbering System** ($1,00,000.00$ Lakhs and Crores) via custom C formatting routines in `src/core/json_utils.c`.

---

## 4. Cryptographic Security Standards
1. **Argon2id (RFC 9106)**: Memory-hard hashing algorithm resistant to GPU and ASIC brute-force attacks:
   - Time Cost: $t = 2$
   - Memory Cost: $m = 65,536\text{ KiB}$ ($64\text{ MB}$)
   - Parallelism: $p = 1$
   - Unique static or cryptographic salt.
2. **Transaction PIN**: Dedicated 4-6 digit numeric PIN hashed with Argon2id required for all outgoing debits, withdrawals, and transfers.
3. **Session Management**: Cryptographically generated 64-hex-character session tokens stored server-side in the `sessions` table, transmitted via secure `HttpOnly; SameSite=Strict` cookies and `Authorization: Bearer` headers.
4. **Anti-Money Laundering (AML) Rules**: Rule-based transaction monitoring detects high-value single transfers ($> \text{₹}1,00,000.00$) and automatically marks their status as `FLAGGED` with rule diagnosis for administrator review.

---

## 5. Academic C Data Structures
This application demonstrates real-world implementations of foundational data structures:
1. **Singly Linked List (`CustomerList`)**:
   - In-memory traversal of cached customer profiles with $O(1)$ prepend insertion and $O(n)$ search.
2. **FIFO Queue (`ServiceRequestQueue`)**:
   - First-In-First-Out service request processing pipeline.
3. **LIFO Stack (`OperationStack`)**:
   - Last-In-First-Out transaction undo and audit backtrack stack.
4. **Binary Search (`binary_search_accounts_by_number`)**:
   - $O(\log n)$ search over sorted arrays of bank accounts.
5. **Quick Sort (`quick_sort_transactions`)**:
   - $O(n \log n)$ partition-based sorting of transactions by amount in descending or ascending order.
6. **Merge Sort (`merge_sort_customers`)**:
   - Stable $O(n \log n)$ divide-and-conquer sort for customer rosters by surname.
- The live performance and execution of these algorithms can be tested via the `/api/ds/demo` API endpoint.

# Smart Banking System - REST API Documentation

All API responses return JSON with status code `200` (or `201` for creation) on success, or `400`/`401`/`403`/`500` on error with `{"status":"error","message":"..."}`.

Authentication is passed via `Set-Cookie: session_token=...` or `Authorization: Bearer <session_token>`.

---

## 1. Authentication & Security Endpoints

### Register Customer
- **Endpoint**: `POST /api/auth/register`
- **Body**:
  ```json
  {
    "first_name": "Rahul",
    "last_name": "Sharma",
    "email": "rahul@example.com",
    "phone": "+919876543210",
    "password": "Password@123"
  }
  ```
- **Response**: `201 Created` with customer number, session token, and initial ₹10,000 demo account.

### Customer Login
- **Endpoint**: `POST /api/auth/login`
- **Body**: `{"email": "rajesh@example.com", "password": "Password@123"}`
- **Response**: `200 OK` with user details, `token`, and `csrf_token`.

### Admin Login
- **Endpoint**: `POST /api/auth/admin-login`
- **Body**: `{"username": "superadmin", "password": "Admin@123"}`
- **Response**: `200 OK` with role (`SUPER_ADMIN`, `ADMIN`, `SUPPORT`, `AUDITOR`) and token.

### Get Current Authenticated Profile
- **Endpoint**: `GET /api/auth/me`
- **Response**: `200 OK` with user details.

### Set Transaction PIN
- **Endpoint**: `POST /api/auth/set-pin`
- **Body**: `{"pin": "123456"}`

---

## 2. Core Banking Endpoints

### List Customer Accounts
- **Endpoint**: `GET /api/accounts`
- **Response**:
  ```json
  [
    {
      "id": 1,
      "account_number": "100100200301",
      "account_type": "SAVINGS",
      "balance_paise": 25432050,
      "balance_formatted": "₹2,54,320.50",
      "currency": "INR",
      "status": "ACTIVE",
      "branch_code": "SMRT0001001"
    }
  ]
  ```

### Deposit Funds (ACID)
- **Endpoint**: `POST /api/banking/deposit`
- **Body**:
  ```json
  {
    "account_id": 1,
    "amount_paise": 2500000,
    "description": "Salary Bonus Deposit",
    "category": "Salary"
  }
  ```

### Withdraw Funds (ACID + PIN Verified)
- **Endpoint**: `POST /api/banking/withdraw`
- **Body**:
  ```json
  {
    "account_id": 1,
    "amount_paise": 500000,
    "pin": "123456",
    "description": "ATM Cash Out",
    "category": "General"
  }
  ```

### Internal Transfer (ACID + AML Flagging)
- **Endpoint**: `POST /api/banking/transfer`
- **Body**:
  ```json
  {
    "from_account_id": 1,
    "to_account_number": "100100200401",
    "amount_paise": 1000000,
    "pin": "123456",
    "description": "Rent Payment",
    "category": "Transfer"
  }
  ```

### Transaction History & Filter
- **Endpoint**: `GET /api/banking/transactions?limit=50&type=ALL&category=ALL`

### Monthly Account Statement
- **Endpoint**: `GET /api/banking/statement?account_id=1&year=2026&month=11`

---

## 3. Financial Intelligence & AI Assistant

### Analytics Dashboard Summary
- **Endpoint**: `GET /api/analytics/summary`
- **Response**: Total balance, monthly income, monthly expenses, savings rate, category breakdown, 6-month trend.

### AI Financial Assistant (Deterministic)
- **Endpoint**: `POST /api/ai/assistant`
- **Body**: `{"prompt": "Where did I spend the most money this month?"}`
- **Response**:
  ```json
  {
    "status": "success",
    "engine": "Deterministic Rule-Based Intelligence Engine",
    "message": "Based on your latest account transactions, your primary outflow category is Transfer...",
    "actionable_tip": "Try establishing a weekly cap on Transfer to free up ₹2,500 every month."
  }
  ```

---

## 4. Academic C Data Structures Benchmark

### Execute Live In-Memory Benchmark
- **Endpoint**: `GET /api/ds/demo`
- **Response**: JSON report with execution results for Linked List, FIFO Queue, LIFO Stack, Binary Search ($O(\log n)$), and Quick Sort ($O(n \log n)$).

---

## 5. Administrative Control Endpoints (RBAC Enforced)

- `GET /api/admin/overview`: Consolidated KPI counts and vault deposits.
- `GET /api/admin/customers?q=search`: Customer roster and balances.
- `POST /api/admin/customers/status`: Update customer status (`ACTIVE`, `FROZEN`, `SUSPENDED`).
- `GET /api/admin/transactions?flagged=1`: Transaction monitoring with AML filter.
- `GET /api/admin/service-requests`: Queued customer support tickets.
- `POST /api/admin/service-requests/action`: Approve/reject ticket with officer notes.
- `GET /api/admin/audit-logs`: Immutable system audit ledger.
- `GET /api/admin/system-health`: Server and database diagnostics.

# Smart Banking System - Database Documentation

## Overview
The Smart Banking System utilizes a normalized MySQL 8.0 relational schema designed with ACID guarantees, foreign keys with appropriate cascade/set-null constraints, and indexing for banking scale.

## Key Design Principles
1. **Integer Arithmetic for Balances**: All currency values (`balance_paise`, `amount_paise`, `target_amount_paise`) are stored as `BIGINT` representing integer **paise** (1 INR = 100 paise). This completely eliminates floating-point rounding errors and precision loss.
2. **ACID Transaction Boundaries**: All debit, credit, and transfer operations execute inside explicit `START TRANSACTION` / `COMMIT` / `ROLLBACK` blocks.
3. **Audit Trail**: Every customer action, administrative approval, and system event creates an immutable row in `audit_logs`.
4. **Argon2id Hashing**: Passwords and 6-digit transaction PINs are stored exclusively as Argon2id hashes with dedicated salts.

## Applying the Schema and Seed Data

```powershell
# Apply schema
& "C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe" -u root < database/schema.sql

# Apply seed data
& "C:\Program Files\MySQL\MySQL Server 8.0\bin\mysql.exe" -u root < database/seed.sql
```

## Default Seed Credentials

### Customers (Password: `Password@123`, PIN: `123456`)
- `rajesh@example.com` (Account: 100100200301 Savings, 100100200302 Current)
- `priya@example.com` (Account: 100100200401 Savings)
- `amit@example.com` (Account: 100100200501 Savings)

### Administrators (Password: `Admin@123`)
- `superadmin` - Role: `SUPER_ADMIN`
- `admin` - Role: `ADMIN`
- `support` - Role: `SUPPORT`
- `auditor` - Role: `AUDITOR`

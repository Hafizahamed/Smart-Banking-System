-- ==============================================================================
-- SMART BANKING MANAGEMENT SYSTEM - DATABASE SCHEMA (MySQL 8.0)
-- Production-Ready, Normalized Banking Database with ACID Guarantees
-- All monetary balances stored in integer paise (1 INR = 100 paise)
-- ==============================================================================

-- Drop tables in reverse foreign-key order for idempotent re-runs
DROP TABLE IF EXISTS sessions;
DROP TABLE IF EXISTS audit_logs;
DROP TABLE IF EXISTS login_history;
DROP TABLE IF EXISTS notifications;
DROP TABLE IF EXISTS service_requests;
DROP TABLE IF EXISTS virtual_cards;
DROP TABLE IF EXISTS savings_contributions;
DROP TABLE IF EXISTS savings_goals;
DROP TABLE IF EXISTS beneficiaries;
DROP TABLE IF EXISTS transactions;
DROP TABLE IF EXISTS accounts;
DROP TABLE IF EXISTS admins;
DROP TABLE IF EXISTS transaction_pins;
DROP TABLE IF EXISTS customers;

-- 1. Customers Table
CREATE TABLE customers (
    id INT AUTO_INCREMENT PRIMARY KEY,
    customer_number VARCHAR(32) NOT NULL UNIQUE,
    first_name VARCHAR(50) NOT NULL,
    last_name VARCHAR(50) NOT NULL,
    email VARCHAR(100) NOT NULL UNIQUE,
    phone VARCHAR(20) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    status ENUM('ACTIVE', 'FROZEN', 'SUSPENDED') NOT NULL DEFAULT 'ACTIVE',
    two_factor_enabled TINYINT(1) NOT NULL DEFAULT 0,
    two_factor_secret VARCHAR(64) NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_customers_email (email),
    INDEX idx_customers_status (status)
) ENGINE=InnoDB;

-- 2. Transaction PINs
CREATE TABLE transaction_pins (
    customer_id INT PRIMARY KEY,
    pin_hash VARCHAR(255) NOT NULL,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    CONSTRAINT fk_pin_customer FOREIGN KEY (customer_id) REFERENCES customers(id) ON DELETE CASCADE
) ENGINE=InnoDB;

-- 3. Admins Table (Role-based access control)
CREATE TABLE admins (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) NOT NULL UNIQUE,
    full_name VARCHAR(100) NOT NULL,
    email VARCHAR(100) NOT NULL UNIQUE,
    password_hash VARCHAR(255) NOT NULL,
    role ENUM('SUPER_ADMIN', 'ADMIN', 'SUPPORT', 'AUDITOR') NOT NULL DEFAULT 'ADMIN',
    status ENUM('ACTIVE', 'SUSPENDED') NOT NULL DEFAULT 'ACTIVE',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_admins_role (role)
) ENGINE=InnoDB;

-- 4. Bank Accounts (Savings and Current)
CREATE TABLE accounts (
    id INT AUTO_INCREMENT PRIMARY KEY,
    customer_id INT NOT NULL,
    account_number VARCHAR(20) NOT NULL UNIQUE,
    account_type ENUM('SAVINGS', 'CURRENT') NOT NULL DEFAULT 'SAVINGS',
    balance_paise BIGINT NOT NULL DEFAULT 0,
    currency VARCHAR(3) NOT NULL DEFAULT 'INR',
    status ENUM('ACTIVE', 'FROZEN', 'SUSPENDED', 'CLOSED') NOT NULL DEFAULT 'ACTIVE',
    branch_code VARCHAR(20) NOT NULL DEFAULT 'SMRT0001001',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    CONSTRAINT fk_account_customer FOREIGN KEY (customer_id) REFERENCES customers(id) ON DELETE CASCADE,
    INDEX idx_accounts_cust (customer_id),
    INDEX idx_accounts_status (status)
) ENGINE=InnoDB;

-- 5. Transactions Table
CREATE TABLE transactions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    reference_id VARCHAR(64) NOT NULL UNIQUE,
    from_account_id INT NULL,
    to_account_id INT NULL,
    transaction_type ENUM('DEPOSIT', 'WITHDRAWAL', 'TRANSFER', 'INTEREST', 'FEE') NOT NULL,
    amount_paise BIGINT NOT NULL,
    balance_after_paise BIGINT NOT NULL,
    category VARCHAR(50) NOT NULL DEFAULT 'General',
    description VARCHAR(255) NOT NULL,
    status ENUM('SUCCESS', 'FAILED', 'FLAGGED', 'PENDING') NOT NULL DEFAULT 'SUCCESS',
    flag_reason VARCHAR(255) NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_tx_from_account FOREIGN KEY (from_account_id) REFERENCES accounts(id) ON DELETE SET NULL,
    CONSTRAINT fk_tx_to_account FOREIGN KEY (to_account_id) REFERENCES accounts(id) ON DELETE SET NULL,
    INDEX idx_tx_from (from_account_id),
    INDEX idx_tx_to (to_account_id),
    INDEX idx_tx_created (created_at),
    INDEX idx_tx_type (transaction_type),
    INDEX idx_tx_category (category)
) ENGINE=InnoDB;

-- 6. Beneficiaries
CREATE TABLE beneficiaries (
    id INT AUTO_INCREMENT PRIMARY KEY,
    customer_id INT NOT NULL,
    beneficiary_name VARCHAR(100) NOT NULL,
    account_number VARCHAR(20) NOT NULL,
    bank_name VARCHAR(100) NOT NULL DEFAULT 'Smart Bank',
    ifsc_code VARCHAR(20) NOT NULL DEFAULT 'SMRT0001001',
    email VARCHAR(100) NULL,
    nickname VARCHAR(50) NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_ben_customer FOREIGN KEY (customer_id) REFERENCES customers(id) ON DELETE CASCADE,
    INDEX idx_ben_customer (customer_id)
) ENGINE=InnoDB;

-- 7. Savings Goals
CREATE TABLE savings_goals (
    id INT AUTO_INCREMENT PRIMARY KEY,
    customer_id INT NOT NULL,
    account_id INT NOT NULL,
    title VARCHAR(100) NOT NULL,
    target_amount_paise BIGINT NOT NULL,
    current_amount_paise BIGINT NOT NULL DEFAULT 0,
    target_date DATE NOT NULL,
    category VARCHAR(50) NOT NULL DEFAULT 'Emergency',
    status ENUM('IN_PROGRESS', 'COMPLETED', 'CANCELLED') NOT NULL DEFAULT 'IN_PROGRESS',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_goal_customer FOREIGN KEY (customer_id) REFERENCES customers(id) ON DELETE CASCADE,
    CONSTRAINT fk_goal_account FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE,
    INDEX idx_goals_cust (customer_id)
) ENGINE=InnoDB;

-- 8. Savings Contributions
CREATE TABLE savings_contributions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    goal_id INT NOT NULL,
    amount_paise BIGINT NOT NULL,
    note VARCHAR(255) NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_contrib_goal FOREIGN KEY (goal_id) REFERENCES savings_goals(id) ON DELETE CASCADE,
    INDEX idx_contrib_goal (goal_id)
) ENGINE=InnoDB;

-- 9. Virtual Debit Cards (Simulated)
CREATE TABLE virtual_cards (
    id INT AUTO_INCREMENT PRIMARY KEY,
    customer_id INT NOT NULL,
    account_id INT NOT NULL,
    card_number_masked VARCHAR(50) NOT NULL,
    card_type ENUM('VISA', 'MASTERCARD', 'RUPAY') NOT NULL DEFAULT 'VISA',
    expiry_month INT NOT NULL,
    expiry_year INT NOT NULL,
    spending_limit_paise BIGINT NOT NULL DEFAULT 5000000,
    status ENUM('ACTIVE', 'FROZEN', 'BLOCKED') NOT NULL DEFAULT 'ACTIVE',
    is_simulated TINYINT(1) NOT NULL DEFAULT 1,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_card_customer FOREIGN KEY (customer_id) REFERENCES customers(id) ON DELETE CASCADE,
    CONSTRAINT fk_card_account FOREIGN KEY (account_id) REFERENCES accounts(id) ON DELETE CASCADE,
    INDEX idx_card_cust (customer_id)
) ENGINE=InnoDB;

-- 10. Service Requests (Queue managed)
CREATE TABLE service_requests (
    id INT AUTO_INCREMENT PRIMARY KEY,
    ticket_number VARCHAR(32) NOT NULL UNIQUE,
    customer_id INT NOT NULL,
    account_id INT NULL,
    request_type ENUM('CHEQUE_BOOK', 'DEBIT_CARD_REISSUE', 'ADDRESS_UPDATE', 'ACCOUNT_UPGRADE', 'LIMIT_INCREASE', 'ACCOUNT_UNFREEZE', 'GENERAL_SUPPORT') NOT NULL,
    description TEXT NOT NULL,
    priority ENUM('LOW', 'MEDIUM', 'HIGH', 'URGENT') NOT NULL DEFAULT 'MEDIUM',
    status ENUM('PENDING', 'IN_REVIEW', 'APPROVED', 'REJECTED', 'RESOLVED') NOT NULL DEFAULT 'PENDING',
    admin_notes TEXT NULL,
    resolved_by_admin_id INT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    CONSTRAINT fk_sr_customer FOREIGN KEY (customer_id) REFERENCES customers(id) ON DELETE CASCADE,
    INDEX idx_sr_status (status),
    INDEX idx_sr_cust (customer_id)
) ENGINE=InnoDB;

-- 11. Customer Notifications
CREATE TABLE notifications (
    id INT AUTO_INCREMENT PRIMARY KEY,
    customer_id INT NOT NULL,
    title VARCHAR(150) NOT NULL,
    message TEXT NOT NULL,
    type ENUM('TRANSACTION', 'SECURITY', 'SYSTEM', 'PROMOTION', 'SERVICE') NOT NULL DEFAULT 'SYSTEM',
    is_read TINYINT(1) NOT NULL DEFAULT 0,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    CONSTRAINT fk_notif_customer FOREIGN KEY (customer_id) REFERENCES customers(id) ON DELETE CASCADE,
    INDEX idx_notif_customer (customer_id, is_read)
) ENGINE=InnoDB;

-- 12. Login History
CREATE TABLE login_history (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_type ENUM('CUSTOMER', 'ADMIN') NOT NULL,
    user_id INT NOT NULL,
    ip_address VARCHAR(45) NOT NULL,
    user_agent VARCHAR(255) NULL,
    status ENUM('SUCCESS', 'FAILED') NOT NULL,
    failure_reason VARCHAR(100) NULL,
    login_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_lh_user (user_type, user_id)
) ENGINE=InnoDB;

-- 13. Audit Logs (Immutable audit trail)
CREATE TABLE audit_logs (
    id INT AUTO_INCREMENT PRIMARY KEY,
    actor_type ENUM('CUSTOMER', 'ADMIN', 'SYSTEM') NOT NULL,
    actor_id INT NULL,
    action VARCHAR(100) NOT NULL,
    entity_type VARCHAR(50) NOT NULL,
    entity_id INT NULL,
    details TEXT NULL,
    ip_address VARCHAR(45) NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_audit_actor (actor_type, actor_id),
    INDEX idx_audit_action (action)
) ENGINE=InnoDB;

-- 14. Sessions Table (Server-side Session Storage)
CREATE TABLE sessions (
    id INT AUTO_INCREMENT PRIMARY KEY,
    session_token VARCHAR(128) NOT NULL UNIQUE,
    user_type ENUM('CUSTOMER', 'ADMIN') NOT NULL,
    user_id INT NOT NULL,
    role VARCHAR(32) NOT NULL DEFAULT 'CUSTOMER',
    csrf_token VARCHAR(64) NOT NULL,
    ip_address VARCHAR(45) NULL,
    user_agent VARCHAR(255) NULL,
    expires_at DATETIME NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_session_token (session_token),
    INDEX idx_session_expiry (expires_at)
) ENGINE=InnoDB;

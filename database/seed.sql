-- ==============================================================================
-- SMART BANKING MANAGEMENT SYSTEM - SEED DATA
-- Fully initialized demo environment with authentic Argon2id password hashes
-- Default Passwords:
--   Customers: Password@123
--   Admins:    Admin@123
--   Txn PIN:   123456
-- ==============================================================================

-- 1. Seed Customers
INSERT INTO customers (id, customer_number, first_name, last_name, email, phone, password_hash, status, two_factor_enabled) VALUES
(1, 'CUST10001', 'Rajesh', 'Sharma', 'rajesh@example.com', '+919876543210', '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$PKbV/0FtW2QN/J4uuLDuTRWC5kirC/Gf6tH/7AM3yCU', 'ACTIVE', 0),
(2, 'CUST10002', 'Priya', 'Patel', 'priya@example.com', '+919876543211', '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$PKbV/0FtW2QN/J4uuLDuTRWC5kirC/Gf6tH/7AM3yCU', 'ACTIVE', 0),
(3, 'CUST10003', 'Amit', 'Kumar', 'amit@example.com', '+919876543212', '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$PKbV/0FtW2QN/J4uuLDuTRWC5kirC/Gf6tH/7AM3yCU', 'ACTIVE', 0);

-- 2. Seed Transaction PINs (123456)
INSERT INTO transaction_pins (customer_id, pin_hash) VALUES
(1, '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$RkayMI/dXePAYuMXtwQW7Q0+4C4IhsAZW8Ol8WjP6RQ'),
(2, '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$RkayMI/dXePAYuMXtwQW7Q0+4C4IhsAZW8Ol8WjP6RQ'),
(3, '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$RkayMI/dXePAYuMXtwQW7Q0+4C4IhsAZW8Ol8WjP6RQ');

-- 3. Seed Admins (Admin@123)
INSERT INTO admins (id, username, full_name, email, password_hash, role, status) VALUES
(1, 'superadmin', 'System Super Administrator', 'superadmin@smartbank.internal', '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$5OXrH7kKMConSWnfKxk+SEg0EA+Swa48Mwn9b0KvcPQ', 'SUPER_ADMIN', 'ACTIVE'),
(2, 'admin', 'Branch Operations Manager', 'admin@smartbank.internal', '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$5OXrH7kKMConSWnfKxk+SEg0EA+Swa48Mwn9b0KvcPQ', 'ADMIN', 'ACTIVE'),
(3, 'support', 'Customer Care Officer', 'support@smartbank.internal', '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$5OXrH7kKMConSWnfKxk+SEg0EA+Swa48Mwn9b0KvcPQ', 'SUPPORT', 'ACTIVE'),
(4, 'auditor', 'Compliance Audit Officer', 'auditor@smartbank.internal', '$argon2id$v=19$m=65536,t=2,p=1$c21hcnRiYW5raW5nc2FsdA$5OXrH7kKMConSWnfKxk+SEg0EA+Swa48Mwn9b0KvcPQ', 'AUDITOR', 'ACTIVE');

-- 4. Seed Accounts (Paise: 1 INR = 100 paise)
-- Rajesh has Savings (₹2,54,320.50) and Current (₹1,15,400.00)
-- Priya has Savings (₹1,85,000.00)
-- Amit has Savings (₹92,500.00)
INSERT INTO accounts (id, customer_id, account_number, account_type, balance_paise, currency, status, branch_code) VALUES
(1, 1, '100100200301', 'SAVINGS', 25432050, 'INR', 'ACTIVE', 'SMRT0001001'),
(2, 1, '100100200302', 'CURRENT', 11540000, 'INR', 'ACTIVE', 'SMRT0001001'),
(3, 2, '100100200401', 'SAVINGS', 18500000, 'INR', 'ACTIVE', 'SMRT0001001'),
(4, 3, '100100200501', 'SAVINGS',  9250000, 'INR', 'ACTIVE', 'SMRT0001001');

-- 5. Seed Transactions (with realistic timestamps and categories)
INSERT INTO transactions (id, reference_id, from_account_id, to_account_id, transaction_type, amount_paise, balance_after_paise, category, description, status, created_at) VALUES
(1, 'TXN202611010001', NULL, 1, 'DEPOSIT', 15000000, 27500000, 'Salary', 'Monthly Corporate Salary Credit', 'SUCCESS', '2026-11-01 09:30:00'),
(2, 'TXN202611020002', 1, NULL, 'WITHDRAWAL', 500000, 27000000, 'General', 'ATM Cash Withdrawal - Connaught Place', 'SUCCESS', '2026-11-02 14:15:00'),
(3, 'TXN202611030003', 1, NULL, 'WITHDRAWAL', 425000, 26575000, 'Shopping', 'Amazon India Electronics purchase', 'SUCCESS', '2026-11-03 18:45:00'),
(4, 'TXN202611040004', 1, 3, 'TRANSFER', 1500000, 25075000, 'Transfer', 'Internal Fund Transfer to Priya Patel', 'SUCCESS', '2026-11-04 11:10:00'),
(5, 'TXN202611050005', 1, NULL, 'WITHDRAWAL', 185000, 24890000, 'Food & Dining', 'Swiggy Gourmet Dining & Grocery', 'SUCCESS', '2026-11-05 20:20:00'),
(6, 'TXN202611060006', 1, NULL, 'WITHDRAWAL', 345000, 24545000, 'Utilities', 'Tata Power Electricity Bill payment', 'SUCCESS', '2026-11-06 10:05:00'),
(7, 'TXN202611070007', NULL, 1, 'DEPOSIT', 1000000, 25545000, 'Investment', 'Mutual Fund Dividend Payout', 'SUCCESS', '2026-11-07 16:00:00'),
(8, 'TXN202611080008', 1, NULL, 'WITHDRAWAL', 149900, 25395100, 'Entertainment', 'Annual OTT Subscription renewal', 'SUCCESS', '2026-11-08 22:15:00'),
(9, 'TXN202611090009', 1, NULL, 'WITHDRAWAL', 280000, 25115100, 'Travel', 'Indian Oil Fuel refuel - Fort Station', 'SUCCESS', '2026-11-09 13:40:00'),
(10, 'TXN202611100010', NULL, 1, 'INTEREST', 316950, 25432050, 'Interest', 'Quarterly Savings Interest Credit', 'SUCCESS', '2026-11-10 00:01:00');

-- 6. Seed Beneficiaries
INSERT INTO beneficiaries (id, customer_id, beneficiary_name, account_number, bank_name, ifsc_code, email, nickname) VALUES
(1, 1, 'Priya Patel', '100100200401', 'Smart Bank', 'SMRT0001001', 'priya@example.com', 'Priya (Colleague)'),
(2, 1, 'Amit Kumar', '100100200501', 'Smart Bank', 'SMRT0001001', 'amit@example.com', 'Amit (Brother)'),
(3, 1, 'Rohit Verma', '200100300901', 'HDFC Bank', 'HDFC0001422', 'rohit.verma@example.com', 'Rohit Landlord');

-- 7. Seed Savings Goals
INSERT INTO savings_goals (id, customer_id, account_id, title, target_amount_paise, current_amount_paise, target_date, category, status) VALUES
(1, 1, 1, 'MacBook Pro M3 Max', 22000000, 14000000, '2027-04-30', 'Gadget', 'IN_PROGRESS'),
(2, 1, 1, 'Emergency Reserve Fund', 50000000, 31000000, '2027-12-31', 'Emergency Fund', 'IN_PROGRESS'),
(3, 1, 1, 'Europe Vacation 2027', 35000000, 8500000, '2027-09-15', 'Vacation', 'IN_PROGRESS');

-- 8. Seed Savings Contributions
INSERT INTO savings_contributions (id, goal_id, amount_paise, note, created_at) VALUES
(1, 1, 5000000, 'Initial savings allocation', '2026-10-01 10:00:00'),
(2, 1, 9000000, 'Bonus deposit contribution', '2026-11-01 15:30:00'),
(3, 2, 20000000, 'Primary emergency deposit', '2026-09-15 12:00:00'),
(4, 2, 11000000, 'Monthly surplus saving', '2026-11-05 09:00:00'),
(5, 3, 8500000, 'First installment for trip booking', '2026-11-02 18:00:00');

-- 9. Seed Virtual Debit Cards
INSERT INTO virtual_cards (id, customer_id, account_id, card_number_masked, card_type, expiry_month, expiry_year, spending_limit_paise, status, is_simulated) VALUES
(1, 1, 1, '4532 **** **** 8842', 'VISA', 11, 2029, 7500000, 'ACTIVE', 1),
(2, 1, 2, '6074 **** **** 1923', 'RUPAY', 8, 2030, 5000000, 'ACTIVE', 1);

-- 10. Seed Service Requests
INSERT INTO service_requests (id, ticket_number, customer_id, account_id, request_type, description, priority, status) VALUES
(1, 'SR-20261105-001', 1, 1, 'CHEQUE_BOOK', 'Request for 50-leaf personalized cheque book delivered to registered address.', 'LOW', 'PENDING'),
(2, 'SR-20261107-002', 1, 2, 'LIMIT_INCREASE', 'Request to increase daily online transfer limit to ₹5,00,000 for business transactions.', 'HIGH', 'IN_REVIEW'),
(3, 'SR-20261020-003', 1, 1, 'ACCOUNT_UPGRADE', 'Upgrade savings tier to Platinum Privilege Banking.', 'MEDIUM', 'APPROVED');

-- 11. Seed Notifications
INSERT INTO notifications (id, customer_id, title, message, type, is_read, created_at) VALUES
(1, 1, 'Salary Credited', 'Your Account 100100200301 has been credited with ₹1,50,000.00 towards monthly salary.', 'TRANSACTION', 1, '2026-11-01 09:30:00'),
(2, 1, 'Quarterly Interest Credited', 'Interest of ₹3,169.50 has been credited to your Savings Account.', 'TRANSACTION', 0, '2026-11-10 00:01:00'),
(3, 1, 'Security Notice: New Session', 'Login detected from Chrome Windows 11 on 127.0.0.1.', 'SECURITY', 0, '2026-11-11 20:00:00'),
(4, 1, 'Service Request Update', 'Your ticket SR-20261020-003 for Platinum Upgrade has been APPROVED.', 'SERVICE', 0, '2026-11-08 14:00:00');

-- 12. Seed Login History
INSERT INTO login_history (id, user_type, user_id, ip_address, user_agent, status, login_at) VALUES
(1, 'CUSTOMER', 1, '127.0.0.1', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/131.0', 'SUCCESS', '2026-11-10 10:15:00'),
(2, 'CUSTOMER', 1, '127.0.0.1', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/131.0', 'SUCCESS', '2026-11-11 19:40:00'),
(3, 'ADMIN', 1, '127.0.0.1', 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/131.0', 'SUCCESS', '2026-11-11 18:00:00');

-- 13. Seed Audit Logs
INSERT INTO audit_logs (id, actor_type, actor_id, action, entity_type, entity_id, details, ip_address, created_at) VALUES
(1, 'SYSTEM', NULL, 'SCHEMA_INITIALIZED', 'DATABASE', NULL, 'Initial schema and tables created.', '127.0.0.1', '2026-11-01 00:00:00'),
(2, 'CUSTOMER', 1, 'LOGIN_SUCCESS', 'CUSTOMER', 1, 'Customer Rajesh Sharma authenticated successfully.', '127.0.0.1', '2026-11-11 19:40:00'),
(3, 'ADMIN', 1, 'STATUS_UPDATE', 'SERVICE_REQUEST', 3, 'Admin approved ticket SR-20261020-003.', '127.0.0.1', '2026-11-08 14:00:00');

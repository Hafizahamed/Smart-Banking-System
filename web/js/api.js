/**
 * SMART BANKING MANAGEMENT SYSTEM - API CLIENT
 */

const API = {
  csrfToken: '',

  async request(endpoint, options = {}) {
    const defaultHeaders = {
      'Content-Type': 'application/json',
      'X-CSRF-Token': this.csrfToken || localStorage.getItem('csrf_token') || ''
    };

    const config = {
      ...options,
      headers: {
        ...defaultHeaders,
        ...options.headers
      }
    };

    try {
      const response = await fetch(endpoint, config);
      const data = await response.json().catch(() => ({}));

      if (!response.ok) {
        throw new Error(data.message || `Request failed with status ${response.status}`);
      }

      if (data.csrf_token) {
        this.csrfToken = data.csrf_token;
        localStorage.setItem('csrf_token', data.csrf_token);
      }

      return data;
    } catch (err) {
      console.error(`[API Error] ${endpoint}:`, err);
      throw err;
    }
  },

  // Auth Endpoints
  register(payload) { return this.request('/api/auth/register', { method: 'POST', body: JSON.stringify(payload) }); },
  login(payload) { return this.request('/api/auth/login', { method: 'POST', body: JSON.stringify(payload) }); },
  adminLogin(payload) { return this.request('/api/auth/admin-login', { method: 'POST', body: JSON.stringify(payload) }); },
  logout() { return this.request('/api/auth/logout', { method: 'POST' }); },
  getMe() { return this.request('/api/auth/me'); },
  changePassword(payload) { return this.request('/api/auth/change-password', { method: 'POST', body: JSON.stringify(payload) }); },
  setPin(pin) { return this.request('/api/auth/set-pin', { method: 'POST', body: JSON.stringify({ pin }) }); },
  verifyPin(pin) { return this.request('/api/auth/verify-pin', { method: 'POST', body: JSON.stringify({ pin }) }); },
  toggle2FA(enabled) { return this.request('/api/auth/toggle-2fa', { method: 'POST', body: JSON.stringify({ enabled }) }); },

  // Banking Endpoints
  getAccounts() { return this.request('/api/accounts'); },
  createAccount(payload) { return this.request('/api/accounts', { method: 'POST', body: JSON.stringify(payload) }); },
  freezeAccount(accountId, reason) { return this.request('/api/accounts/freeze', { method: 'POST', body: JSON.stringify({ account_id: accountId, reason }) }); },
  unfreezeAccount(accountId) { return this.request('/api/accounts/unfreeze', { method: 'POST', body: JSON.stringify({ account_id: accountId }) }); },

  deposit(payload) { return this.request('/api/banking/deposit', { method: 'POST', body: JSON.stringify(payload) }); },
  withdraw(payload) { return this.request('/api/banking/withdraw', { method: 'POST', body: JSON.stringify(payload) }); },
  transfer(payload) { return this.request('/api/banking/transfer', { method: 'POST', body: JSON.stringify(payload) }); },
  getTransactions(params = {}) {
    const q = new URLSearchParams(params).toString();
    return this.request(`/api/banking/transactions?${q}`);
  },
  getStatement(accountId, year, month) {
    return this.request(`/api/banking/statement?account_id=${accountId}&year=${year}&month=${month}`);
  },

  // Beneficiaries
  getBeneficiaries() { return this.request('/api/beneficiaries'); },
  addBeneficiary(payload) { return this.request('/api/beneficiaries', { method: 'POST', body: JSON.stringify(payload) }); },
  deleteBeneficiary(id) { return this.request('/api/beneficiaries/delete', { method: 'POST', body: JSON.stringify({ id }) }); },

  // Virtual Cards
  getCards() { return this.request('/api/cards'); },
  createCard(accountId, cardType) { return this.request('/api/cards', { method: 'POST', body: JSON.stringify({ account_id: accountId, card_type: cardType }) }); },
  toggleCardFreeze(cardId) { return this.request('/api/cards/toggle-freeze', { method: 'POST', body: JSON.stringify({ card_id: cardId }) }); },

  // Savings
  getSavingsGoals() { return this.request('/api/savings/goals'); },
  createSavingsGoal(payload) { return this.request('/api/savings/goals', { method: 'POST', body: JSON.stringify(payload) }); },
  contributeSavingsGoal(payload) { return this.request('/api/savings/contribute', { method: 'POST', body: JSON.stringify(payload) }); },

  // Service Requests
  getServiceRequests() { return this.request('/api/service-requests'); },
  createServiceRequest(payload) { return this.request('/api/service-requests', { method: 'POST', body: JSON.stringify(payload) }); },

  // Notifications
  getNotifications() { return this.request('/api/notifications'); },
  markNotificationRead(id) { return this.request('/api/notifications/read', { method: 'POST', body: JSON.stringify({ id }) }); },
  markAllNotificationsRead() { return this.request('/api/notifications/read-all', { method: 'POST' }); },

  // Login History & Analytics
  getLoginHistory() { return this.request('/api/login-history'); },
  getAnalyticsSummary() { return this.request('/api/analytics/summary'); },
  askAI(prompt) { return this.request('/api/ai/assistant', { method: 'POST', body: JSON.stringify({ prompt }) }); },
  getDSDemo() { return this.request('/api/ds/demo'); },

  // Admin Portal
  adminGetOverview() { return this.request('/api/admin/overview'); },
  adminGetCustomers(search = '') { return this.request(`/api/admin/customers?q=${encodeURIComponent(search)}`); },
  adminUpdateCustomerStatus(customerId, status, reason) { return this.request('/api/admin/customers/status', { method: 'POST', body: JSON.stringify({ customer_id: customerId, status, reason }) }); },
  adminUpdateAccountStatus(accountId, status, reason) { return this.request('/api/admin/accounts/status', { method: 'POST', body: JSON.stringify({ account_id: accountId, status, reason }) }); },
  adminGetTransactions(flaggedOnly = false) { return this.request(`/api/admin/transactions?flagged=${flaggedOnly ? '1' : '0'}`); },
  adminGetServiceRequests(status = 'ALL') { return this.request(`/api/admin/service-requests?status=${status}`); },
  adminProcessServiceRequest(ticketId, decision, notes) { return this.request('/api/admin/service-requests/action', { method: 'POST', body: JSON.stringify({ ticket_id: ticketId, decision, admin_notes: notes }) }); },
  adminGetAuditLogs() { return this.request('/api/admin/audit-logs'); },
  adminGetSystemHealth() { return this.request('/api/admin/system-health'); }
};

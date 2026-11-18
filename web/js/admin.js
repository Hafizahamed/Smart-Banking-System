/**
 * SMART BANKING MANAGEMENT SYSTEM - ADMIN PORTAL CONTROLLER
 */

const AdminApp = {
  adminUser: null,
  activeTab: 'overview',

  async init() {
    this.setupListeners();
    await this.checkSession();
  },

  setupListeners() {
    // Admin Login Form
    const loginForm = document.getElementById('admin-login-form');
    if (loginForm) {
      loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const username = document.getElementById('admin-username').value.trim();
        const password = document.getElementById('admin-password').value;

        try {
          const res = await API.adminLogin({ username, password });
          if (res.status === 'success') {
            this.adminUser = res.user;
            this.renderAdminView();
            await this.loadOverview();
          }
        } catch (err) {
          alert('Admin Login Failed: ' + err.message);
        }
      });
    }

    // Admin Logout
    const logoutBtn = document.getElementById('admin-logout-btn');
    if (logoutBtn) {
      logoutBtn.addEventListener('click', async () => {
        await API.logout();
        this.adminUser = null;
        this.renderAdminView();
      });
    }

    // Tab Navigation
    document.querySelectorAll('.admin-tab-btn').forEach(btn => {
      btn.addEventListener('click', (e) => {
        const tab = e.target.getAttribute('data-tab');
        this.switchTab(tab);
      });
    });

    // Customer Search
    const searchBtn = document.getElementById('btn-admin-search-cust');
    if (searchBtn) {
      searchBtn.addEventListener('click', () => {
        const q = document.getElementById('admin-search-input').value.trim();
        this.loadCustomers(q);
      });
    }
  },

  async checkSession() {
    try {
      const res = await API.getMe();
      if (res.status === 'success' && res.user && res.user.role !== 'CUSTOMER') {
        this.adminUser = res.user;
        this.renderAdminView();
        await this.loadOverview();
      } else {
        this.adminUser = null;
        this.renderAdminView();
      }
    } catch {
      this.adminUser = null;
      this.renderAdminView();
    }
  },

  renderAdminView() {
    const loginSection = document.getElementById('admin-login-view');
    const dashboardSection = document.getElementById('admin-dashboard-view');

    if (this.adminUser) {
      if (loginSection) loginSection.style.display = 'none';
      if (dashboardSection) dashboardSection.style.display = 'block';

      document.getElementById('admin-display-name').textContent = this.adminUser.name || this.adminUser.username;
      const roleBadge = document.getElementById('admin-display-role');
      if (roleBadge) roleBadge.textContent = this.adminUser.role;
    } else {
      if (loginSection) loginSection.style.display = 'flex';
      if (dashboardSection) dashboardSection.style.display = 'none';
    }
  },

  async switchTab(tab) {
    this.activeTab = tab;
    document.querySelectorAll('.admin-tab-btn').forEach(b => {
      b.classList.toggle('active', b.getAttribute('data-tab') === tab);
    });

    document.querySelectorAll('.admin-tab-pane').forEach(p => {
      p.style.display = (p.id === `tab-${tab}`) ? 'block' : 'none';
    });

    if (tab === 'overview') await this.loadOverview();
    else if (tab === 'customers') await this.loadCustomers();
    else if (tab === 'transactions') await this.loadTransactions();
    else if (tab === 'requests') await this.loadServiceRequests();
    else if (tab === 'audit') await this.loadAuditLogs();
    else if (tab === 'health') await this.loadSystemHealth();
  },

  async loadOverview() {
    try {
      const data = await API.adminGetOverview();
      document.getElementById('kpi-total-cust').textContent = data.total_customers;
      document.getElementById('kpi-total-acc').textContent = data.total_accounts;
      document.getElementById('kpi-total-vault').textContent = data.total_deposits_formatted;
      document.getElementById('kpi-total-tx').textContent = data.total_transactions;
      document.getElementById('kpi-flagged-tx').textContent = data.flagged_transactions;
      document.getElementById('kpi-pending-sr').textContent = data.pending_service_requests;
    } catch (err) {
      console.error(err);
    }
  },

  async loadCustomers(search = '') {
    try {
      const customers = await API.adminGetCustomers(search);
      const tbody = document.getElementById('admin-cust-tbody');
      if (!tbody) return;

      tbody.innerHTML = customers.map(c => `
        <tr>
          <td><strong>${c.customer_number}</strong></td>
          <td>${c.first_name} ${c.last_name}</td>
          <td>${c.email}</td>
          <td>${c.phone}</td>
          <td><span class="badge ${c.status === 'ACTIVE' ? 'badge-success' : 'badge-danger'}">${c.status}</span></td>
          <td>${c.account_count} accounts</td>
          <td><strong>${c.total_balance_formatted}</strong></td>
          <td>
            ${c.status === 'ACTIVE' ?
              `<button class="btn btn-danger btn-sm" onclick="AdminApp.toggleCustomerStatus(${c.id}, 'SUSPENDED')">Suspend</button>` :
              `<button class="btn btn-primary btn-sm" onclick="AdminApp.toggleCustomerStatus(${c.id}, 'ACTIVE')">Reactivate</button>`
            }
          </td>
        </tr>
      `).join('');
    } catch (err) {
      console.error(err);
    }
  },

  async toggleCustomerStatus(custId, newStatus) {
    const reason = prompt(`Enter reason for updating customer status to ${newStatus}:`, 'Administrative action');
    if (reason === null) return;
    try {
      await API.adminUpdateCustomerStatus(custId, newStatus, reason);
      alert(`Customer status changed to ${newStatus}`);
      await this.loadCustomers();
    } catch (err) {
      alert('Error: ' + err.message);
    }
  },

  async loadTransactions(flaggedOnly = false) {
    try {
      const txns = await API.adminGetTransactions(flaggedOnly);
      const tbody = document.getElementById('admin-tx-tbody');
      if (!tbody) return;

      tbody.innerHTML = txns.map(t => `
        <tr>
          <td><strong>${t.reference_id}</strong></td>
          <td>${t.type}</td>
          <td>${t.from_account_number} &rarr; ${t.to_account_number}</td>
          <td>${t.category}</td>
          <td><strong>${t.amount_formatted}</strong></td>
          <td><span class="badge ${t.status === 'FLAGGED' ? 'badge-warning' : 'badge-success'}">${t.status}</span></td>
          <td style="color: #ef4444; font-size: 0.8rem;">${t.flag_reason || 'Clean'}</td>
          <td>${t.created_at}</td>
        </tr>
      `).join('');
    } catch (err) {
      console.error(err);
    }
  },

  async loadServiceRequests() {
    try {
      const reqs = await API.adminGetServiceRequests();
      const tbody = document.getElementById('admin-sr-tbody');
      if (!tbody) return;

      tbody.innerHTML = reqs.map(r => `
        <tr>
          <td><strong>${r.ticket_number}</strong></td>
          <td>${r.customer_name}</td>
          <td><span class="badge badge-info">${r.request_type}</span></td>
          <td><span class="badge ${r.priority === 'HIGH' || r.priority === 'URGENT' ? 'badge-danger' : 'badge-warning'}">${r.priority}</span></td>
          <td style="max-width: 280px;">${r.description}</td>
          <td><span class="badge ${r.status === 'APPROVED' ? 'badge-success' : r.status === 'REJECTED' ? 'badge-danger' : 'badge-warning'}">${r.status}</span></td>
          <td>
            ${r.status === 'PENDING' || r.status === 'IN_REVIEW' ? `
              <button class="btn btn-primary btn-sm" onclick="AdminApp.processRequest(${r.id}, 'APPROVED')">Approve</button>
              <button class="btn btn-danger btn-sm" onclick="AdminApp.processRequest(${r.id}, 'REJECTED')">Reject</button>
            ` : `<span style="color: var(--text-dim); font-size: 0.8rem;">Closed</span>`}
          </td>
        </tr>
      `).join('');
    } catch (err) {
      console.error(err);
    }
  },

  async processRequest(ticketId, decision) {
    const notes = prompt(`Admin notes for ${decision}:`, 'Verified by bank officer.');
    if (notes === null) return;
    try {
      await API.adminProcessServiceRequest(ticketId, decision, notes);
      alert(`Ticket has been ${decision}`);
      await this.loadServiceRequests();
      await this.loadOverview();
    } catch (err) {
      alert('Error: ' + err.message);
    }
  },

  async loadAuditLogs() {
    try {
      const logs = await API.adminGetAuditLogs();
      const tbody = document.getElementById('admin-audit-tbody');
      if (!tbody) return;

      tbody.innerHTML = logs.map(l => `
        <tr>
          <td>${l.created_at}</td>
          <td><span class="badge badge-info">${l.actor_type} (ID ${l.actor_id || 'System'})</span></td>
          <td><strong>${l.action}</strong></td>
          <td>${l.entity_type} #${l.entity_id || '-'}</td>
          <td style="color: var(--text-muted);">${l.details}</td>
          <td style="font-family: var(--font-mono);">${l.ip_address}</td>
        </tr>
      `).join('');
    } catch (err) {
      console.error(err);
    }
  },

  async loadSystemHealth() {
    try {
      const h = await API.adminGetSystemHealth();
      document.getElementById('health-db-status').textContent = h.database.status;
      document.getElementById('health-mysql-ver').textContent = h.mysql_server_version;
      document.getElementById('health-http-engine').textContent = h.http_engine;
      document.getElementById('health-crypto').textContent = h.cryptography;
      document.getElementById('health-sessions').textContent = h.active_sessions;
    } catch (err) {
      console.error(err);
    }
  }
};

window.addEventListener('DOMContentLoaded', () => {
  AdminApp.init();
});

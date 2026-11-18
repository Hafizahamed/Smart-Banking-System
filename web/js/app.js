/**
 * SMART BANKING MANAGEMENT SYSTEM - CUSTOMER APPLICATION CONTROLLER
 */

const App = {
  async init() {
    // Initialize Theme
    State.setTheme(State.theme);

    // Setup Event Listeners
    this.setupListeners();

    // Check Session
    try {
      const meRes = await API.getMe();
      if (meRes.status === 'success' && meRes.user) {
        State.setUser(meRes.user);
        await this.loadAccounts();
        await this.loadNotifications();
      }
    } catch {
      State.setUser(null);
    }

    // Initialize Router
    Router.init();
    if (window.lucide) lucide.createIcons();
  },

  setupListeners() {
    // Theme Toggle
    const themeBtn = document.getElementById('theme-toggle-btn');
    if (themeBtn) {
      themeBtn.addEventListener('click', () => {
        const next = State.theme === 'dark' ? 'light' : 'dark';
        State.setTheme(next);
      });
    }

    // Sidebar Drawer Navigation Controls (Toggle, Close, Backdrop, Escape, Nav-Clicks)
    const sidebarToggle = document.getElementById('sidebar-toggle-btn');
    if (sidebarToggle) {
      sidebarToggle.addEventListener('click', (e) => {
        e.stopPropagation();
        UI.toggleSidebar();
      });
    }

    const sidebarClose = document.getElementById('sidebar-close-btn');
    if (sidebarClose) {
      sidebarClose.addEventListener('click', (e) => {
        e.stopPropagation();
        UI.closeSidebar();
      });
    }

    const sidebarBackdrop = document.getElementById('sidebar-backdrop');
    if (sidebarBackdrop) {
      sidebarBackdrop.addEventListener('click', () => {
        UI.closeSidebar();
      });
    }

    // Auto-close sidebar whenever any menu link is chosen (Deposit, Withdraw, Transfer, etc.)
    document.querySelectorAll('.sidebar-nav .nav-item').forEach(link => {
      link.addEventListener('click', () => {
        UI.closeSidebar();
      });
    });

    // Close on click outside sidebar drawer
    document.addEventListener('click', (e) => {
      const sidebar = document.querySelector('.app-sidebar');
      if (sidebar && sidebar.classList.contains('open')) {
        if (!sidebar.contains(e.target) && sidebarToggle && !sidebarToggle.contains(e.target)) {
          UI.closeSidebar();
        }
      }
    });

    // Close on Escape key (both sidebar and modals)
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        UI.closeSidebar();
        document.querySelectorAll('.modal-backdrop.open').forEach(m => m.classList.remove('open'));
      }
    });

    // Logout
    const logoutBtn = document.getElementById('btn-logout');
    if (logoutBtn) {
      logoutBtn.addEventListener('click', async (e) => {
        e.preventDefault();
        try {
          await API.logout();
          State.setUser(null);
          UI.showToast('info', 'Logged Out', 'You have been signed out securely.');
          Router.navigate('login');
        } catch (err) {
          console.error(err);
        }
      });
    }

    // Listen for route changes
    window.addEventListener('route-changed', (e) => {
      this.handleRouteEnter(e.detail);
    });

    // Forms Handlers Setup
    this.setupAuthForms();
    this.setupBankingForms();
    this.setupSecurityForms();
    this.setupAIAssistant();
  },

  async handleRouteEnter(route) {
    if (!State.user) return;

    if (route === 'dashboard') {
      await this.loadDashboard();
    } else if (route === 'accounts') {
      await this.loadAccounts();
      this.renderAccountsPage();
    } else if (route === 'transactions') {
      await this.loadTransactionsPage();
    } else if (route === 'analytics') {
      await this.loadAnalyticsPage();
    } else if (route === 'savings') {
      await this.loadSavingsPage();
    } else if (route === 'beneficiaries') {
      await this.loadBeneficiariesPage();
    } else if (route === 'cards') {
      await this.loadCardsPage();
    } else if (route === 'notifications') {
      await this.loadNotificationsPage();
    } else if (route === 'statements') {
      this.populateAccountSelects();
    } else if (route === 'profile') {
      this.renderProfilePage();
    } else if (route === 'login-history') {
      await this.loadLoginHistoryPage();
    } else if (route === 'deposit' || route === 'withdraw' || route === 'transfer' || route === 'qr-transfer') {
      this.populateAccountSelects();
    } else if (route === 'ds-demo') {
      await this.loadDSDemoPage();
    }
  },

  // 1. Auth Forms
  setupAuthForms() {
    const loginForm = document.getElementById('form-login');
    if (loginForm) {
      loginForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const email = document.getElementById('login-email').value.trim();
        const password = document.getElementById('login-password').value;
        const btn = loginForm.querySelector('button[type="submit"]');

        try {
          btn.disabled = true;
          btn.textContent = 'Authenticating...';
          const res = await API.login({ email, password });
          if (res.status === 'success') {
            State.setUser(res.user);
            UI.showToast('success', 'Authentication Successful', `Welcome back, ${res.user.name}!`);
            await this.loadAccounts();
            Router.navigate('dashboard');
          }
        } catch (err) {
          UI.showToast('error', 'Login Failed', err.message);
        } finally {
          btn.disabled = false;
          btn.textContent = 'Sign In';
        }
      });
    }

    const regForm = document.getElementById('form-register');
    if (regForm) {
      regForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const first_name = document.getElementById('reg-firstname').value.trim();
        const last_name = document.getElementById('reg-lastname').value.trim();
        const email = document.getElementById('reg-email').value.trim();
        const phone = document.getElementById('reg-phone').value.trim();
        const password = document.getElementById('reg-password').value;
        const btn = regForm.querySelector('button[type="submit"]');

        try {
          btn.disabled = true;
          btn.textContent = 'Creating Account...';
          const res = await API.register({ first_name, last_name, email, phone, password });
          if (res.status === 'success') {
            State.setUser(res.user);
            UI.showToast('success', 'Registration Complete', 'Your Smart Banking account is ready!');
            await this.loadAccounts();
            Router.navigate('dashboard');
          }
        } catch (err) {
          UI.showToast('error', 'Registration Failed', err.message);
        } finally {
          btn.disabled = false;
          btn.textContent = 'Open Bank Account';
        }
      });
    }
  },

  // 2. Load Core Data
  async loadAccounts() {
    try {
      const accounts = await API.getAccounts();
      State.setAccounts(accounts);
      this.populateAccountSelects();
    } catch (err) {
      console.error(err);
    }
  },

  async loadNotifications() {
    try {
      const notifs = await API.getNotifications();
      State.notifications = notifs;
      const unread = notifs.filter(n => !n.is_read).length;
      State.unreadNotificationsCount = unread;

      const badge = document.getElementById('nav-notif-count');
      if (badge) {
        badge.textContent = unread;
        badge.style.display = unread > 0 ? 'inline-block' : 'none';
      }
    } catch (err) {
      console.error(err);
    }
  },

  populateAccountSelects() {
    const selects = document.querySelectorAll('.account-select-dropdown');
    selects.forEach(sel => {
      sel.innerHTML = '';
      State.accounts.forEach(acc => {
        const opt = document.createElement('option');
        opt.value = acc.id;
        opt.textContent = `${acc.account_type} (${acc.account_number}) - ${acc.balance_formatted} [${acc.status}]`;
        sel.appendChild(opt);
      });
    });
  },

  // 3. Dashboard View
  async loadDashboard() {
    try {
      const [summary, txns] = await Promise.all([
        API.getAnalyticsSummary(),
        API.getTransactions({ limit: 5 })
      ]);

      // Update Top Statistics Cards
      document.getElementById('dash-total-balance').textContent = summary.total_balance_formatted;
      document.getElementById('dash-total-income').textContent = summary.total_income_formatted;
      document.getElementById('dash-total-expense').textContent = summary.total_expenses_formatted;
      document.getElementById('dash-savings-rate').textContent = `${summary.savings_rate_pct}%`;

      // Update User Name Greeting
      const greeting = document.getElementById('dash-greeting-name');
      if (greeting && State.user) {
        greeting.textContent = State.user.name || State.user.first_name;
      }

      // Initialize Charts
      if (summary.monthly_trends) {
        BankingCharts.initSpendingTrends('dash-trends-chart', summary.monthly_trends);
      }
      if (summary.category_breakdown) {
        BankingCharts.initCategoryDoughnut('dash-category-chart', summary.category_breakdown);
      }

      // Render Recent Transactions Table
      this.renderTransactionsList('dash-recent-txns', txns);
    } catch (err) {
      console.error(err);
    }
  },

  renderTransactionsList(elementId, txns) {
    const container = document.getElementById(elementId);
    if (!container) return;

    if (!txns || txns.length === 0) {
      container.innerHTML = `<tr><td colspan="6" style="text-align: center; color: var(--text-dim); padding: 2rem;">No banking transactions recorded yet.</td></tr>`;
      return;
    }

    container.innerHTML = txns.map(t => {
      const isCredit = !t.is_debit;
      const amtClass = isCredit ? 'amount-credit' : 'amount-debit';
      const sign = isCredit ? '+' : '-';
      const statusBadge = t.status === 'FLAGGED' ? '<span class="badge badge-warning">FLAGGED</span>' :
                          t.status === 'SUCCESS' ? '<span class="badge badge-success">SUCCESS</span>' :
                          '<span class="badge badge-danger">FAILED</span>';

      return `
        <tr>
          <td><strong>${t.reference_id}</strong></td>
          <td>${t.type}</td>
          <td><span class="badge badge-info">${t.category}</span></td>
          <td style="color: var(--text-muted);">${t.description || 'N/A'}</td>
          <td class="${amtClass}"><strong>${sign}${t.amount_formatted}</strong></td>
          <td>${statusBadge}</td>
          <td style="color: var(--text-dim); font-size: 0.8rem;">${UI.formatDate(t.created_at)}</td>
        </tr>
      `;
    }).join('');
  },

  // 3.1 Transactions History Page
  async loadTransactionsPage() {
    try {
      const txns = await API.getTransactions();
      const tbody = document.getElementById('full-txns-tbody');
      if (!tbody) return;

      if (!txns || txns.length === 0) {
        tbody.innerHTML = `<tr><td colspan="9" style="text-align: center; color: var(--text-dim); padding: 2rem;">No transaction history found for your accounts.</td></tr>`;
        return;
      }

      tbody.innerHTML = txns.map(t => {
        const isDebit = t.is_debit;
        const statusBadge = t.status === 'SUCCESS' ? '<span class="badge badge-success">SUCCESS</span>' :
                            t.status === 'FLAGGED' ? '<span class="badge badge-warning">FLAGGED</span>' :
                            '<span class="badge badge-danger">FAILED</span>';

        const accountsDisplay = t.type === 'TRANSFER'
          ? `<span style="font-family: var(--font-mono); font-size: 0.8rem;">${t.from_account_number} &rarr; ${t.to_account_number}</span>`
          : `<span style="font-family: var(--font-mono); font-size: 0.8rem;">${t.from_account_number || t.to_account_number}</span>`;

        return `
          <tr>
            <td style="font-family: var(--font-mono); font-size: 0.85rem; font-weight: 600;">${t.reference_id}</td>
            <td><span class="badge badge-secondary">${t.type}</span></td>
            <td>${accountsDisplay}</td>
            <td><span class="badge badge-info">${t.category || 'General'}</span></td>
            <td style="max-width: 200px; overflow: hidden; text-overflow: ellipsis; white-space: nowrap;">${t.description || '-'}</td>
            <td class="${isDebit ? 'amount-debit' : 'amount-credit'}" style="font-weight: 700;">
              ${isDebit ? '-' : '+'}${t.amount_formatted}
            </td>
            <td style="font-family: var(--font-mono); font-size: 0.85rem;">${t.balance_after_formatted || '-'}</td>
            <td>${statusBadge}</td>
            <td style="font-size: 0.8rem; color: var(--text-muted);">${UI.formatDate(t.created_at)}</td>
          </tr>
        `;
      }).join('');
    } catch (err) {
      console.error('Error loading transactions page:', err);
      UI.showToast('error', 'Transactions Error', 'Failed to load transaction ledger.');
    }
  },

  // 3.2 Financial Analytics Page
  async loadAnalyticsPage() {
    try {
      const summary = await API.getAnalyticsSummary();
      if (!summary) return;

      // Update KPI Cards
      const balEl = document.getElementById('analytics-total-balance');
      const incEl = document.getElementById('analytics-total-income');
      const expEl = document.getElementById('analytics-total-expense');
      const rateEl = document.getElementById('analytics-savings-rate');
      const netEl = document.getElementById('analytics-net-savings');
      const countEl = document.getElementById('analytics-account-count');

      if (balEl) balEl.textContent = summary.total_balance_formatted || '₹0.00';
      if (incEl) incEl.textContent = summary.total_income_formatted || '₹0.00';
      if (expEl) expEl.textContent = summary.total_expenses_formatted || '₹0.00';
      if (rateEl) rateEl.textContent = `${(summary.savings_rate_pct || 0).toFixed(1)}%`;
      if (netEl) netEl.textContent = `${summary.net_savings_formatted || '₹0.00'} retained`;
      if (countEl && summary.total_accounts) {
        countEl.textContent = `${summary.total_accounts} Active Account${summary.total_accounts > 1 ? 's' : ''}`;
      }

      // Render Charts
      if (summary.monthly_trends) {
        BankingCharts.initSpendingTrends('analytics-flow-chart', summary.monthly_trends);
      }
      if (summary.category_breakdown) {
        BankingCharts.initCategoryDoughnut('analytics-pie-chart', summary.category_breakdown);
      }

      // Render Category Breakdown Ledger Table
      const catTable = document.getElementById('analytics-categories-table');
      if (catTable) {
        const cats = summary.category_breakdown || [];
        if (cats.length === 0) {
          catTable.innerHTML = `<tr><td colspan="4" style="text-align: center; color: var(--text-dim); padding: 2rem;">No categorized expenses recorded yet.</td></tr>`;
        } else {
          catTable.innerHTML = cats.map(c => {
            const pct = Number(c.percentage || 0).toFixed(1);
            return `
              <tr>
                <td><strong>${c.category}</strong></td>
                <td style="font-family: var(--font-mono); font-weight: 600;">${c.amount_formatted}</td>
                <td><span class="badge badge-info">${pct}%</span></td>
                <td style="width: 35%;">
                  <div style="background: rgba(255,255,255,0.06); height: 8px; border-radius: 4px; overflow: hidden;">
                    <div style="background: var(--accent-cyan); height: 100%; width: ${Math.min(pct, 100)}%;"></div>
                  </div>
                </td>
              </tr>
            `;
          }).join('');
        }
      }

      // Render Automated Financial Insights
      const insightsContainer = document.getElementById('analytics-insights-container');
      if (insightsContainer) {
        const rate = summary.savings_rate_pct || 0;
        const cats = summary.category_breakdown || [];
        const topCat = cats.length > 0 ? cats[0].category : 'None';
        const topAmount = cats.length > 0 ? cats[0].amount_formatted : '₹0.00';

        const healthBadge = rate >= 40 
          ? '<span class="badge badge-success">EXCELLENT</span>' 
          : rate >= 20 
          ? '<span class="badge badge-info">HEALTHY</span>' 
          : '<span class="badge badge-warning">ATTENTION NEEDED</span>';

        const advice = rate >= 40
          ? 'Your net savings rate is exceptional. Consider channeling surplus capital into high-yield savings goals or long-term investments.'
          : rate >= 20
          ? 'Healthy balance maintained. You are meeting standard recommended 50/30/20 financial guidelines.'
          : 'High expenditure ratio detected relative to monthly income. Review recurring debits to optimize cash flow.';

        insightsContainer.innerHTML = `
          <div style="padding: 1rem; background: rgba(255,255,255,0.03); border-radius: var(--radius-md); border-left: 3px solid var(--accent-cyan);">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.5rem;">
              <span style="font-weight: 600; font-size: 0.95rem;">Savings Health Score</span>
              ${healthBadge}
            </div>
            <p style="font-size: 0.85rem; color: var(--text-muted); line-height: 1.5; margin: 0;">${advice}</p>
          </div>

          <div style="padding: 1rem; background: rgba(255,255,255,0.03); border-radius: var(--radius-md); border-left: 3px solid #3b82f6;">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.5rem;">
              <span style="font-weight: 600; font-size: 0.95rem;">Primary Spending Driver</span>
              <span class="badge badge-secondary">${topCat}</span>
            </div>
            <p style="font-size: 0.85rem; color: var(--text-muted); line-height: 1.5; margin: 0;">
              Largest outgoing allocation is <strong>${topCat}</strong> totaling <strong>${topAmount}</strong> (${cats.length > 0 ? cats[0].percentage : 0}% of all outgoings).
            </p>
          </div>

          <div style="padding: 1rem; background: rgba(255,255,255,0.03); border-radius: var(--radius-md); border-left: 3px solid #10b981;">
            <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.5rem;">
              <span style="font-weight: 600; font-size: 0.95rem;">Capital Retention Strategy</span>
              <span class="badge badge-success">ACTIVE</span>
            </div>
            <p style="font-size: 0.85rem; color: var(--text-muted); line-height: 1.5; margin: 0;">
              You retained <strong>${summary.net_savings_formatted}</strong> this cycle. Keep maintaining emergency funds covering at least 3-6 months of average expenses.
            </p>
          </div>
        `;
      }

      if (window.lucide) lucide.createIcons();
    } catch (err) {
      console.error('Error loading analytics:', err);
      UI.showToast('error', 'Analytics Error', 'Failed to load consolidated analytics.');
    }
  },

  // 4. Accounts Page
  renderAccountsPage() {
    const container = document.getElementById('accounts-grid');
    if (!container) return;

    if (State.accounts.length === 0) {
      container.innerHTML = `<div style="grid-column: 1/-1; text-align: center; padding: 3rem; color: var(--text-dim);">No accounts found. Create your first account below.</div>`;
      return;
    }

    container.innerHTML = State.accounts.map(acc => {
      const isFrozen = acc.status === 'FROZEN';
      const statusBadge = isFrozen ? '<span class="badge badge-warning">FROZEN</span>' :
                          acc.status === 'ACTIVE' ? '<span class="badge badge-success">ACTIVE</span>' :
                          '<span class="badge badge-danger">SUSPENDED</span>';

      return `
        <div class="card">
          <div class="card-header">
            <div>
              <span class="badge badge-info">${acc.account_type}</span>
              <h3 style="font-size: 1.15rem; margin-top: 0.4rem; font-family: var(--font-mono);">${acc.account_number}</h3>
            </div>
            ${statusBadge}
          </div>
          <div style="margin: 1.25rem 0;">
            <div style="font-size: 0.8rem; color: var(--text-muted);">Available Balance</div>
            <div style="font-size: 1.85rem; font-weight: 700; color: var(--accent-cyan);">${acc.balance_formatted}</div>
            <div style="font-size: 0.75rem; color: var(--text-dim); margin-top: 0.2rem;">Branch: ${acc.branch_code}</div>
          </div>
          <div style="display: flex; gap: 0.5rem; margin-top: 1rem; border-top: 1px solid var(--border-color); padding-top: 1rem;">
            ${isFrozen ? 
              `<button class="btn btn-secondary btn-sm" onclick="App.unfreezeAccount(${acc.id})"><i data-lucide="unlock"></i> Reactivate</button>` :
              `<button class="btn btn-danger btn-sm" onclick="App.freezeAccount(${acc.id})"><i data-lucide="lock"></i> Emergency Freeze</button>`
            }
            <button class="btn btn-primary btn-sm" onclick="Router.navigate('transfer')"><i data-lucide="send"></i> Transfer</button>
          </div>
        </div>
      `;
    }).join('');

    if (window.lucide) lucide.createIcons();
  },

  async freezeAccount(accId) {
    if (!confirm('Are you sure you want to freeze this account? All outgoing withdrawals will be halted until reactivated.')) return;
    try {
      await API.freezeAccount(accId, 'Emergency Customer Freeze');
      UI.showToast('warning', 'Account Frozen', 'Account has been frozen for your security.');
      await this.loadAccounts();
      this.renderAccountsPage();
    } catch (err) {
      UI.showToast('error', 'Action Failed', err.message);
    }
  },

  async unfreezeAccount(accId) {
    try {
      await API.unfreezeAccount(accId);
      UI.showToast('success', 'Account Reactivated', 'Account is now active for transactions.');
      await this.loadAccounts();
      this.renderAccountsPage();
    } catch (err) {
      UI.showToast('error', 'Action Failed', err.message);
    }
  },

  // 5. Banking Forms Setup
  setupBankingForms() {
    // Deposit Form
    const depositForm = document.getElementById('form-deposit');
    if (depositForm) {
      depositForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const account_id = parseInt(document.getElementById('deposit-account-select').value);
        const amount_inr = document.getElementById('deposit-amount').value;
        const amount_paise = UI.rupeesToPaise(amount_inr);
        const description = document.getElementById('deposit-desc').value.trim();
        const category = document.getElementById('deposit-cat').value;

        if (amount_paise <= 0) {
          UI.showToast('error', 'Invalid Amount', 'Please enter a valid deposit amount.');
          return;
        }

        try {
          const res = await API.deposit({ account_id, amount_paise, description, category });
          UI.showToast('success', 'Deposit Confirmed', `Ref: ${res.reference_id}. New Balance: ${UI.formatINR(res.balance_after_paise)}`);
          depositForm.reset();
          await this.loadAccounts();
          Router.navigate('dashboard');
        } catch (err) {
          UI.showToast('error', 'Deposit Failed', err.message);
        }
      });
    }

    // Withdrawal Form
    const withdrawForm = document.getElementById('form-withdraw');
    if (withdrawForm) {
      withdrawForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const account_id = parseInt(document.getElementById('withdraw-account-select').value);
        const amount_inr = document.getElementById('withdraw-amount').value;
        const amount_paise = UI.rupeesToPaise(amount_inr);
        const pin = document.getElementById('withdraw-pin').value.trim();
        const description = document.getElementById('withdraw-desc').value.trim();
        const category = document.getElementById('withdraw-cat').value;

        if (amount_paise <= 0) {
          UI.showToast('error', 'Invalid Amount', 'Please enter a valid withdrawal amount.');
          return;
        }

        try {
          const res = await API.withdraw({ account_id, amount_paise, pin, description, category });
          UI.showToast('success', 'Withdrawal Confirmed', `Ref: ${res.reference_id}. Remaining: ${UI.formatINR(res.balance_after_paise)}`);
          withdrawForm.reset();
          await this.loadAccounts();
          Router.navigate('dashboard');
        } catch (err) {
          UI.showToast('error', 'Withdrawal Denied', err.message);
        }
      });
    }

    // Transfer Form
    const transferForm = document.getElementById('form-transfer');
    if (transferForm) {
      transferForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const from_account_id = parseInt(document.getElementById('transfer-from-select').value);
        const to_account_number = document.getElementById('transfer-to-acc').value.trim();
        const amount_inr = document.getElementById('transfer-amount').value;
        const amount_paise = UI.rupeesToPaise(amount_inr);
        const pin = document.getElementById('transfer-pin').value.trim();
        const description = document.getElementById('transfer-desc').value.trim();
        const category = document.getElementById('transfer-cat').value;

        if (amount_paise <= 0) {
          UI.showToast('error', 'Invalid Amount', 'Transfer amount must be greater than zero.');
          return;
        }

        try {
          const res = await API.transfer({ from_account_id, to_account_number, amount_paise, pin, description, category });
          if (res.is_flagged) {
            UI.showToast('warning', 'High-Value Transfer Flagged', 'Transfer recorded. Due to high value (> ₹1,00,000), it has been queued for compliance monitoring.');
          } else {
            UI.showToast('success', 'Transfer Successful', `Funds successfully sent. Ref: ${res.reference_id}`);
          }
          transferForm.reset();
          await this.loadAccounts();
          Router.navigate('dashboard');
        } catch (err) {
          UI.showToast('error', 'Transfer Failed', err.message);
        }
      });
    }

    // New Account Creation Modal Form
    const newAccForm = document.getElementById('form-new-account');
    if (newAccForm) {
      newAccForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const account_type = document.getElementById('newacc-type').value;
        const deposit_inr = document.getElementById('newacc-deposit').value;
        const initial_deposit_paise = UI.rupeesToPaise(deposit_inr);

        try {
          const res = await API.createAccount({ account_type, initial_deposit_paise });
          UI.showToast('success', 'Account Opened', `New ${res.account.account_type} account ${res.account.account_number} created!`);
          UI.closeModal('modal-new-account');
          newAccForm.reset();
          await this.loadAccounts();
          this.renderAccountsPage();
        } catch (err) {
          UI.showToast('error', 'Error', err.message);
        }
      });
    }

    // Monthly Statement Form
    const stmtForm = document.getElementById('form-generate-statement');
    if (stmtForm) {
      stmtForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const accId = document.getElementById('stmt-account-select').value;
        const year = document.getElementById('stmt-year').value;
        const month = document.getElementById('stmt-month').value;

        try {
          const stmt = await API.getStatement(accId, year, month);
          this.renderStatementResult(stmt);
        } catch (err) {
          UI.showToast('error', 'Statement Error', err.message);
        }
      });
    }

    // Service Request Form
    const srForm = document.getElementById('form-service-request');
    if (srForm) {
      srForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const account_id = parseInt(document.getElementById('sr-account-select').value);
        const request_type = document.getElementById('sr-type-select').value;
        const priority = document.getElementById('sr-priority-select').value;
        const description = document.getElementById('sr-desc').value.trim();

        try {
          await API.createServiceRequest({ account_id, request_type, priority, description });
          UI.showToast('success', 'Ticket Submitted', 'Your service request has been queued for bank officer action.');
          srForm.reset();
        } catch (err) {
          UI.showToast('error', 'Submission Failed', err.message);
        }
      });
    }
  },

  renderStatementResult(stmt) {
    const container = document.getElementById('statement-preview-area');
    if (!container) return;

    if (stmt.error) {
      UI.showToast('error', 'Statement Error', stmt.error);
      return;
    }

    container.style.display = 'block';
    document.getElementById('stmt-disp-acc').textContent = `${stmt.account_type || 'ACCOUNT'} - ${stmt.account_number || ''}`;
    document.getElementById('stmt-disp-period').textContent = `Statement Cycle: ${stmt.statement_period || 'N/A'}`;
    document.getElementById('stmt-disp-credits').textContent = stmt.total_deposits || '₹0.00';
    document.getElementById('stmt-disp-debits').textContent = stmt.total_withdrawals || '₹0.00';
    document.getElementById('stmt-disp-closing').textContent = stmt.closing_balance || '₹0.00';

    const tbody = document.getElementById('stmt-txns-body');
    if (stmt.transactions && stmt.transactions.length > 0) {
      tbody.innerHTML = stmt.transactions.map(t => `
        <tr>
          <td>${UI.formatDate(t.date)}</td>
          <td><strong style="font-family: var(--font-mono); font-size: 0.85rem;">${t.ref}</strong></td>
          <td><span class="badge badge-secondary">${t.type}</span></td>
          <td style="color: var(--text-muted);">${t.desc || '-'}</td>
          <td class="${t.is_debit ? 'amount-debit' : 'amount-credit'}" style="font-weight: 700;">${t.is_debit ? '-' : '+'}${t.amount}</td>
          <td style="font-family: var(--font-mono); font-size: 0.85rem;">${t.balance_after}</td>
        </tr>
      `).join('');
    } else {
      tbody.innerHTML = `<tr><td colspan="6" style="text-align: center; color: var(--text-dim); padding: 1.5rem;">No activity during this statement period.</td></tr>`;
    }

    UI.showToast('success', 'Statement Ready', `Generated monthly statement for ${stmt.statement_period}`);
    container.scrollIntoView({ behavior: 'smooth' });
  },

  // 6. Savings Goals & Contributions
  async loadSavingsPage() {
    try {
      const goals = await API.getSavingsGoals();
      const container = document.getElementById('savings-goals-grid');
      if (!container) return;

      if (goals.length === 0) {
        container.innerHTML = `<div style="grid-column: 1/-1; text-align: center; padding: 3rem; color: var(--text-dim);">No savings goals set. Create one to begin tracking targets!</div>`;
        return;
      }

      container.innerHTML = goals.map(g => {
        return `
          <div class="card">
            <div class="card-header">
              <span class="badge badge-info">${g.category}</span>
              <span class="badge ${g.status === 'COMPLETED' ? 'badge-success' : 'badge-warning'}">${g.status}</span>
            </div>
            <h3 style="font-size: 1.2rem; margin-bottom: 0.5rem;">${g.title}</h3>
            <div style="font-size: 0.85rem; color: var(--text-muted); margin-bottom: 1rem;">Linked Account: ${g.account_number}</div>
            
            <div style="margin-bottom: 1rem;">
              <div style="display: flex; justify-content: space-between; font-size: 0.85rem; margin-bottom: 0.35rem;">
                <span style="font-weight: 600; color: var(--accent-cyan);">${g.current_formatted}</span>
                <span style="color: var(--text-muted);">${g.target_formatted} (${g.progress_pct.toFixed(1)}%)</span>
              </div>
              <div style="height: 8px; background: rgba(255,255,255,0.06); border-radius: 999px; overflow: hidden;">
                <div style="width: ${g.progress_pct}%; height: 100%; background: var(--accent-gradient); border-radius: 999px;"></div>
              </div>
            </div>

            <div style="display: flex; justify-content: space-between; align-items: center; border-top: 1px solid var(--border-color); padding-top: 1rem; margin-top: 1rem;">
              <div style="font-size: 0.75rem; color: var(--text-dim);">Target Date: ${g.target_date}</div>
              ${g.status !== 'COMPLETED' ? `
                <button class="btn btn-primary btn-sm" onclick="App.openContributeModal(${g.id}, '${g.title}')">
                  <i data-lucide="plus-circle"></i> Contribute
                </button>
              ` : '<span style="color: var(--color-success); font-weight: 700; font-size: 0.85rem;">Goal Met!</span>'}
            </div>
          </div>
        `;
      }).join('');

      if (window.lucide) lucide.createIcons();
    } catch (err) {
      console.error(err);
    }
  },

  openContributeModal(goalId, title) {
    document.getElementById('contrib-goal-id').value = goalId;
    document.getElementById('contrib-goal-title').textContent = title;
    UI.openModal('modal-contribute-goal');
  },

  // 7. Virtual Debit Cards
  formatCardNumber(cardNum) {
    if (!cardNum) return '<span class="card-num-group">••••</span> <span class="card-num-dots">&bull;&bull;&bull;&bull;</span> <span class="card-num-dots">&bull;&bull;&bull;&bull;</span> <span class="card-num-group">••••</span>';
    const digits = cardNum.match(/\d+/g);
    if (digits && digits.length >= 2) {
      const first4 = digits[0].slice(0, 4);
      const last4 = digits[digits.length - 1].slice(-4);
      return `<span class="card-num-group">${first4}</span> <span class="card-num-dots">&bull;&bull;&bull;&bull;</span> <span class="card-num-dots">&bull;&bull;&bull;&bull;</span> <span class="card-num-group">${last4}</span>`;
    }
    const clean = cardNum.replace(/ÔÇó/g, '&bull;').replace(/\*/g, '&bull;');
    return `<span class="card-num-group">${clean}</span>`;
  },

  async loadCardsPage() {
    try {
      const cards = await API.getCards();
      const container = document.getElementById('virtual-cards-container');
      if (!container) return;

      if (cards.length === 0) {
        container.innerHTML = `<div style="text-align: center; padding: 3rem; color: var(--text-dim);">No virtual cards issued yet. Click "Issue Simulated Card" below.</div>`;
        return;
      }

      container.innerHTML = cards.map(c => `
        <div style="display: flex; flex-direction: column; align-items: center; gap: 1rem;">
          <div class="virtual-card-wrapper">
            <div class="virtual-debit-card" style="${c.status === 'FROZEN' ? 'filter: grayscale(0.85); opacity: 0.7;' : ''}">
              <span class="card-simulated-tag">SIMULATED CARD</span>
              <div style="display: flex; justify-content: space-between; align-items: center;">
                <div class="card-chip"></div>
                <div style="font-weight: 800; font-size: 1.15rem; letter-spacing: 0.05em; color: var(--accent-cyan);">${c.type}</div>
              </div>
              <div class="card-number-display">${this.formatCardNumber(c.card_number)}</div>
              <div class="card-meta-row">
                <div>
                  <div class="card-holder-label">Card Holder</div>
                  <div class="card-holder-name">${State.user ? State.user.name : 'CUSTOMER'}</div>
                </div>
                <div>
                  <div class="card-holder-label">Expires</div>
                  <div style="font-family: var(--font-mono); font-size: 0.9rem;">${c.expiry_month.toString().padStart(2, '0')}/${c.expiry_year}</div>
                </div>
              </div>
            </div>
          </div>

          <div style="display: flex; gap: 1rem; align-items: center;">
            <span class="badge ${c.status === 'ACTIVE' ? 'badge-success' : 'badge-warning'}">${c.status}</span>
            <span style="font-size: 0.85rem; color: var(--text-muted);">Daily Limit: ${c.spending_limit_formatted}</span>
            <button class="btn btn-secondary btn-sm" onclick="App.toggleCardFreeze(${c.id})">
              <i data-lucide="${c.status === 'ACTIVE' ? 'lock' : 'unlock'}"></i>
              ${c.status === 'ACTIVE' ? 'Freeze Card' : 'Unfreeze Card'}
            </button>
          </div>
        </div>
      `).join('');

      if (window.lucide) lucide.createIcons();
    } catch (err) {
      console.error(err);
    }
  },

  // 7.1 Notifications Page
  async loadNotificationsPage() {
    try {
      const notifs = await API.getNotifications();
      const container = document.getElementById('notifications-full-list');
      if (!container) return;

      if (!notifs || notifs.length === 0) {
        container.innerHTML = `<div style="text-align: center; color: var(--text-dim); padding: 3rem;">No notifications in your feed.</div>`;
        return;
      }

      container.innerHTML = notifs.map(n => {
        const isUnread = !n.is_read;
        const icon = n.type === 'FRAUD_ALERT' || n.type === 'SECURITY' ? 'shield-alert' :
                     n.type === 'TRANSACTION' ? 'arrow-left-right' : 'bell';
        const iconColor = n.type === 'FRAUD_ALERT' ? 'var(--color-danger)' :
                          n.type === 'SECURITY' ? 'var(--color-warning)' : 'var(--accent-cyan)';

        return `
          <div class="card" style="padding: 1.25rem; display: flex; gap: 1rem; align-items: flex-start; ${isUnread ? 'border-left: 3px solid var(--accent-cyan); background: rgba(0, 229, 255, 0.02);' : 'opacity: 0.85;'}">
            <div style="padding: 0.5rem; background: rgba(255,255,255,0.04); border-radius: 50%; color: ${iconColor};">
              <i data-lucide="${icon}"></i>
            </div>
            <div style="flex: 1;">
              <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 0.35rem;">
                <h4 style="font-size: 0.95rem; font-weight: 700; ${isUnread ? 'color: var(--text-main);' : 'color: var(--text-muted);'}">${n.title}</h4>
                <span style="font-size: 0.75rem; color: var(--text-dim);">${UI.formatDate(n.created_at)}</span>
              </div>
              <p style="font-size: 0.85rem; color: var(--text-muted); line-height: 1.4; margin: 0;">${n.message}</p>
            </div>
            ${isUnread ? `
              <button class="btn btn-secondary btn-sm" onclick="API.markNotificationRead(${n.id}).then(() => { App.loadNotificationsPage(); App.loadNotifications(); })" title="Mark as Read">
                <i data-lucide="check"></i>
              </button>
            ` : ''}
          </div>
        `;
      }).join('');

      if (window.lucide) lucide.createIcons();
    } catch (err) {
      console.error('Error loading notifications:', err);
    }
  },

  async toggleCardFreeze(cardId) {
    try {
      await API.toggleCardFreeze(cardId);
      UI.showToast('info', 'Virtual Card', 'Virtual card status toggled.');
      await this.loadCardsPage();
    } catch (err) {
      UI.showToast('error', 'Error', err.message);
    }
  },

  // 8. Beneficiaries
  async loadBeneficiariesPage() {
    try {
      const list = await API.getBeneficiaries();
      const container = document.getElementById('beneficiaries-list');
      if (!container) return;

      if (list.length === 0) {
        container.innerHTML = `<tr><td colspan="6" style="text-align: center; color: var(--text-dim); padding: 2rem;">No saved beneficiaries found.</td></tr>`;
        return;
      }

      container.innerHTML = list.map(b => `
        <tr>
          <td><strong>${b.name}</strong></td>
          <td style="font-family: var(--font-mono);">${b.account_number}</td>
          <td>${b.bank_name}</td>
          <td><span class="badge badge-info">${b.ifsc}</span></td>
          <td>${b.nickname || 'N/A'}</td>
          <td>
            <button class="btn btn-danger btn-sm" onclick="App.deleteBeneficiary(${b.id})">Remove</button>
            <button class="btn btn-primary btn-sm" onclick="App.quickTransferTo('${b.account_number}')">Send Money</button>
          </td>
        </tr>
      `).join('');
    } catch (err) {
      console.error(err);
    }
  },

  async deleteBeneficiary(id) {
    if (!confirm('Remove this beneficiary from your quick list?')) return;
    try {
      await API.deleteBeneficiary(id);
      UI.showToast('success', 'Removed', 'Beneficiary deleted.');
      await this.loadBeneficiariesPage();
    } catch (err) {
      UI.showToast('error', 'Error', err.message);
    }
  },

  quickTransferTo(accNum) {
    Router.navigate('transfer');
    setTimeout(() => {
      const inp = document.getElementById('transfer-to-acc');
      if (inp) inp.value = accNum;
    }, 150);
  },

  // 9. AI Assistant
  setupAIAssistant() {
    const aiForm = document.getElementById('ai-chat-form');
    if (aiForm) {
      aiForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const input = document.getElementById('ai-prompt-input');
        const prompt = input.value.trim();
        if (!prompt) return;

        const chatBox = document.getElementById('ai-messages-list');
        // Add User Bubble
        const userDiv = document.createElement('div');
        userDiv.className = 'chat-bubble chat-bubble-user';
        userDiv.textContent = prompt;
        chatBox.appendChild(userDiv);
        input.value = '';
        chatBox.scrollTop = chatBox.scrollHeight;

        try {
          const res = await API.askAI(prompt);
          const aiDiv = document.createElement('div');
          aiDiv.className = 'chat-bubble chat-bubble-ai';
          aiDiv.innerHTML = `
            <div style="font-weight: 700; color: var(--accent-cyan); font-size: 0.75rem; margin-bottom: 0.25rem;">
              <i data-lucide="bot"></i> SMART ADVISOR (Deterministic Engine)
            </div>
            <div>${res.message.replace(/\*\*(.*?)\*\*/g, '<strong>$1</strong>')}</div>
            ${res.actionable_tip ? `<div style="margin-top: 0.65rem; padding: 0.5rem; background: rgba(255,255,255,0.05); border-radius: 6px; font-size: 0.85rem; border-left: 3px solid var(--accent-cyan);"><i data-lucide="lightbulb"></i> <strong>Pro Tip:</strong> ${res.actionable_tip}</div>` : ''}
          `;
          chatBox.appendChild(aiDiv);
          if (window.lucide) lucide.createIcons();
          chatBox.scrollTop = chatBox.scrollHeight;
        } catch (err) {
          UI.showToast('error', 'AI Assistant', err.message);
        }
      });
    }
  },

  // 10. Security & Profile Forms
  setupSecurityForms() {
    const pwdForm = document.getElementById('form-change-password');
    if (pwdForm) {
      pwdForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const old_password = document.getElementById('sec-old-pwd').value;
        const new_password = document.getElementById('sec-new-pwd').value;
        try {
          await API.changePassword({ old_password, new_password });
          UI.showToast('success', 'Security Updated', 'Password successfully changed via Argon2id.');
          pwdForm.reset();
        } catch (err) {
          UI.showToast('error', 'Error', err.message);
        }
      });
    }

    const pinForm = document.getElementById('form-set-pin');
    if (pinForm) {
      pinForm.addEventListener('submit', async (e) => {
        e.preventDefault();
        const pin = document.getElementById('sec-txn-pin').value.trim();
        try {
          await API.setPin(pin);
          UI.showToast('success', 'PIN Updated', 'Transaction PIN has been updated.');
          pinForm.reset();
        } catch (err) {
          UI.showToast('error', 'Error', err.message);
        }
      });
    }
  },

  renderProfilePage() {
    if (!State.user) return;
    document.getElementById('prof-name').textContent = State.user.name;
    document.getElementById('prof-cust-num').textContent = State.user.customer_number || 'N/A';
    document.getElementById('prof-email').textContent = State.user.email;
    document.getElementById('prof-phone').textContent = State.user.phone || '+91 98765 43210';
    document.getElementById('prof-status').textContent = State.user.status;
  },

  async loadLoginHistoryPage() {
    try {
      const history = await API.getLoginHistory();
      const container = document.getElementById('login-history-table');
      if (!container) return;

      container.innerHTML = history.map(h => `
        <tr>
          <td>${UI.formatDate(h.login_at)}</td>
          <td><span class="badge ${h.status === 'SUCCESS' ? 'badge-success' : 'badge-danger'}">${h.status}</span></td>
          <td style="font-family: var(--font-mono);">${h.ip_address}</td>
          <td style="color: var(--text-muted);">${h.user_agent}</td>
        </tr>
      `).join('');
    } catch (err) {
      console.error(err);
    }
  },

  async loadDSDemoPage() {
    try {
      const report = await API.getDSDemo();
      document.getElementById('ds-demo-raw').textContent = JSON.stringify(report, null, 2);
    } catch (err) {
      console.error(err);
    }
  }
};

window.addEventListener('DOMContentLoaded', () => {
  App.init();
});

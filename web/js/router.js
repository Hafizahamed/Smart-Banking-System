/**
 * SMART BANKING MANAGEMENT SYSTEM - CLIENT ROUTER
 */

const Router = {
  routes: [
    'dashboard', 'accounts', 'deposit', 'withdraw', 'transfer',
    'transactions', 'analytics', 'savings', 'beneficiaries',
    'cards', 'notifications', 'statements', 'profile', 'security',
    'login-history', 'ai-assistant', 'qr-transfer', 'ds-demo',
    'login', 'register'
  ],

  init() {
    window.addEventListener('hashchange', () => this.handleRoute());
    this.handleRoute();
  },

  navigate(viewName) {
    window.location.hash = `#${viewName}`;
  },

  handleRoute() {
    let hash = window.location.hash.replace('#', '').trim();
    if (!hash || !this.routes.includes(hash)) {
      hash = State.user ? 'dashboard' : 'login';
    }

    // Auth Guard
    const isPublic = (hash === 'login' || hash === 'register');
    if (!State.user && !isPublic) {
      window.location.hash = '#login';
      return;
    }
    if (State.user && isPublic) {
      window.location.hash = '#dashboard';
      return;
    }

    // Toggle Sidebar & Header visibility for auth pages vs app
    const sidebar = document.querySelector('.app-sidebar');
    const header = document.querySelector('.top-header');
    const mainWrapper = document.querySelector('.main-wrapper');

    if (isPublic) {
      if (sidebar) sidebar.style.display = 'none';
      if (header) header.style.display = 'none';
      if (mainWrapper) mainWrapper.style.marginLeft = '0';
    } else {
      if (sidebar) sidebar.style.display = 'flex';
      if (header) header.style.display = 'flex';
      if (mainWrapper) mainWrapper.style.marginLeft = '';
    }

    // Auto-close mobile sidebar drawer upon route navigation
    if (typeof UI !== 'undefined' && UI.closeSidebar) {
      UI.closeSidebar();
    }

    // Hide all view sections
    document.querySelectorAll('.view-section').forEach(sec => {
      sec.classList.remove('active');
    });

    // Show target section
    const targetSection = document.getElementById(`view-${hash}`);
    if (targetSection) {
      targetSection.classList.add('active');
    }

    // Update active nav item
    document.querySelectorAll('.nav-item').forEach(item => {
      const target = item.getAttribute('href');
      if (target === `#${hash}`) {
        item.classList.add('active');
      } else {
        item.classList.remove('active');
      }
    });

    // Update Header Title
    const headerTitle = document.getElementById('page-header-title');
    if (headerTitle) {
      const titles = {
        'dashboard': 'Financial Overview',
        'accounts': 'My Bank Accounts',
        'deposit': 'Deposit Funds',
        'withdraw': 'Withdraw Funds',
        'transfer': 'Internal Fund Transfer',
        'transactions': 'Transaction History & Ledger',
        'analytics': 'Financial Intelligence & Insights',
        'savings': 'Savings Goals & Challenges',
        'beneficiaries': 'Beneficiary Management',
        'cards': 'Simulated Virtual Debit Cards',
        'notifications': 'Alerts & Messages',
        'statements': 'Account Statements',
        'profile': 'Customer Profile',
        'security': 'Security & Credentials',
        'login-history': 'Device & Login Audit History',
        'ai-assistant': 'AI Financial Advisor',
        'qr-transfer': 'QR Code Transfer Simulation',
        'ds-demo': 'C Core Data Structures Benchmark'
      };
      headerTitle.textContent = titles[hash] || 'Smart Banking';
    }

    // Trigger view-specific data refresh
    window.dispatchEvent(new CustomEvent('route-changed', { detail: hash }));

    // Re-initialize Lucide icons
    if (window.lucide) {
      lucide.createIcons();
    }
  }
};

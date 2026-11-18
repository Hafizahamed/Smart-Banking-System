/**
 * SMART BANKING MANAGEMENT SYSTEM - GLOBAL STATE
 */

const State = {
  user: null,
  accounts: [],
  selectedAccountId: null,
  notifications: [],
  unreadNotificationsCount: 0,
  theme: localStorage.getItem('smart_bank_theme') || 'dark',

  setUser(user) {
    this.user = user;
    window.dispatchEvent(new CustomEvent('user-updated', { detail: user }));
  },

  setAccounts(accounts) {
    this.accounts = accounts || [];
    if (this.accounts.length > 0 && !this.selectedAccountId) {
      this.selectedAccountId = this.accounts[0].id;
    }
    window.dispatchEvent(new CustomEvent('accounts-updated', { detail: this.accounts }));
  },

  getActiveAccount() {
    return this.accounts.find(a => a.id === this.selectedAccountId) || this.accounts[0] || null;
  },

  setTheme(newTheme) {
    this.theme = newTheme;
    localStorage.setItem('smart_bank_theme', newTheme);
    document.documentElement.setAttribute('data-theme', newTheme);
    window.dispatchEvent(new CustomEvent('theme-changed', { detail: newTheme }));
  }
};

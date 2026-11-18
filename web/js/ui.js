/**
 * SMART BANKING MANAGEMENT SYSTEM - UI HELPERS & NOTIFICATIONS
 */

const UI = {
  // Toast Notifications
  showToast(type, title, message) {
    const container = document.getElementById('toast-container');
    if (!container) return;

    const toast = document.createElement('div');
    toast.className = `toast toast-${type}`;
    
    const iconName = type === 'success' ? 'check-circle' : type === 'error' ? 'alert-triangle' : 'info';
    
    toast.innerHTML = `
      <i data-lucide="${iconName}" style="color: ${type === 'success' ? 'var(--color-success)' : type === 'error' ? 'var(--color-danger)' : 'var(--accent-cyan)'}"></i>
      <div style="flex: 1;">
        <div style="font-weight: 700; font-size: 0.9rem; margin-bottom: 2px;">${title}</div>
        <div style="font-size: 0.8rem; color: var(--text-muted);">${message}</div>
      </div>
    `;

    container.appendChild(toast);
    if (window.lucide) lucide.createIcons();

    setTimeout(() => {
      toast.style.opacity = '0';
      toast.style.transform = 'translateX(50px)';
      toast.style.transition = 'all 0.3s ease';
      setTimeout(() => toast.remove(), 300);
    }, 4500);
  },

  // Modals
  openModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) {
      modal.classList.add('open');
      if (window.lucide) lucide.createIcons();
    }
  },

  closeModal(modalId) {
    const modal = document.getElementById(modalId);
    if (modal) {
      modal.classList.remove('open');
    }
  },

  // Sidebar Drawer Navigation Controls
  openSidebar() {
    const sidebar = document.querySelector('.app-sidebar');
    const backdrop = document.getElementById('sidebar-backdrop');
    if (window.innerWidth <= 1024) {
      if (sidebar) sidebar.classList.add('open');
      if (backdrop) backdrop.classList.add('active');
    } else {
      document.body.classList.remove('sidebar-collapsed');
    }
  },

  closeSidebar() {
    const sidebar = document.querySelector('.app-sidebar');
    const backdrop = document.getElementById('sidebar-backdrop');
    if (sidebar) sidebar.classList.remove('open');
    if (backdrop) backdrop.classList.remove('active');
  },

  toggleSidebar() {
    const sidebar = document.querySelector('.app-sidebar');
    if (window.innerWidth <= 1024) {
      if (sidebar && sidebar.classList.contains('open')) {
        this.closeSidebar();
      } else {
        this.openSidebar();
      }
    } else {
      document.body.classList.toggle('sidebar-collapsed');
    }
  },

  // Currency Formatter: Converts paise to Indian Rupees string
  formatINR(paise) {
    const isNeg = paise < 0;
    const abs = Math.abs(paise);
    const rupees = Math.floor(abs / 100);
    const coins = (abs % 100).toString().padStart(2, '0');

    // Indian numbering format (1,00,000)
    let s = rupees.toString();
    if (s.length > 3) {
      const last3 = s.slice(-3);
      const otherDigits = s.slice(0, -3);
      s = otherDigits.replace(/\B(?=(\d{2})+(?!\d))/g, ",") + "," + last3;
    }
    return `${isNeg ? '-' : ''}₹${s}.${coins}`;
  },

  // Rupees input to integer paise
  rupeesToPaise(val) {
    const num = parseFloat(val);
    if (isNaN(num) || num <= 0) return 0;
    return Math.round(num * 100);
  },

  formatDate(dateStr) {
    if (!dateStr) return 'N/A';
    const d = new Date(dateStr);
    return d.toLocaleString('en-IN', {
      day: '2-digit', month: 'short', year: 'numeric',
      hour: '2-digit', minute: '2-digit'
    });
  }
};

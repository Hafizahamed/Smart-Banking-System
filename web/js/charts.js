/**
 * SMART BANKING MANAGEMENT SYSTEM - CHART.JS CONTROLLER
 */

const BankingCharts = {
  charts: {},

  initSpendingTrends(canvasId, trendsData) {
    const ctx = document.getElementById(canvasId);
    if (!ctx || !window.Chart) return;

    if (this.charts[canvasId]) {
      this.charts[canvasId].destroy();
      delete this.charts[canvasId];
    }

    const labels = (trendsData || []).map(d => d.month);
    const incomeData = (trendsData || []).map(d => d.income_paise / 100);
    const expenseData = (trendsData || []).map(d => d.expense_paise / 100);

    const isDark = State.theme === 'dark';
    const gridColor = isDark ? 'rgba(255, 255, 255, 0.05)' : 'rgba(0, 0, 0, 0.06)';
    const textColor = isDark ? '#94a3b8' : '#64748b';

    this.charts[canvasId] = new Chart(ctx, {
      type: 'bar',
      data: {
        labels: labels,
        datasets: [
          {
            label: 'Income (₹)',
            data: incomeData,
            backgroundColor: 'rgba(16, 185, 129, 0.75)',
            borderColor: '#10b981',
            borderWidth: 1.5,
            borderRadius: 6
          },
          {
            label: 'Expenses (₹)',
            data: expenseData,
            backgroundColor: 'rgba(239, 68, 68, 0.75)',
            borderColor: '#ef4444',
            borderWidth: 1.5,
            borderRadius: 6
          }
        ]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
          legend: {
            labels: { color: textColor, font: { family: 'Inter', weight: 600 } }
          },
          tooltip: {
            callbacks: {
              label: (context) => ` ${context.dataset.label}: ₹${Number(context.raw || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 })}`
            }
          }
        },
        scales: {
          x: { grid: { color: gridColor }, ticks: { color: textColor } },
          y: { grid: { color: gridColor }, ticks: { color: textColor } }
        }
      }
    });
  },

  initCategoryDoughnut(canvasId, categoryData) {
    const ctx = document.getElementById(canvasId);
    if (!ctx || !window.Chart) return;

    if (this.charts[canvasId]) {
      this.charts[canvasId].destroy();
      delete this.charts[canvasId];
    }

    const cats = categoryData || [];
    const labels = cats.map(c => c.category);
    const data = cats.map(c => c.amount_paise / 100);
    const colors = [
      '#00e5ff', '#3b82f6', '#8b5cf6', '#ec4899', '#f59e0b', '#10b981', '#06b6d4', '#64748b'
    ];

    const isDark = State.theme === 'dark';
    const textColor = isDark ? '#f8fafc' : '#0f172a';

    this.charts[canvasId] = new Chart(ctx, {
      type: 'doughnut',
      data: {
        labels: labels.length ? labels : ['No Expenses Yet'],
        datasets: [{
          data: data.length ? data : [1],
          backgroundColor: data.length ? colors.slice(0, data.length) : ['#334155'],
          borderWidth: 2,
          borderColor: isDark ? '#080c16' : '#ffffff'
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        cutout: '70%',
        plugins: {
          legend: {
            position: 'bottom',
            labels: { color: textColor, boxWidth: 12, padding: 14 }
          },
          tooltip: {
            callbacks: {
              label: (context) => ` ₹${Number(context.raw || 0).toLocaleString('en-IN', { minimumFractionDigits: 2 })}`
            }
          }
        }
      }
    });
  }
};

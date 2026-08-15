/* OneSky Bank — frontend interactions: toasts, tabs, EMI calculator, charts. */

(function () {
  "use strict";

  /* ---------- Indian rupee formatter (paise -> ₹ string) ---------- */
  function inr(paise) {
    const neg = paise < 0;
    paise = Math.abs(paise);
    const rupees = Math.floor(paise / 100);
    const p = paise % 100;
    let s = rupees.toString();
    let tail = s.slice(-3);
    let head = s.slice(0, -3);
    const parts = [];
    while (head.length > 2) {
      parts.unshift(head.slice(-2));
      head = head.slice(0, -2);
    }
    if (head) parts.unshift(head);
    const body = (parts.length ? parts.join(",") + "," : "") + tail + "." + String(p).padStart(2, "0");
    return (neg ? "-₹" : "₹") + body;
  }

  /* ---------- Flash toasts: dismiss + auto-hide ---------- */
  function setupToasts() {
    document.querySelectorAll(".flash").forEach(function (flash) {
      const close = flash.querySelector(".flash-close");
      const hide = function () {
        flash.style.transition = "opacity .25s ease, transform .25s ease";
        flash.style.opacity = "0";
        flash.style.transform = "translateY(-8px)";
        setTimeout(function () { flash.remove(); }, 260);
      };
      if (close) close.addEventListener("click", hide);
      setTimeout(hide, 5200);
    });
  }

  /* ---------- Tabbed panels ---------- */
  function setupTabs() {
    document.querySelectorAll("[data-tabs]").forEach(function (root) {
      const buttons = root.querySelectorAll(".tab-btn");
      buttons.forEach(function (btn) {
        btn.addEventListener("click", function () {
          const target = btn.dataset.tab;
          buttons.forEach(function (b) {
            b.classList.toggle("active", b.dataset.tab === target);
          });
          root.querySelectorAll(".tab-panel").forEach(function (panel) {
            panel.classList.toggle("active", panel.dataset.panel === target);
          });
        });
      });
    });
  }

  /* ---------- Live EMI calculator ---------- */
  function setupEmiCalculator() {
    const amountEl = document.getElementById("emi-amount");
    const rateEl = document.getElementById("emi-rate");
    const monthsEl = document.getElementById("emi-months");
    const quote = document.getElementById("emi-quote");
    if (!amountEl || !rateEl || !monthsEl || !quote) return;

    function update() {
      const amount = amountEl.value;
      const rate = rateEl.value;
      const months = monthsEl.value;
      if (!amount || !rate || !months) {
        quote.style.display = "none";
        return;
      }
      fetch("/api/emi?principal=" + encodeURIComponent(amount) +
            "&rate=" + encodeURIComponent(rate) +
            "&months=" + encodeURIComponent(months), { credentials: "same-origin" })
        .then(function (r) { return r.json(); })
        .then(function (data) {
          if (!data.ok) { quote.style.display = "none"; return; }
          quote.style.display = "block";
          document.getElementById("emi-value").textContent = data.emi_display + "/month";
          document.getElementById("emi-total").textContent = data.total_display;
          document.getElementById("emi-interest").textContent = data.interest_display;
        })
        .catch(function () { quote.style.display = "none"; });
    }

    [amountEl, rateEl, monthsEl].forEach(function (el) {
      el.addEventListener("input", update);
    });
    update();
  }

  /* ---------- Chart global defaults ---------- */
  const CHART_COLORS = [
    "#6366f1", "#0ea5e9", "#0d9488", "#f59e0b", "#ec4899", "#8b5cf6", "#22c55e",
  ];

  if (window.Chart) {
    Chart.defaults.font.family =
      '"Inter", system-ui, -apple-system, "Segoe UI", sans-serif';
    Chart.defaults.color = "#64748b";
    Chart.defaults.borderColor = "rgba(148, 163, 184, .25)";
  }

  function baseTooltip() {
    return {
      mode: "index",
      intersect: false,
      callbacks: {
        label: function (ctx) {
          const value = Math.round(ctx.parsed.y ?? ctx.parsed);
          return " " + ctx.dataset.label + ": " + inr(value);
        },
      },
    };
  }

  /* ---------- Customer dashboard charts ---------- */
  function setupCustomerCharts() {
    const host = document.getElementById("customer-charts");
    if (!host || !window.Chart) return;

    fetch("/api/me/overview", { credentials: "same-origin" })
      .then(function (r) { return r.json(); })
      .then(function (data) {
        if (!data.ok) return;
        const acctCtx = document.getElementById("chart-account-mix");
        if (acctCtx && data.accounts.length > 1) {
          new Chart(acctCtx, {
            type: "doughnut",
            data: {
              labels: data.accounts.map(function (a) { return a.number; }),
              datasets: [{
                data: data.accounts.map(function (a) { return a.balance; }),
                backgroundColor: CHART_COLORS,
                borderWidth: 0,
              }],
            },
            options: {
              cutout: "68%",
              plugins: {
                tooltip: {
                  callbacks: {
                    label: function (ctx) {
                      return " " + ctx.label + ": " + inr(ctx.parsed);
                    },
                  },
                },
              },
            },
          });
        }

        const recentCtx = document.getElementById("chart-recent-activity");
        if (recentCtx) {
          const rows = data.recent.slice().reverse();
          new Chart(recentCtx, {
            type: "bar",
            data: {
              labels: rows.map(function (t) {
                return t.time.slice(11, 16);
              }),
              datasets: [{
                label: "Transaction",
                data: rows.map(function (t) {
                  const dir = t.type === "DEPOSIT" || t.type === "TRANSFER_IN" || t.type === "INTEREST" ? 1 : -1;
                  return dir * t.amount;
                }),
                backgroundColor: rows.map(function (t) {
                  return (t.type === "DEPOSIT" || t.type === "TRANSFER_IN" || t.type === "INTEREST")
                    ? "rgba(22, 163, 74, .75)" : "rgba(220, 38, 38, .75)";
                }),
                borderRadius: 6,
              }],
            },
            options: {
              plugins: { tooltip: baseTooltip(), legend: { display: false } },
              scales: {
                y: {
                  ticks: {
                    callback: function (v) { return inr(v * 100); },
                  },
                },
              },
            },
          });
        }
      })
      .catch(function () { /* charts are decorative; fail silently */ });
  }

  /* ---------- Admin dashboard charts ---------- */
  function setupAdminCharts() {
    const host = document.getElementById("admin-charts");
    if (!host || !window.Chart) return;

    fetch("/api/admin/overview", { credentials: "same-origin" })
      .then(function (r) { return r.json(); })
      .then(function (data) {
        if (!data.ok) return;

        const sevenCtx = document.getElementById("chart-seven-days");
        if (sevenCtx) {
          new Chart(sevenCtx, {
            type: "bar",
            data: {
              labels: data.seven_days.map(function (d) { return d.date; }),
              datasets: [
                {
                  label: "Volume",
                  data: data.seven_days.map(function (d) { return d.total; }),
                  backgroundColor: "rgba(99, 102, 241, .8)",
                  borderRadius: 6,
                },
                {
                  label: "Count",
                  data: data.seven_days.map(function (d) { return d.count * 1000; }),
                  backgroundColor: "rgba(14, 165, 233, .55)",
                  borderRadius: 6,
                },
              ],
            },
            options: {
              plugins: { tooltip: baseTooltip(), legend: { position: "bottom" } },
              scales: {
                y: {
                  ticks: {
                    callback: function (v) { return inr(v * 100); },
                  },
                },
              },
            },
          });
        }

        const typeCtx = document.getElementById("chart-tx-type");
        if (typeCtx) {
          new Chart(typeCtx, {
            type: "doughnut",
            data: {
              labels: data.by_type.map(function (t) { return t.type; }),
              datasets: [{
                data: data.by_type.map(function (t) { return t.total; }),
                backgroundColor: CHART_COLORS,
                borderWidth: 0,
              }],
            },
            options: {
              cutout: "62%",
              plugins: {
                tooltip: {
                  callbacks: {
                    label: function (ctx) {
                      return " " + ctx.label + ": " + inr(ctx.parsed);
                    },
                  },
                },
              },
            },
          });
        }

        const topCtx = document.getElementById("chart-top-accounts");
        if (topCtx) {
          new Chart(topCtx, {
            type: "bar",
            data: {
              labels: data.top_accounts.map(function (a) {
                return a.number + " · " + a.name.split(" ")[0];
              }),
              datasets: [{
                label: "Balance",
                data: data.top_accounts.map(function (a) { return a.balance; }),
                backgroundColor: "rgba(13, 148, 136, .8)",
                borderRadius: 6,
              }],
            },
            options: {
              indexAxis: "y",
              plugins: { tooltip: baseTooltip(), legend: { display: false } },
              scales: {
                x: {
                  ticks: {
                    callback: function (v) { return inr(v * 100); },
                  },
                },
              },
            },
          });
        }
      })
      .catch(function () { /* decorative */ });
  }

  /* ---------- Boot ---------- */
  document.addEventListener("DOMContentLoaded", function () {
    setupToasts();
    setupTabs();
    setupEmiCalculator();
    setupCustomerCharts();
    setupAdminCharts();
  });
})();

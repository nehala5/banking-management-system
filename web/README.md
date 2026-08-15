# OneSky Bank — Web Edition

A full-stack banking management system with a modern web frontend, built as the
web sibling of the C++ console version. Flask + SQLite backend, hand-rolled
HTML/CSS/JS frontend with live charts (Chart.js).

## Features

**Customers**
- Register as a new customer — a savings account is opened automatically
- Dashboard with total balance, quick actions, account cards and activity charts
- Deposit, withdraw and transfer money between accounts (own or other customers')
- Full transaction history with running balances per account
- Apply for loans with a **live EMI calculator**; pay EMIs one at a time
- **Fixed deposits** — open an FD with quarterly compounding, live maturity quote,
  early closure with penalty, auto-maturity crediting
- **Debit cards** — instant Luhn-valid card issuance, card withdrawals, PIN change,
  block/unblock
- **Notifications** — in-app alerts for every deposit, transfer, loan decision,
  EMI, FD and card event, with a bell badge and unread tracking
- Download a plain-text account statement (same format as the C++ version)

**Admins**
- Bank dashboard: deposits, loan book, loan-to-deposit ratio, pending approvals
- Customer directory with search and per-customer drill-down (accounts, loans,
  cards and fixed deposits)
- Open / close accounts, set savings interest rates
- Approve or reject loan applications (approval disburses to the account)
- Apply monthly interest to all savings accounts
- **FD desk** — full fixed-deposit book with one-click processing of matured deposits
- Reports: top accounts, transaction-type breakdown, daily volume charts

**Tech**
- Flask 3 + SQLite (stdlib `sqlite3`, no ORM) — all money stored as integer paise
- Sessions for auth, salted SHA-256 PINs, per-account ownership checks
- Chart.js + vanilla JS for charts, live EMI quotes and tabbed forms
- Responsive layout with a sky-to-indigo design system (no UI framework)

## Run it

```
pip install -r requirements.txt
python run.py
```

Open http://127.0.0.1:5000

Seeded demo logins (created on first run):

| Role     | User ID | PIN  |
|----------|---------|------|
| Admin    | 1000    | 1234 |
| Customer | 1001    | 0000 |

Delete the `data/` folder to wipe the database and re-seed fresh.

## Project layout

```
app/
  database.py          # SQLite schema + demo seed data
  money.py             # paise formatting, amount parsing, PIN hashing
  services/            # banking business logic (auth, accounts, txns, loans, reports)
  routes/              # Flask blueprints (public, customer, admin, JSON API)
  templates/           # Jinja pages (customer + admin portals, auth, errors)
  static/css/style.css # design system
  static/js/app.js     # toasts, tabs, EMI calculator, charts
run.py                 # entry point
```

## Notes

- Demo project — PINs are salted hashes but not a hardened KDF, and CSRF
  protection is intentionally omitted. Do not use for real banking.
- Chart.js loads from a CDN, so charts need internet access.

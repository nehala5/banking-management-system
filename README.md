# OneSky Bank — Banking Management System (C++17)

A complete console-based banking management system written in modern C++17.
Accounts, money movements, loans and reports are fully functional, with
file-based persistence so nothing is lost between runs.

Built as a layered, maintainable project rather than one giant `main.cpp`:
models → persistence → services → console UI.

---

## Features

### Authentication
- Admin and customer login with PIN verification (PINs are hashed, never stored in plaintext)
- New-customer registration with automatic savings account opening
- Pre-seeded demo accounts for instant exploration

### Accounts
- Open Savings / Current accounts, close accounts (guarded)
- Balances stored in **paise** (integer math — no floating-point money bugs)
- Monthly interest accrual on savings accounts, admin configurable rate

### Transactions
- Deposit, withdraw, and internal transfers (debit + credit audit entries)
- Full transaction trail with running balance, timestamp and remarks
- Daily activity volume tracking

### Loans
- Loan applications with **reducing-balance EMI calculation**
  `EMI = P·r·(1+r)ⁿ / ((1+r)ⁿ − 1)`
- Admin approval/rejection with principal disbursal into the account
- EMI payments with amortisation, due-date tracking and auto-close on final payment

### Reports (admin)
- Bank-wide overview (deposits, loan book, loan-to-deposit ratio)
- Daily activity, loan book, top accounts by balance
- Per-account statements and full customer summaries
- Customer-facing statement download to a text file

---

## Getting started

Requirements: a C++17 compiler. On Windows, [MinGW-w64](https://winlibs.com/) `g++` works out of the box.

**Windows**
```bat
build.bat
build\onesky_bank.exe
```
The Windows build is linked **statically** (`-static`), so the resulting
`onesky_bank.exe` runs on any 64-bit Windows without needing MinGW DLLs on the
PATH.

**Linux / macOS (or Windows with make)**
```sh
make run
```

**Manually**
```sh
g++ -std=c++17 -Wall -Wextra -Iinclude src/main.cpp \
    src/models/*.cpp src/core/*.cpp src/services/*.cpp src/ui/*.cpp -o onesky_bank
```

Data is created automatically in `./data/` on first launch.

### Demo credentials

| Role | User ID | PIN |
|------|---------|-----|
| Admin | 1000 | 1234 |
| Demo customer | 1001 | 0000 |

---

## Project layout

```
include/
  models/     Customer, Account, Transaction, Loan        (data structures)
  core/       Database, Auth, Hashing, Utils              (persistence + infra)
  services/   Account, Transaction, Loan, Report services  (business logic)
  ui/         AppController, Menu, Validator              (console interaction)
src/          matching .cpp implementations
data/         runtime storage (created automatically, git-ignored)
```

### Data files (`data/`)
| File | Contents |
|------|----------|
| `customers.dat` | customers & admins (id, name, contact, PIN hash, role) |
| `accounts.dat` | accounts (number, owner, type, balance in paise, rate, status) |
| `transactions.dat` | every money movement with running balance |
| `loans.dat` | loan applications and active loans |
| `seq.dat` | ID / account-number / loan-number sequences |
| `statements/` | exported account statement files |

Each row is a single line of `|`-separated fields — deliberately simple so the
data is human-readable and editable with any text editor.

---

## Notes & limitations

- This is a **demonstration** system. PIN hashing uses FNV-1a (not a
  cryptographically secure KDF) and there is no network layer — do not use it
  with real money or real customer data.
- Interest is accrued manually from the admin menu (a scheduled job would
  replace this in production).
- Amounts use `long long` paise (max ≈ ₹9.2 × 10¹⁶), which is more than enough
  for any realistic bank balance.

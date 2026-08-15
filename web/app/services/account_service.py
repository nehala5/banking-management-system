"""Account management: open/close, interest rates and monthly interest accrual."""
from .. import money
from ..database import get_db, next_id
from . import notification_service

ACCOUNT_TYPES = ("Savings", "Checking")

MIN_OPENING_PAISE = 10000  # ₹100.00


def open_account(user_id, acct_type, opening_paise, remark="New account opened",
                 require_minimum=True):
    """Open a new account. Returns (account_number, None) or (None, error)."""
    if acct_type not in ACCOUNT_TYPES:
        return None, "Unknown account type."
    if require_minimum and opening_paise < MIN_OPENING_PAISE:
        return None, f"Minimum opening deposit is {money.format_paise(MIN_OPENING_PAISE)}."
    if opening_paise < 0:
        return None, "Opening deposit cannot be negative."

    db = get_db()
    number = f"OB{next_id(db, 'account_seq'):06d}"
    now = money.now()
    rate = 3.5 if acct_type == "Savings" else 0.0
    try:
        db.execute(
            "INSERT INTO accounts (account_number, user_id, type, balance_paise,"
            " interest_rate, status, created_at, last_interest_applied)"
            " VALUES (?, ?, ?, ?, ?, 'Active', ?, ?)",
            (number, user_id, acct_type, opening_paise, rate, now, money.today()),
        )
        db.execute(
            "INSERT INTO transactions (account_number, type, amount_paise,"
            " balance_after_paise, counterparty, timestamp, remark)"
            " VALUES (?, 'OPENING', ?, ?, ?, ?, ?)",
            (number, opening_paise, opening_paise, "OneSky Bank", now, remark),
        )
        notification_service.notify(
            user_id, "Account opened",
            f"A {acct_type} account {number} was opened. "
            f"Balance {money.format_paise(opening_paise)}.",
            "success",
        )
        db.commit()
    except Exception:
        db.rollback()
        return None, "Could not open the account."
    return number, None


def accounts_of(user_id):
    db = get_db()
    return db.execute(
        "SELECT * FROM accounts WHERE user_id = ? ORDER BY account_number",
        (user_id,),
    ).fetchall()


def get_account(number):
    return get_db().execute(
        "SELECT * FROM accounts WHERE account_number = ?", (number,)
    ).fetchone()


def set_rate(number, rate_raw):
    """Set the interest rate for a savings account."""
    try:
        rate = float(rate_raw)
    except (TypeError, ValueError):
        return "Interest rate must be a number."
    if rate < 0 or rate > 20:
        return "Interest rate must be between 0% and 20%."
    db = get_db()
    acct = get_account(number)
    if acct is None:
        return "Account not found."
    if acct["type"] != "Savings":
        return "Interest rate only applies to savings accounts."
    db.execute(
        "UPDATE accounts SET interest_rate = ? WHERE account_number = ?",
        (rate, number),
    )
    db.commit()
    return None


def close_account(number):
    """Close an account (empty balance required). Returns error or None."""
    db = get_db()
    acct = get_account(number)
    if acct is None:
        return "Account not found."
    if acct["status"] != "Active":
        return "Account is already closed."
    if acct["balance_paise"] != 0:
        return "Balance must be zero before the account can be closed."
    open_loans = db.execute(
        "SELECT 1 FROM loans WHERE account_number = ? AND status = 'Active'",
        (number,),
    ).fetchone()
    if open_loans:
        return "Clear outstanding loans before closing this account."
    db.execute(
        "UPDATE accounts SET status = 'Closed' WHERE account_number = ?", (number,)
    )
    db.execute(
        "INSERT INTO transactions (account_number, type, amount_paise,"
        " balance_after_paise, counterparty, timestamp, remark)"
        " VALUES (?, 'CLOSED', 0, 0, 'OneSky Bank', ?, 'Account closed')",
        (number, money.now()),
    )
    notification_service.notify(
        acct["user_id"], "Account closed",
        f"Account {number} was closed.",
        "info",
    )
    db.commit()
    return None


def apply_monthly_interest():
    """Apply one month of interest to every active savings account.

    Returns (count, total_paise) of interest credited.
    """
    db = get_db()
    rows = db.execute(
        "SELECT * FROM accounts WHERE status = 'Active' AND type = 'Savings'"
        " AND interest_rate > 0"
    ).fetchall()
    now = money.now()
    total = 0
    count = 0
    for acct in rows:
        interest = round(acct["balance_paise"] * acct["interest_rate"] / 100 / 12)
        if interest <= 0:
            continue
        new_balance = acct["balance_paise"] + interest
        db.execute(
            "UPDATE accounts SET balance_paise = ?, last_interest_applied = ?"
            " WHERE account_number = ?",
            (new_balance, money.today(), acct["account_number"]),
        )
        db.execute(
            "INSERT INTO transactions (account_number, type, amount_paise,"
            " balance_after_paise, counterparty, timestamp, remark)"
            " VALUES (?, 'INTEREST', ?, ?, 'OneSky Bank', ?, 'Monthly interest credit')",
            (acct["account_number"], interest, new_balance, now),
        )
        notification_service.notify(
            acct["user_id"], "Interest credited",
            f"{money.format_paise(interest)} interest was credited to {acct['account_number']} "
            f"at {acct['interest_rate']:g}% p.a. New balance {money.format_paise(new_balance)}.",
            "success",
        )
        total += interest
        count += 1
    db.commit()
    return count, total

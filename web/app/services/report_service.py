"""Reports: admin overview, customer summaries and downloadable statements."""
from .. import money
from ..database import get_db
from . import transaction_service


def bank_overview():
    """High-level figures for the admin dashboard."""
    db = get_db()
    customers = db.execute(
        "SELECT COUNT(*) AS c FROM users WHERE role = 'CUSTOMER'"
    ).fetchone()["c"]
    accounts = db.execute(
        "SELECT COUNT(*) AS c FROM accounts WHERE status = 'Active'"
    ).fetchone()["c"]
    deposits = db.execute(
        "SELECT COALESCE(SUM(balance_paise), 0) AS t FROM accounts"
        " WHERE status = 'Active'"
    ).fetchone()["t"]
    loan_book = db.execute(
        "SELECT COALESCE(SUM(outstanding_paise), 0) AS t FROM loans"
        " WHERE status = 'Active'"
    ).fetchone()["t"]
    pending_loans = db.execute(
        "SELECT COUNT(*) AS c FROM loans WHERE status = 'Pending'"
    ).fetchone()["c"]
    active_loans = db.execute(
        "SELECT COUNT(*) AS c FROM loans WHERE status = 'Active'"
    ).fetchone()["c"]
    avg_balance = round(deposits / accounts) if accounts else 0

    return {
        "customers": customers,
        "active_accounts": accounts,
        "total_deposits_paise": deposits,
        "loan_book_paise": loan_book,
        "pending_loans": pending_loans,
        "active_loans": active_loans,
        "avg_balance_paise": avg_balance,
        "ldr": (loan_book / deposits * 100.0) if deposits else 0.0,
    }


def top_accounts(limit=5):
    db = get_db()
    return db.execute(
        "SELECT a.*, u.name AS user_name FROM accounts a"
        " JOIN users u ON u.id = a.user_id"
        " WHERE a.status = 'Active'"
        " ORDER BY a.balance_paise DESC LIMIT ?",
        (limit,),
    ).fetchall()


def customer_summary(user_id):
    """Totals for one customer's portal sidebar."""
    db = get_db()
    accounts = db.execute(
        "SELECT * FROM accounts WHERE user_id = ?", (user_id,)
    ).fetchall()
    total = sum(a["balance_paise"] for a in accounts)
    active = sum(1 for a in accounts if a["status"] == "Active")
    loans = db.execute(
        "SELECT COALESCE(SUM(outstanding_paise), 0) AS t, COUNT(*) AS c"
        " FROM loans WHERE user_id = ? AND status = 'Active'",
        (user_id,),
    ).fetchone()
    return {
        "account_count": len(accounts),
        "active_accounts": active,
        "total_balance_paise": total,
        "loan_outstanding_paise": loans["t"],
        "loan_count": loans["c"],
    }


def statement(account_number, limit=100):
    """Transaction history with running balance, oldest-first."""
    db = get_db()
    acct = db.execute(
        "SELECT * FROM accounts WHERE account_number = ?", (account_number,)
    ).fetchone()
    if acct is None:
        return None, []
    rows = db.execute(
        "SELECT * FROM transactions WHERE account_number = ?"
        " ORDER BY id DESC LIMIT ?",
        (account_number, limit),
    ).fetchall()
    rows = list(reversed(rows))
    return acct, rows


def statement_text(account_number, limit=100):
    """Plain-text statement, matching the C++ download format."""
    acct, rows = statement(account_number, limit)
    if acct is None:
        return None
    db = get_db()
    holder = db.execute(
        "SELECT name FROM users WHERE id = ?", (acct["user_id"],)
    ).fetchone()
    holder = holder["name"] if holder else "Unknown"

    lines = []
    lines.append("=" * 62)
    lines.append("        ONESKY BANK — ACCOUNT STATEMENT")
    lines.append("=" * 62)
    lines.append(f"Account       : {acct['account_number']}")
    lines.append(f"Holder        : {holder}")
    lines.append(f"Type          : {acct['type']}")
    lines.append(f"Status        : {acct['status']}")
    lines.append(f"Rate          : {acct['interest_rate']:.1f}% p.a.")
    lines.append(f"Current bal   : {money.format_paise(acct['balance_paise'])}")
    lines.append(f"Generated on  : {money.now()}")
    lines.append("-" * 62)
    lines.append(f"{'Date':<19}{'Type':<13}{'Amount':>14}  {'Remark'}")
    lines.append("-" * 62)
    for t in rows:
        label = transaction_service.TYPE_MAP.get(t["type"], t["type"])
        sign = "+" if t["amount_paise"] >= 0 else "-"
        amt = f"{sign}{money.format_paise_plain(t['amount_paise'])}"
        remark = (t["remark"] or t["counterparty"] or "")[:24]
        lines.append(
            f"{t['timestamp']:<19}{label:<13}{amt:>14}  {remark}"
        )
    lines.append("=" * 62)
    lines.append("  Thank you for banking with OneSky Bank.")
    return "\n".join(lines)

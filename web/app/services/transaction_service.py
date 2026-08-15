"""Money movement: deposits, withdrawals, transfers and transaction history."""
from .. import money
from ..database import get_db
from .account_service import get_account

TYPE_MAP = {
    "DEPOSIT": "Deposit",
    "WITHDRAW": "Withdrawal",
    "TRANSFER_IN": "Transfer in",
    "TRANSFER_OUT": "Transfer out",
    "OPENING": "Opening",
    "INTEREST": "Interest",
    "CLOSED": "Closed",
}


def _record(db, number, tx_type, amount_paise, balance_after_paise,
            counterparty, remark):
    db.execute(
        "INSERT INTO transactions (account_number, type, amount_paise,"
        " balance_after_paise, counterparty, timestamp, remark)"
        " VALUES (?, ?, ?, ?, ?, ?, ?)",
        (number, tx_type, amount_paise, balance_after_paise,
         counterparty, money.now(), remark),
    )


def deposit(number, paise, counterparty="Cash deposit", remark="Deposit"):
    """Deposit into an account. Returns error or None."""
    if paise <= 0:
        return "Amount must be positive."
    db = get_db()
    acct = get_account(number)
    if acct is None:
        return "Account not found."
    if acct["status"] != "Active":
        return "Account is closed — deposits are not allowed."
    new_balance = acct["balance_paise"] + paise
    db.execute(
        "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
        (new_balance, number),
    )
    _record(db, number, "DEPOSIT", paise, new_balance, counterparty, remark)
    db.commit()
    return None


def withdraw(number, paise, counterparty="Cash withdrawal", remark="Withdrawal"):
    """Withdraw from an account (overdrafts not allowed). Returns error or None."""
    if paise <= 0:
        return "Amount must be positive."
    db = get_db()
    acct = get_account(number)
    if acct is None:
        return "Account not found."
    if acct["status"] != "Active":
        return "Account is closed — withdrawals are not allowed."
    if paise > acct["balance_paise"]:
        return f"Insufficient balance. Available: {money.format_paise(acct['balance_paise'])}"
    new_balance = acct["balance_paise"] - paise
    db.execute(
        "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
        (new_balance, number),
    )
    _record(db, number, "WITHDRAW", paise, new_balance, counterparty, remark)
    db.commit()
    return None


def transfer(from_number, to_number, paise, remark="Transfer"):
    """Move money between two accounts. Returns error or None."""
    if paise <= 0:
        return "Amount must be positive."
    if from_number == to_number:
        return "Source and destination accounts must be different."
    db = get_db()
    src = get_account(from_number)
    dst = get_account(to_number)
    if src is None or dst is None:
        return "One of the accounts was not found."
    if src["status"] != "Active":
        return "Source account is closed."
    if dst["status"] != "Active":
        return "Destination account is closed."
    if paise > src["balance_paise"]:
        return f"Insufficient balance. Available: {money.format_paise(src['balance_paise'])}"

    src_name = db.execute("SELECT name FROM users WHERE id = ?", (src["user_id"],)).fetchone()
    dst_name = db.execute("SELECT name FROM users WHERE id = ?", (dst["user_id"],)).fetchone()
    src_label = src_name["name"] if src_name else ""
    dst_label = dst_name["name"] if dst_name else ""

    new_src = src["balance_paise"] - paise
    new_dst = dst["balance_paise"] + paise
    db.execute(
        "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
        (new_src, from_number),
    )
    db.execute(
        "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
        (new_dst, to_number),
    )
    _record(db, from_number, "TRANSFER_OUT", paise, new_src, to_number, f"{remark} to {dst_label}")
    _record(db, to_number, "TRANSFER_IN", paise, new_dst, from_number, f"{remark} from {src_label}")
    db.commit()
    return None


def history(number, limit=100):
    db = get_db()
    return db.execute(
        "SELECT * FROM transactions WHERE account_number = ?"
        " ORDER BY id DESC LIMIT ?",
        (number, limit),
    ).fetchall()


def all_history(limit=200):
    db = get_db()
    return db.execute(
        "SELECT t.*, a.user_id FROM transactions t"
        " JOIN accounts a ON a.account_number = t.account_number"
        " ORDER BY t.id DESC LIMIT ?",
        (limit,),
    ).fetchall()


def today_activity():
    """Aggregate transaction counts and volume for today (admin dashboard)."""
    db = get_db()
    row = db.execute(
        "SELECT COUNT(*) AS count, COALESCE(SUM(amount_paise), 0) AS total"
        " FROM transactions WHERE timestamp LIKE ?",
        (money.today() + "%",),
    ).fetchone()
    return {"count": row["count"], "total_paise": row["total"]}


def last_seven_days():
    """Per-day transaction volume for the last 7 days (charts)."""
    db = get_db()
    from datetime import datetime, timedelta

    today_dt = datetime.now()
    days = []
    for i in range(6, -1, -1):
        d = today_dt - timedelta(days=i)
        prefix = d.strftime("%Y-%m-%d") + "%"
        row = db.execute(
            "SELECT COUNT(*) AS count, COALESCE(SUM(amount_paise), 0) AS total"
            " FROM transactions WHERE timestamp LIKE ?",
            (prefix,),
        ).fetchone()
        days.append({
            "date": d.strftime("%d %b"),
            "count": row["count"],
            "total_paise": row["total"],
        })
    return days

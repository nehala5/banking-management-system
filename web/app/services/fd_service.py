"""Fixed deposits (FDs): issue, projected maturity, auto-maturity and early close."""
from .. import money
from ..database import get_db
from .account_service import get_account
from . import notification_service

MIN_FD_PAISE = 100000  # ₹1,000.00
EARLY_CLOSE_PENALTY_PCT = 0.5  # % of principal forfeited on early closure


def fd_rate_for(months: int) -> float:
    """OneSky FD rate tiers by tenure."""
    if months <= 0:
        return 0.0
    if months < 6:
        return 5.5
    if months < 12:
        return 6.5
    if months < 24:
        return 7.0
    return 7.5


def maturity_paise(principal_paise, rate, months) -> int:
    return money.fd_maturity(principal_paise, rate, months)


def open_fd(user_id, account_number, principal_paise, months):
    """Move money from a savings account into a fixed deposit.

    Returns (deposit_id, None) or (None, error).
    """
    if principal_paise < MIN_FD_PAISE:
        return None, f"Minimum FD amount is {money.format_paise(MIN_FD_PAISE)}."
    if not (3 <= months <= 60):
        return None, "FD tenure must be between 3 and 60 months."

    db = get_db()
    acct = get_account(account_number)
    if acct is None or acct["user_id"] != user_id:
        return None, "Account not found or not yours."
    if acct["status"] != "Active":
        return None, "Account is closed — cannot open a fixed deposit."
    if principal_paise > acct["balance_paise"]:
        return None, (f"Insufficient balance. Available: "
                      f"{money.format_paise(acct['balance_paise'])}")

    rate = fd_rate_for(months)
    today = money.today()
    maturity = maturity_paise(principal_paise, rate, months)

    new_balance = acct["balance_paise"] - principal_paise
    db.execute(
        "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
        (new_balance, account_number),
    )
    db.execute(
        "INSERT INTO deposits (user_id, account_number, principal_paise, rate,"
        " tenure_months, maturity_paise, status, start_date, maturity_date)"
        " VALUES (?, ?, ?, ?, ?, ?, 'Active', ?, ?)",
        (user_id, account_number, principal_paise, rate, months, maturity,
         today, money.add_months(today, months)),
    )
    deposit_id = db.execute("SELECT last_insert_rowid() AS id").fetchone()["id"]
    db.execute(
        "INSERT INTO transactions (account_number, type, amount_paise,"
        " balance_after_paise, counterparty, timestamp, remark)"
        " VALUES (?, 'FD_ISSUED', ?, ?, 'OneSky Bank', ?, 'Fixed deposit opened')",
        (account_number, principal_paise, new_balance, money.now()),
    )
    notification_service.notify(
        user_id, "Fixed deposit opened",
        f"FD #{deposit_id} of {money.format_paise(principal_paise)} @ {rate:g}% for "
        f"{months} months was opened against {account_number}. Matures on "
        f"{money.add_months(today, months)} for {money.format_paise(maturity)}.",
        "money",
    )
    db.commit()
    return deposit_id, None


def fds_of(user_id):
    return get_db().execute(
        "SELECT * FROM deposits WHERE user_id = ? ORDER BY id DESC", (user_id,)
    ).fetchall()


def all_fds():
    db = get_db()
    return db.execute(
        "SELECT d.*, u.name AS user_name FROM deposits d"
        " JOIN users u ON u.id = d.user_id ORDER BY d.id DESC"
    ).fetchall()


def get_fd(deposit_id):
    return get_db().execute(
        "SELECT * FROM deposits WHERE id = ?", (deposit_id,)
    ).fetchone()


def process_maturities():
    """Credit matured FDs back to their linked accounts.

    Returns (count, total_paise) of principal+interest credited.
    """
    db = get_db()
    due = db.execute(
        "SELECT * FROM deposits WHERE status = 'Active' AND maturity_date <= ?",
        (money.today(),),
    ).fetchall()
    now = money.now()
    count = 0
    total = 0
    for fd in due:
        acct = get_account(fd["account_number"])
        if acct is None or acct["status"] != "Active":
            continue
        new_balance = acct["balance_paise"] + fd["maturity_paise"]
        db.execute(
            "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
            (new_balance, fd["account_number"]),
        )
        db.execute(
            "UPDATE deposits SET status = 'Matured', closed_date = ?,"
            " closed_amount_paise = ? WHERE id = ?",
            (now, fd["maturity_paise"], fd["id"]),
        )
        db.execute(
            "INSERT INTO transactions (account_number, type, amount_paise,"
            " balance_after_paise, counterparty, timestamp, remark)"
            " VALUES (?, 'FD_MATURED', ?, ?, 'OneSky Bank', ?, 'FD matured - principal + interest')",
            (fd["account_number"], fd["maturity_paise"], new_balance, now),
        )
        notification_service.notify(
            fd["user_id"], "Fixed deposit matured",
            f"FD #{fd['id']} matured and {money.format_paise(fd['maturity_paise'])} "
            f"was credited to {fd['account_number']}.",
            "success",
        )
        count += 1
        total += fd["maturity_paise"]
    db.commit()
    return count, total


def early_close(deposit_id, user_id):
    """Premature closure: refund principal minus the penalty, forfeit interest."""
    db = get_db()
    fd = get_fd(deposit_id)
    if fd is None or fd["user_id"] != user_id:
        return "Fixed deposit not found."
    if fd["status"] != "Active":
        return "This fixed deposit is not active."
    acct = get_account(fd["account_number"])
    if acct is None or acct["status"] != "Active":
        return "Linked account is closed."

    penalty = round(fd["principal_paise"] * EARLY_CLOSE_PENALTY_PCT / 100)
    refund = fd["principal_paise"] - penalty
    new_balance = acct["balance_paise"] + refund
    now = money.now()

    db.execute(
        "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
        (new_balance, fd["account_number"]),
    )
    db.execute(
        "UPDATE deposits SET status = 'Closed', closed_date = ?,"
        " closed_amount_paise = ?, penalty_paise = ? WHERE id = ?",
        (now, refund, penalty, deposit_id),
    )
    db.execute(
        "INSERT INTO transactions (account_number, type, amount_paise,"
        " balance_after_paise, counterparty, timestamp, remark)"
        " VALUES (?, 'FD_CLOSED', ?, ?, 'OneSky Bank', ?,"
        " 'FD closed early - penalty deducted')",
        (fd["account_number"], refund, new_balance, now),
    )
    notification_service.notify(
        user_id, "Fixed deposit closed early",
        f"FD #{deposit_id} was closed early. {money.format_paise(refund)} credited "
        f"to {fd['account_number']} (penalty {money.format_paise(penalty)}).",
        "warning",
    )
    db.commit()
    return None

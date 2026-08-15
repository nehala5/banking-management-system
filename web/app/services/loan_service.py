"""Loan application, approval, EMI computation and repayment."""
import math

from .. import money
from ..database import get_db, next_id
from .account_service import get_account
from . import transaction_service


def compute_emi(principal_paise, rate, months) -> int:
    """Standard amortising EMI in paise."""
    principal = principal_paise / 100.0
    r = rate / 100.0 / 12.0
    if r == 0:
        return round(principal / months)
    emi = principal * r * (1 + r) ** months / ((1 + r) ** months - 1)
    return round(emi * 100)


def apply_loan(user_id, account_number, principal_paise, rate, months):
    """Submit a loan application. Returns (loan_id, None) or (None, error)."""
    if principal_paise <= 0:
        return None, "Loan amount must be positive."
    if not (1 <= rate <= 36):
        return None, "Interest rate must be between 1% and 36%."
    if not (3 <= months <= 120):
        return None, "Tenure must be between 3 and 120 months."

    db = get_db()
    acct = get_account(account_number)
    if acct is None or acct["user_id"] != user_id:
        return None, "Account not found or not yours."
    if acct["status"] != "Active":
        return None, "Account is closed — cannot apply for a loan."
    if acct["type"] == "Checking":
        return None, "Loans can only be linked to savings accounts."

    emi = compute_emi(principal_paise, rate, months)
    loan_id = f"LN{next_id(db, 'loan_seq'):04d}"
    db.execute(
        "INSERT INTO loans (loan_id, user_id, account_number, principal_paise,"
        " outstanding_paise, rate, tenure_months, emi_paise, status, applied_date)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?, 'Pending', ?)",
        (loan_id, user_id, account_number, principal_paise, principal_paise,
         rate, months, emi, money.now()),
    )
    db.commit()
    return loan_id, None


def loans_of(user_id):
    db = get_db()
    return db.execute(
        "SELECT l.*, a.type AS account_type FROM loans l"
        " JOIN accounts a ON a.account_number = l.account_number"
        " WHERE l.user_id = ? ORDER BY l.applied_date DESC",
        (user_id,),
    ).fetchall()


def pending_loans():
    db = get_db()
    return db.execute(
        "SELECT l.*, u.name AS user_name, a.type AS account_type FROM loans l"
        " JOIN users u ON u.id = l.user_id"
        " JOIN accounts a ON a.account_number = l.account_number"
        " WHERE l.status = 'Pending' ORDER BY l.applied_date",
    ).fetchall()


def active_loans():
    db = get_db()
    return db.execute(
        "SELECT l.*, u.name AS user_name, a.type AS account_type FROM loans l"
        " JOIN users u ON u.id = l.user_id"
        " JOIN accounts a ON a.account_number = l.account_number"
        " WHERE l.status = 'Active' ORDER BY l.applied_date",
    ).fetchall()


def get_loan(loan_id):
    return get_db().execute("SELECT * FROM loans WHERE loan_id = ?", (loan_id,)).fetchone()


def approve_loan(loan_id):
    """Approve a pending loan and disburse to the linked account."""
    db = get_db()
    loan = get_loan(loan_id)
    if loan is None:
        return "Loan not found."
    if loan["status"] != "Pending":
        return "Loan is not pending approval."
    acct = get_account(loan["account_number"])
    if acct is None or acct["status"] != "Active":
        return "Linked account is closed."

    due_date = money.add_months(money.today(), 1)
    db.execute(
        "UPDATE loans SET status = 'Active', decision_date = ?, next_due_date = ?"
        " WHERE loan_id = ?",
        (money.now(), due_date, loan_id),
    )
    new_balance = acct["balance_paise"] + loan["principal_paise"]
    db.execute(
        "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
        (new_balance, loan["account_number"]),
    )
    transaction_service._record(
        db, loan["account_number"], "DEPOSIT", loan["principal_paise"], new_balance,
        "OneSky Bank", f"Loan disbursement {loan_id}",
    )
    db.commit()
    return None


def reject_loan(loan_id):
    db = get_db()
    loan = get_loan(loan_id)
    if loan is None:
        return "Loan not found."
    if loan["status"] != "Pending":
        return "Loan is not pending approval."
    db.execute(
        "UPDATE loans SET status = 'Rejected', decision_date = ? WHERE loan_id = ?",
        (money.now(), loan_id),
    )
    db.commit()
    return None


def pay_emi(loan_id):
    """Pay one EMI from the linked account. Returns error or None."""
    db = get_db()
    loan = get_loan(loan_id)
    if loan is None:
        return "Loan not found."
    if loan["status"] != "Active":
        return "Loan is not active."
    acct = get_account(loan["account_number"])
    if acct is None or acct["status"] != "Active":
        return "Linked account is closed."
    if loan["outstanding_paise"] <= 0:
        return "Loan is already fully repaid."

    due = min(loan["emi_paise"], loan["outstanding_paise"])
    if due > acct["balance_paise"]:
        return f"Insufficient balance to pay EMI. Available: {money.format_paise(acct['balance_paise'])}"

    new_balance = acct["balance_paise"] - due
    outstanding = loan["outstanding_paise"] - due
    amount_paid = loan["amount_paid_paise"] + due
    emi_count = loan["emi_paid_count"] + 1

    db.execute(
        "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
        (new_balance, loan["account_number"]),
    )
    next_due = loan["next_due_date"]
    if outstanding <= 0:
        db.execute(
            "UPDATE loans SET outstanding_paise = 0, amount_paid_paise = ?,"
            " emi_paid_count = ?, status = 'Closed', next_due_date = '' WHERE loan_id = ?",
            (amount_paid, emi_count, loan_id),
        )
    else:
        next_due = money.add_months(next_due, 1)
        db.execute(
            "UPDATE loans SET outstanding_paise = ?, amount_paid_paise = ?,"
            " emi_paid_count = ?, next_due_date = ? WHERE loan_id = ?",
            (outstanding, amount_paid, emi_count, next_due, loan_id),
        )
    transaction_service._record(
        db, loan["account_number"], "WITHDRAW", due, new_balance,
        "OneSky Bank", f"EMI payment for {loan_id}",
    )
    db.commit()
    return None

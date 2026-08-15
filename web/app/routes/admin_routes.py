"""Admin panel routes."""
from flask import (
    Blueprint, flash, redirect, render_template, request, session, url_for,
)

from .. import money
from ..database import get_db
from ..services import (
    account_service, loan_service, report_service, transaction_service,
)
from .decorators import admin_required

admin_bp = Blueprint("admin", __name__, url_prefix="/admin")


@admin_bp.get("/")
@admin_required
def dashboard():
    overview = report_service.bank_overview()
    activity = transaction_service.today_activity()
    seven_days = transaction_service.last_seven_days()
    pending = loan_service.pending_loans()
    recent = transaction_service.all_history(8)
    return render_template(
        "admin/dashboard.html",
        overview=overview,
        activity=activity,
        seven_days=seven_days,
        pending=pending[:4],
        recent=recent,
    )


@admin_bp.get("/customers")
@admin_required
def customers():
    q = request.args.get("q", "").strip()
    db = get_db()
    if q:
        rows = db.execute(
            "SELECT u.*,"
            " (SELECT COALESCE(SUM(balance_paise), 0) FROM accounts a"
            "  WHERE a.user_id = u.id) AS total_balance"
            " FROM users u WHERE u.role = 'CUSTOMER'"
            " AND (u.name LIKE ? OR u.email LIKE ? OR u.phone LIKE ?)"
            " ORDER BY u.id",
            (f"%{q}%", f"%{q}%", f"%{q}%"),
        ).fetchall()
    else:
        rows = db.execute(
            "SELECT u.*,"
            " (SELECT COALESCE(SUM(balance_paise), 0) FROM accounts a"
            "  WHERE a.user_id = u.id) AS total_balance"
            " FROM users u WHERE u.role = 'CUSTOMER' ORDER BY u.id"
        ).fetchall()
    return render_template("admin/customers.html", customers=rows, q=q)


@admin_bp.get("/customers/<int:user_id>")
@admin_required
def customer_detail(user_id):
    db = get_db()
    user = db.execute("SELECT * FROM users WHERE id = ?", (user_id,)).fetchone()
    if user is None:
        return render_template("errors/404.html"), 404
    accounts = account_service.accounts_of(user_id)
    loans = loan_service.loans_of(user_id)
    summary = report_service.customer_summary(user_id)
    return render_template(
        "admin/customer_detail.html",
        user=user,
        accounts=accounts,
        loans=loans,
        summary=summary,
    )


@admin_bp.get("/accounts")
@admin_required
def accounts():
    db = get_db()
    rows = db.execute(
        "SELECT a.*, u.name AS user_name FROM accounts a"
        " JOIN users u ON u.id = a.user_id ORDER BY a.account_number"
    ).fetchall()
    customers = db.execute(
        "SELECT id, name FROM users WHERE role = 'CUSTOMER' ORDER BY id"
    ).fetchall()
    return render_template("admin/accounts.html", accounts=rows, customers=customers)


@admin_bp.post("/accounts/open")
@admin_required
def open_account():
    user_id_raw = request.form.get("user_id", "")
    acct_type = request.form.get("type", "Savings")
    opening, error = money.parse_amount(request.form.get("opening", "0"))
    remark = request.form.get("remark", "") or "New account opened"
    if not user_id_raw.isdigit():
        error = error or "Please choose a customer."
    elif error is None:
        number, error = account_service.open_account(
            int(user_id_raw), acct_type, opening, remark
        )
        if error is None:
            flash(f"Account {number} opened for customer {user_id_raw}.", "success")
            return redirect(url_for("admin.accounts"))
    flash(error, "error")
    return redirect(url_for("admin.accounts"))


@admin_bp.post("/accounts/<number>/close")
@admin_required
def close_account(number):
    error = account_service.close_account(number)
    if error:
        flash(error, "error")
    else:
        flash(f"Account {number} closed.", "success")
    return redirect(url_for("admin.accounts"))


@admin_bp.post("/accounts/<number>/rate")
@admin_required
def set_rate(number):
    rate = request.form.get("rate", "")
    error = account_service.set_rate(number, rate)
    if error:
        flash(error, "error")
    else:
        flash(f"Interest rate for {number} updated to {rate}% p.a.", "success")
    return redirect(url_for("admin.accounts"))


@admin_bp.get("/loans")
@admin_required
def loans():
    pending = loan_service.pending_loans()
    active = loan_service.active_loans()
    db = get_db()
    history = db.execute(
        "SELECT l.*, u.name AS user_name FROM loans l"
        " JOIN users u ON u.id = l.user_id"
        " WHERE l.status IN ('Rejected', 'Closed') ORDER BY l.decision_date DESC"
    ).fetchall()
    return render_template(
        "admin/loans.html", pending=pending, active=active, history=history
    )


@admin_bp.post("/loans/<loan_id>/approve")
@admin_required
def approve_loan(loan_id):
    error = loan_service.approve_loan(loan_id)
    if error:
        flash(error, "error")
    else:
        flash(f"Loan {loan_id} approved and disbursed.", "success")
    return redirect(url_for("admin.loans"))


@admin_bp.post("/loans/<loan_id>/reject")
@admin_required
def reject_loan(loan_id):
    error = loan_service.reject_loan(loan_id)
    if error:
        flash(error, "error")
    else:
        flash(f"Loan {loan_id} rejected.", "info")
    return redirect(url_for("admin.loans"))


@admin_bp.post("/interest")
@admin_required
def apply_interest():
    count, total = account_service.apply_monthly_interest()
    if count:
        flash(
            f"Monthly interest applied to {count} savings account(s): "
            f"{money.format_paise(total)} credited.",
            "success",
        )
    else:
        flash("No savings accounts earned interest this cycle.", "info")
    return redirect(url_for("admin.dashboard"))


@admin_bp.get("/reports")
@admin_required
def reports():
    overview = report_service.bank_overview()
    top = report_service.top_accounts(8)
    seven_days = transaction_service.last_seven_days()
    db = get_db()
    by_type = db.execute(
        "SELECT type, COUNT(*) AS c, COALESCE(SUM(amount_paise), 0) AS total"
        " FROM transactions GROUP BY type ORDER BY total DESC"
    ).fetchall()
    return render_template(
        "admin/reports.html",
        overview=overview,
        top=top,
        seven_days=seven_days,
        by_type=by_type,
    )

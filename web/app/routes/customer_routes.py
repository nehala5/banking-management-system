"""Customer portal routes."""
from flask import (
    Blueprint, abort, flash, redirect, render_template, request, session, url_for,
)

from .. import money
from ..services import (
    account_service, loan_service, report_service, transaction_service,
)
from .decorators import login_required

customer_bp = Blueprint("customer", __name__, url_prefix="/me")


def _require_account_owner(number):
    """Abort unless the logged-in customer owns this account."""
    acct = account_service.get_account(number)
    if acct is None or acct["user_id"] != session.get("user_id"):
        abort(404)
    return acct


@customer_bp.get("/dashboard")
@login_required
def dashboard():
    user_id = session["user_id"]
    accounts = account_service.accounts_of(user_id)
    summary = report_service.customer_summary(user_id)
    recent = []
    for acct in accounts:
        rows = transaction_service.history(acct["account_number"], 3)
        recent.extend(rows)
    recent.sort(key=lambda r: r["id"], reverse=True)
    return render_template(
        "customer/dashboard.html",
        accounts=accounts,
        summary=summary,
        recent=recent[:6],
        greeting=money.greeting(),
    )


@customer_bp.get("/accounts")
@login_required
def accounts():
    user_id = session["user_id"]
    accs = account_service.accounts_of(user_id)
    rows = [report_service.statement(a["account_number"], 5) for a in accs]
    data = [{"acct": a, "txs": txs} for a, txs in rows]
    return render_template("customer/accounts.html", data=data)


@customer_bp.route("/money", methods=["GET", "POST"])
@login_required
def money_page():
    """Single page with deposit / withdraw / transfer tabs."""
    user_id = session["user_id"]
    accounts = account_service.accounts_of(user_id)
    active_tab = request.args.get("tab", "deposit")
    if active_tab not in ("deposit", "withdraw", "transfer"):
        active_tab = "deposit"

    if request.method == "POST":
        action = request.form.get("action")
        amount, error = money.parse_amount(request.form.get("amount"))
        number = request.form.get("account", "")
        acct = account_service.get_account(number)
        if error:
            flash(error, "error")
        elif acct is None or acct["user_id"] != user_id:
            flash("Please pick one of your accounts.", "error")
        else:
            if action == "deposit":
                error = transaction_service.deposit(
                    number, amount,
                    counterparty=request.form.get("counterparty", "") or "Cash deposit",
                    remark=request.form.get("remark", "") or "Deposit",
                )
            elif action == "withdraw":
                error = transaction_service.withdraw(
                    number, amount,
                    counterparty=request.form.get("counterparty", "") or "Cash withdrawal",
                    remark=request.form.get("remark", "") or "Withdrawal",
                )
            elif action == "transfer":
                to_number = request.form.get("to_account", "")
                error = transaction_service.transfer(
                    number, to_number, amount,
                    remark=request.form.get("remark", "") or "Transfer",
                )
            else:
                error = "Unknown action."
            if error:
                flash(error, "error")
            else:
                verb = {"deposit": "Deposited", "withdraw": "Withdrew", "transfer": "Transferred"}[action]
                flash(f"{verb} {money.format_paise(amount)} successfully.", "success")
            return redirect(url_for("customer.money_page", tab=active_tab))

    return render_template(
        "customer/money.html",
        accounts=accounts,
        active_tab=active_tab,
    )


@customer_bp.get("/history/<number>")
@login_required
def history(number):
    _require_account_owner(number)
    rows = transaction_service.history(number, 200)
    return render_template("customer/history.html", number=number, rows=rows)


@customer_bp.route("/loans", methods=["GET", "POST"])
@login_required
def loans():
    user_id = session["user_id"]
    accounts = account_service.accounts_of(user_id)

    if request.method == "POST":
        principal, err = money.parse_amount(request.form.get("amount"))
        try:
            rate = float(request.form.get("rate", ""))
            months = int(request.form.get("months", ""))
        except ValueError:
            err = err or "Please enter a valid rate and tenure."
        number = request.form.get("account", "")
        acct = account_service.get_account(number)
        if err:
            flash(err, "error")
        elif acct is None or acct["user_id"] != user_id:
            flash("Please pick one of your savings accounts.", "error")
        else:
            loan_id, error = loan_service.apply_loan(
                user_id, number, principal, rate, months
            )
            if error:
                flash(error, "error")
            else:
                flash(
                    f"Loan application {loan_id} submitted for "
                    f"{money.format_paise(principal)}. Waiting for admin approval.",
                    "success",
                )
            return redirect(url_for("customer.loans"))

    my_loans = loan_service.loans_of(user_id)
    savings = [a for a in accounts if a["type"] == "Savings"]
    return render_template("customer/loans.html", loans=my_loans, savings=savings)


@customer_bp.post("/loans/<loan_id>/pay")
@login_required
def pay_emi(loan_id):
    loan = loan_service.get_loan(loan_id)
    if loan is None or loan["user_id"] != session["user_id"]:
        abort(404)
    error = loan_service.pay_emi(loan_id)
    if error:
        flash(error, "error")
    else:
        flash(f"EMI for {loan_id} paid. {money.format_paise(loan['emi_paise'])} settled.", "success")
    return redirect(url_for("customer.loans"))


@customer_bp.get("/profile")
@login_required
def profile():
    user_id = session["user_id"]
    summary = report_service.customer_summary(user_id)
    loans = loan_service.loans_of(user_id)
    return render_template("customer/profile.html", summary=summary, loans=loans)


@customer_bp.get("/statement/<number>")
@login_required
def download_statement(number):
    _require_account_owner(number)
    text = report_service.statement_text(number, 200)
    if text is None:
        abort(404)
    filename = f"{number}_statement.txt"
    return text, 200, {
        "Content-Type": "text/plain; charset=utf-8",
        "Content-Disposition": f'attachment; filename="{filename}"',
    }

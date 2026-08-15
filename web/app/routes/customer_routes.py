"""Customer portal routes."""
from flask import (
    Blueprint, abort, flash, redirect, render_template, request, session, url_for,
)

from .. import money
from ..services import (
    account_service, card_service, fd_service, loan_service,
    notification_service, report_service, transaction_service,
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


# ---------- Fixed deposits ----------

@customer_bp.route("/deposits", methods=["GET", "POST"])
@login_required
def deposits():
    user_id = session["user_id"]
    accounts = account_service.accounts_of(user_id)

    if request.method == "POST":
        principal, err = money.parse_amount(request.form.get("amount"))
        try:
            months = int(request.form.get("months", ""))
        except ValueError:
            err = err or "Please enter a tenure in months."
        number = request.form.get("account", "")
        acct = account_service.get_account(number)
        if err:
            flash(err, "error")
        elif acct is None or acct["user_id"] != user_id:
            flash("Please pick one of your accounts.", "error")
        else:
            deposit_id, error = fd_service.open_fd(user_id, number, principal, months)
            if error:
                flash(error, "error")
            else:
                flash(f"Fixed deposit #{deposit_id} opened. Money is locked until maturity!", "success")
            return redirect(url_for("customer.deposits"))

    my_fds = fd_service.fds_of(user_id)
    savings = [a for a in accounts if a["status"] == "Active"]
    return render_template(
        "customer/deposits.html", fds=my_fds, savings=savings,
        fd_rate_for=fd_service.fd_rate_for,
    )


@customer_bp.post("/deposits/<int:deposit_id>/close")
@login_required
def close_fd(deposit_id):
    error = fd_service.early_close(deposit_id, session["user_id"])
    if error:
        flash(error, "error")
    else:
        flash("Fixed deposit closed early. Principal (minus penalty) was returned.", "info")
    return redirect(url_for("customer.deposits"))


# ---------- Debit cards ----------

@customer_bp.route("/cards", methods=["GET", "POST"])
@login_required
def cards():
    user_id = session["user_id"]
    accounts = account_service.accounts_of(user_id)

    if request.method == "POST":
        action = request.form.get("action")
        if action == "issue":
            number = request.form.get("account", "")
            pin = request.form.get("pin", "")
            acct = account_service.get_account(number)
            if acct is None or acct["user_id"] != user_id:
                flash("Please pick one of your accounts.", "error")
            else:
                card, error = card_service.issue_card(user_id, number, pin)
                if error:
                    flash(error, "error")
                else:
                    flash(
                        f"Card •••• {card['card_number'][-4:]} issued on {number}. "
                        f"Expires {card['expiry']}.",
                        "success",
                    )
            return redirect(url_for("customer.cards"))
        if action == "withdraw":
            card_number = request.form.get("card", "")
            amount, error = money.parse_amount(request.form.get("amount"))
            if error:
                flash(error, "error")
            else:
                error = card_service.card_withdrawal(card_number, amount)
                if error:
                    flash(error, "error")
                else:
                    flash(f"Withdrew {money.format_paise(amount)} with your card.", "success")
            return redirect(url_for("customer.cards"))
        if action == "pin":
            card_number = request.form.get("card", "")
            pin = request.form.get("new_pin", "")
            error = card_service.set_card_pin(card_number, user_id, pin)
            if error:
                flash(error, "error")
            else:
                flash("Card PIN updated.", "success")
            return redirect(url_for("customer.cards"))
        if action == "toggle":
            card_number = request.form.get("card", "")
            block = request.form.get("block") == "1"
            error = card_service.toggle_block(
                card_number, user_id, block, request.form.get("reason", "")
            )
            if error:
                flash(error, "error")
            else:
                flash("Card blocked." if block else "Card unblocked.", "info")
            return redirect(url_for("customer.cards"))

    my_cards = card_service.cards_of(user_id)
    active_accounts = [a for a in accounts if a["status"] == "Active"]
    return render_template(
        "customer/cards.html", cards=my_cards, accounts=active_accounts
    )


# ---------- Notifications ----------

@customer_bp.get("/notifications")
@login_required
def notifications():
    rows = notification_service.notifications_of(session["user_id"])
    unread = sum(1 for n in rows if not n["is_read"])
    return render_template(
        "customer/notifications.html", rows=rows, unread=unread
    )


@customer_bp.post("/notifications/<int:notif_id>/read")
@login_required
def notification_read(notif_id):
    notification_service.mark_read(notif_id, session["user_id"])
    return redirect(url_for("customer.notifications"))


@customer_bp.post("/notifications/read-all")
@login_required
def notifications_read_all():
    notification_service.mark_all_read(session["user_id"])
    return redirect(url_for("customer.notifications"))


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

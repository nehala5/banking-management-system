"""JSON API used by the frontend for live calculators and charts."""
from flask import Blueprint, jsonify, request, session

from .. import money
from ..database import get_db
from ..services import (
    account_service, fd_service, loan_service, report_service, transaction_service,
)
from .decorators import admin_required, login_required

api_bp = Blueprint("api", __name__, url_prefix="/api")


@api_bp.get("/emi")
@login_required
def emi():
    """Live EMI quote: /api/emi?principal=1200000&rate=8.5&months=60"""
    try:
        principal, _ = money.parse_amount(request.args.get("principal", ""))
        rate = float(request.args.get("rate", ""))
        months = int(request.args.get("months", ""))
    except (TypeError, ValueError):
        return jsonify({"ok": False, "error": "Invalid parameters"}), 400
    if not principal or not (1 <= rate <= 36) or not (3 <= months <= 120):
        return jsonify({"ok": False, "error": "Out of range"}), 400

    emi = loan_service.compute_emi(principal, rate, months)
    total = emi * months
    interest = total - principal
    return jsonify({
        "ok": True,
        "principal": principal,
        "emi_paise": emi,
        "total_paise": total,
        "interest_paise": interest,
        "principal_display": money.format_paise(principal),
        "emi_display": money.format_paise(emi),
        "total_display": money.format_paise(total),
        "interest_display": money.format_paise(interest),
    })


@api_bp.get("/fd")
@login_required
def fd_quote():
    """Live FD quote: /api/fd?principal=500000&months=12"""
    try:
        principal, _ = money.parse_amount(request.args.get("principal", ""))
        months = int(request.args.get("months", ""))
    except (TypeError, ValueError):
        return jsonify({"ok": False, "error": "Invalid parameters"}), 400
    if not principal or not (3 <= months <= 60):
        return jsonify({"ok": False, "error": "Out of range"}), 400

    rate = fd_service.fd_rate_for(months)
    maturity = fd_service.maturity_paise(principal, rate, months)
    return jsonify({
        "ok": True,
        "rate": rate,
        "maturity_date": money.add_months(money.today(), months),
        "maturity_paise": maturity,
        "interest_paise": maturity - principal,
        "rate_display": f"{rate:g}% p.a.",
        "maturity_display": money.format_paise(maturity),
        "interest_display": money.format_paise(maturity - principal),
    })


@api_bp.get("/me/overview")
@login_required
def me_overview():
    """Chart data for the customer dashboard."""
    user_id = session["user_id"]
    accounts = account_service.accounts_of(user_id)
    recent = []
    for acct in accounts:
        recent.extend(transaction_service.history(acct["account_number"], 8))
    recent.sort(key=lambda r: r["id"], reverse=True)
    recent = recent[:8]
    return jsonify({
        "ok": True,
        "accounts": [
            {
                "number": a["account_number"],
                "type": a["type"],
                "balance": a["balance_paise"],
                "label": f"{a['type']} • {a['account_number']}",
            }
            for a in accounts
        ],
        "recent": [
            {
                "type": t["type"],
                "amount": t["amount_paise"],
                "remark": t["remark"] or t["counterparty"],
                "time": t["timestamp"],
            }
            for t in recent
        ],
    })


@api_bp.get("/admin/overview")
@admin_required
def admin_overview():
    """Chart data for the admin dashboard."""
    seven = transaction_service.last_seven_days()
    db = get_db()
    by_type = db.execute(
        "SELECT type, COUNT(*) AS c, COALESCE(SUM(amount_paise), 0) AS total"
        " FROM transactions GROUP BY type"
    ).fetchall()
    top = report_service.top_accounts(5)
    return jsonify({
        "ok": True,
        "seven_days": seven,
        "by_type": [
            {"type": t["type"], "count": t["c"], "total": t["total"]} for t in by_type
        ],
        "top_accounts": [
            {
                "number": a["account_number"],
                "name": a["user_name"],
                "balance": a["balance_paise"],
            }
            for a in top
        ],
    })

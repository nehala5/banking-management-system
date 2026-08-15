"""Public routes: landing, sign in, registration and sign out."""
from flask import Blueprint, flash, redirect, render_template, request, session, url_for

from .. import money
from ..services import account_service, auth_service

main_bp = Blueprint("main", __name__)


@main_bp.get("/")
def index():
    if session.get("user_id"):
        return _home_for_role()
    return render_template("auth/landing.html", greeting=money.greeting())


def _home_for_role():
    if session.get("role") == "ADMIN":
        return redirect(url_for("admin.dashboard"))
    return redirect(url_for("customer.dashboard"))


@main_bp.route("/login", methods=["GET", "POST"])
def login():
    if session.get("user_id"):
        return _home_for_role()

    if request.method == "POST":
        user_id = request.form.get("user_id", "").strip()
        pin = request.form.get("pin", "").strip()
        user, error = auth_service.verify_credentials(user_id, pin)
        if error:
            flash(error, "error")
        else:
            session.clear()
            session["user_id"] = user["id"]
            session["role"] = user["role"]
            session["name"] = user["name"]
            flash(f"Welcome back, {user['name']}!", "success")
            return _home_for_role()

    return render_template("auth/login.html", greeting=money.greeting())


@main_bp.route("/register", methods=["GET", "POST"])
def register():
    if session.get("user_id"):
        return _home_for_role()

    form = {"name": "", "email": "", "phone": "", "address": ""}
    if request.method == "POST":
        form["name"] = request.form.get("name", "")
        form["email"] = request.form.get("email", "")
        form["phone"] = request.form.get("phone", "")
        form["address"] = request.form.get("address", "")
        pin = request.form.get("pin", "")
        confirm = request.form.get("pin_confirm", "")
        if pin != confirm:
            flash("The two PINs do not match.", "error")
        else:
            user_id, error = auth_service.register(
                form["name"], form["email"], form["phone"], form["address"], pin
            )
            if error:
                flash(error, "error")
            else:
                number, acct_error = account_service.open_account(
                    user_id, "Savings", 0, "Welcome savings account",
                    require_minimum=False,
                )
                if acct_error:
                    flash(f"User created but account failed: {acct_error}", "error")
                session.clear()
                session["user_id"] = user_id
                session["role"] = "CUSTOMER"
                user = auth_service.get_user(user_id)
                session["name"] = user["name"]
                flash(
                    f"Welcome to OneSky Bank, {user['name']}! Your user ID is {user_id} "
                    f"and savings account {number} is ready.",
                    "success",
                )
                return redirect(url_for("customer.dashboard"))

    return render_template("auth/register.html", form=form)


@main_bp.get("/logout")
def logout():
    session.clear()
    flash("You have been signed out. See you soon!", "info")
    return redirect(url_for("main.index"))


@main_bp.app_errorhandler(404)
def not_found(_e):
    return render_template("errors/404.html"), 404


@main_bp.app_errorhandler(403)
def forbidden(_e):
    return render_template("errors/403.html"), 403


@main_bp.app_errorhandler(500)
def server_error(_e):
    return render_template("errors/500.html"), 500

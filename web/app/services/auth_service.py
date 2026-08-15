"""Authentication: registration, PIN verification and user lookup."""
import sqlite3
import re

from .. import money
from ..database import get_db, next_id


def register(name, email, phone, address, pin):
    """Register a new customer. Returns (user_id, None) or (None, error)."""
    name = (name or "").strip()
    email = (email or "").strip().lower()
    phone = (phone or "").strip()
    address = (address or "").strip()
    pin = (pin or "").strip()

    if len(name) < 2:
        return None, "Please enter your full name."
    if not re.fullmatch(r"[^@\s]+@[^@\s]+\.[^@\s]+", email):
        return None, "Please enter a valid email address."
    if not re.fullmatch(r"\d{10}", phone):
        return None, "Phone must be exactly 10 digits."
    if not re.fullmatch(r"\d{4}", pin):
        return None, "PIN must be exactly 4 digits."

    db = get_db()
    if db.execute("SELECT 1 FROM users WHERE email = ?", (email,)).fetchone():
        return None, "An account with this email already exists."

    user_id = next_id(db, "customer_seq")
    try:
        db.execute(
            "INSERT INTO users (id, name, email, phone, address, pin_hash, role, created_at)"
            " VALUES (?, ?, ?, ?, ?, ?, 'CUSTOMER', ?)",
            (user_id, name, email, phone, address,
             money.hash_pin(pin, money.salt_for(user_id)), money.now()),
        )
        db.commit()
    except sqlite3.IntegrityError:
        return None, "Could not create the account. Please try again."
    return user_id, None


def verify_credentials(user_id_raw, pin):
    """Check login credentials. Returns (user_row, None) or (None, error)."""
    user_id_raw = (user_id_raw or "").strip()
    pin = (pin or "").strip()
    if not user_id_raw.isdigit():
        return None, "User ID must be a number."
    if not re.fullmatch(r"\d{4}", pin):
        return None, "PIN must be exactly 4 digits."

    db = get_db()
    row = db.execute("SELECT * FROM users WHERE id = ?", (int(user_id_raw),)).fetchone()
    if row is None:
        return None, "Unknown user ID."
    if row["pin_hash"] != money.hash_pin(pin, money.salt_for(row["id"])):
        return None, "Incorrect PIN."
    return row, None


def get_user(user_id) -> sqlite3.Row | None:
    return get_db().execute("SELECT * FROM users WHERE id = ?", (user_id,)).fetchone()

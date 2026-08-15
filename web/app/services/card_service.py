"""Debit cards: Luhn-valid issuance, card withdrawals, PIN and block/unblock."""
import random
import re
import sqlite3

from .. import money
from ..database import get_db
from .account_service import get_account
from . import notification_service

BIN = "4871"  # OneSky Bank BIN prefix


def luhn_valid(number: str) -> bool:
    """Validate a card number against the Luhn checksum."""
    digits = [int(ch) for ch in number if ch.isdigit()]
    if len(digits) != 16:
        return False
    total = 0
    for i, d in enumerate(reversed(digits)):
        if i % 2 == 1:
            d *= 2
            if d > 9:
                d -= 9
        total += d
    return total % 10 == 0


def generate_card_number(db) -> str:
    """Generate a unique 16-digit, Luhn-valid card number."""
    while True:
        body = BIN + "".join(str(random.randint(0, 9)) for _ in range(11))
        total = 0
        for i, ch in enumerate(reversed(body)):
            d = int(ch)
            if i % 2 == 0:
                d *= 2
                if d > 9:
                    d -= 9
            total += d
        check = (10 - total % 10) % 10
        candidate = body + str(check)
        assert luhn_valid(candidate)
        if not db.execute(
            "SELECT 1 FROM cards WHERE card_number = ?", (candidate,)
        ).fetchone():
            return candidate


def issue_card(user_id, account_number, pin):
    """Issue a debit card against one of the user's accounts.

    Returns (card_row, None) or (None, error).
    """
    pin = (pin or "").strip()
    if not re.fullmatch(r"\d{4}", pin):
        return None, "Card PIN must be exactly 4 digits."

    db = get_db()
    acct = get_account(account_number)
    if acct is None or acct["user_id"] != user_id:
        return None, "Account not found or not yours."
    if acct["status"] != "Active":
        return None, "Account is closed — cannot issue a card."

    number = generate_card_number(db)
    db.execute(
        "INSERT INTO cards (card_number, user_id, account_number, pin_hash,"
        " expiry, status, issued_at) VALUES (?, ?, ?, ?, ?, 'Active', ?)",
        (number, user_id, account_number,
         money.hash_pin(pin, money.salt_for_card(number)), money.card_expiry(),
         money.now()),
    )
    notification_service.notify(
        user_id, "Debit card issued",
        f"Your OneSky debit card ending {number[-4:]} is active on {account_number}. "
        f"Expires {money.card_expiry()}.",
        "success",
    )
    db.commit()
    card = get_card(number)
    return card, None


def get_card(number):
    return get_db().execute(
        "SELECT * FROM cards WHERE card_number = ?", (number,)
    ).fetchone()


def cards_of(user_id):
    return get_db().execute(
        "SELECT * FROM cards WHERE user_id = ? ORDER BY issued_at DESC", (user_id,)
    ).fetchall()


def card_withdrawal(card_number, amount_paise):
    """Cash withdrawal via a card. Returns (None, error)."""
    db = get_db()
    card = get_card(card_number)
    if card is None:
        return "Card not found."
    if card["status"] != "Active":
        return "Card is not active."
    acct = get_account(card["account_number"])
    if acct is None or acct["status"] != "Active":
        return "Linked account is closed."
    if amount_paise <= 0:
        return "Amount must be positive."
    if amount_paise > acct["balance_paise"]:
        return (f"Insufficient balance. Available: "
                f"{money.format_paise(acct['balance_paise'])}")

    new_balance = acct["balance_paise"] - amount_paise
    db.execute(
        "UPDATE accounts SET balance_paise = ? WHERE account_number = ?",
        (new_balance, card["account_number"]),
    )
    db.execute(
        "INSERT INTO transactions (account_number, type, amount_paise,"
        " balance_after_paise, counterparty, timestamp, remark)"
        " VALUES (?, 'WITHDRAW', ?, ?, ?, ?, 'Card withdrawal')",
        (card["account_number"], amount_paise, new_balance,
         f"Card •••• {card['card_number'][-4:]}", money.now()),
    )
    notification_service.notify(
        card["user_id"], "Card withdrawal",
        f"{money.format_paise(amount_paise)} withdrawn from {card['account_number']} "
        f"with card ending {card['card_number'][-4:]}.",
        "warning",
    )
    db.commit()
    return None


def set_card_pin(card_number, user_id, new_pin):
    """Change a card's PIN. Returns error or None."""
    new_pin = (new_pin or "").strip()
    if not re.fullmatch(r"\d{4}", new_pin):
        return "Card PIN must be exactly 4 digits."
    db = get_db()
    card = get_card(card_number)
    if card is None or card["user_id"] != user_id:
        return "Card not found."
    db.execute(
        "UPDATE cards SET pin_hash = ? WHERE card_number = ?",
        (money.hash_pin(new_pin, money.salt_for_card(card_number)), card_number),
    )
    db.commit()
    return None


def toggle_block(card_number, user_id, block: bool, reason=""):
    """Block or unblock a card. Returns error or None."""
    db = get_db()
    card = get_card(card_number)
    if card is None or card["user_id"] != user_id:
        return "Card not found."
    if block and card["status"] == "Blocked":
        return "Card is already blocked."
    if not block and card["status"] == "Active":
        return "Card is already active."
    new_status = "Blocked" if block else "Active"
    db.execute(
        "UPDATE cards SET status = ? WHERE card_number = ?",
        (new_status, card_number),
    )
    verb = "blocked" if block else "unblocked"
    reason_text = f" Reason: {reason}." if reason else ""
    notification_service.notify(
        user_id, f"Card {verb}",
        f"Your card ending {card['card_number'][-4:]} was {verb}.{reason_text}",
        "warning" if block else "info",
    )
    db.commit()
    return None

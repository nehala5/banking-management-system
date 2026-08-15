"""Money and date helpers shared across services and templates.

All monetary values are stored as integer paise (1/100th of a rupee) to avoid
floating-point rounding errors, exactly like the C++ version.
"""
import hashlib
import re
from datetime import datetime, timedelta


def now() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def today() -> str:
    return datetime.now().strftime("%Y-%m-%d")


def add_months(date_str: str, months: int) -> str:
    d = datetime.strptime(date_str, "%Y-%m-%d")
    month = d.month - 1 + months
    year = d.year + month // 12
    month = month % 12 + 1
    day = min(d.day, [31, 29 if _leap(year) else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31][month - 1])
    return f"{year:04d}-{month:02d}-{day:02d}"


def _leap(y: int) -> bool:
    return y % 4 == 0 and (y % 100 != 0 or y % 400 == 0)


def format_paise(paise: int) -> str:
    """₹1,24,560.20 — Indian digit grouping."""
    neg = paise < 0
    paise = abs(paise)
    rupees, p = divmod(paise, 100)
    body = _group(rupees) + "." + f"{p:02d}"
    return ("-₹" if neg else "₹") + body


def format_paise_plain(paise: int) -> str:
    neg = paise < 0
    paise = abs(paise)
    rupees, p = divmod(paise, 100)
    body = _group(rupees) + "." + f"{p:02d}"
    return ("-" if neg else "") + body


def _group(rupees: int) -> str:
    s = str(rupees)
    if len(s) <= 3:
        return s
    tail = s[-3:]
    head = s[:-3]
    parts = []
    while len(head) > 2:
        parts.insert(0, head[-2:])
        head = head[:-2]
    if head:
        parts.insert(0, head)
    return ",".join(parts) + "," + tail


def parse_amount(raw: str):
    """Parse a user-typed amount like '1500' or '1500.50' into paise.

    Returns (paise, None) on success or (None, error_message).
    """
    raw = (raw or "").strip().replace(",", "")
    if not raw:
        return None, "Please enter an amount."
    if not re.fullmatch(r"\d+(\.\d{1,2})?", raw):
        return None, "Invalid amount. Use a format like 1500 or 1500.50"
    rupees, _, frac = raw.partition(".")
    frac = (frac + "00")[:2]
    try:
        total = int(rupees) * 100 + int(frac)
    except ValueError:
        return None, "Invalid amount."
    return total, None


def hash_pin(pin: str, salt: str) -> str:
    """Salted SHA-256 of the PIN (demo-grade; not a KDF)."""
    return hashlib.sha256(f"{salt}:{pin}".encode()).hexdigest()


def salt_for(user_id) -> str:
    """Deterministic per-user salt so the hash can be re-derived at login."""
    return f"onesky-salt-{user_id}"


def salt_for_card(card_number: str) -> str:
    """Per-card salt for card PIN hashes."""
    return f"onesky-card-{card_number}"


def fd_maturity(principal_paise: int, rate: float, months: int) -> int:
    """Projected FD maturity value with quarterly compounding."""
    r = rate / 400.0
    n = months / 12.0
    return round(principal_paise * (1 + r) ** (4 * n))


def card_expiry() -> str:
    """Expiry date MM/YY, three years from now."""
    d = datetime.now()
    year = d.year + 3
    return f"{d.month:02d}/{year % 100:02d}"


def mask_card(number: str) -> str:
    """'XXXXXXXXXXXX4521' style masking of a 16-digit card number."""
    return "XXXXXXXXXXXX" + number[-4:]


def greeting() -> str:
    h = datetime.now().hour
    if h < 12:
        return "Good morning"
    if h < 17:
        return "Good afternoon"
    return "Good evening"

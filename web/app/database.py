"""SQLite database layer: schema, connection management and demo seed data."""
import os
import sqlite3

from flask import g

from . import money
from .config import Config

SCHEMA = """
CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS users (
    id         INTEGER PRIMARY KEY,
    name       TEXT NOT NULL,
    email      TEXT NOT NULL UNIQUE,
    phone      TEXT NOT NULL,
    address    TEXT NOT NULL DEFAULT '',
    pin_hash   TEXT NOT NULL,
    role       TEXT NOT NULL DEFAULT 'CUSTOMER',
    created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS accounts (
    account_number        TEXT PRIMARY KEY,
    user_id               INTEGER NOT NULL REFERENCES users(id),
    type                  TEXT NOT NULL DEFAULT 'Savings',
    balance_paise         INTEGER NOT NULL DEFAULT 0,
    interest_rate         REAL NOT NULL DEFAULT 3.5,
    status                TEXT NOT NULL DEFAULT 'Active',
    created_at            TEXT NOT NULL,
    last_interest_applied TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS transactions (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    account_number      TEXT NOT NULL REFERENCES accounts(account_number),
    type                TEXT NOT NULL,
    amount_paise        INTEGER NOT NULL,
    balance_after_paise INTEGER NOT NULL,
    counterparty        TEXT NOT NULL DEFAULT '',
    timestamp           TEXT NOT NULL,
    remark              TEXT NOT NULL DEFAULT ''
);

CREATE TABLE IF NOT EXISTS loans (
    loan_id            TEXT PRIMARY KEY,
    user_id            INTEGER NOT NULL REFERENCES users(id),
    account_number     TEXT NOT NULL REFERENCES accounts(account_number),
    principal_paise    INTEGER NOT NULL,
    outstanding_paise  INTEGER NOT NULL,
    rate               REAL NOT NULL,
    tenure_months      INTEGER NOT NULL,
    emi_paise          INTEGER NOT NULL DEFAULT 0,
    emi_paid_count     INTEGER NOT NULL DEFAULT 0,
    amount_paid_paise  INTEGER NOT NULL DEFAULT 0,
    status             TEXT NOT NULL DEFAULT 'Pending',
    applied_date       TEXT NOT NULL,
    decision_date      TEXT NOT NULL DEFAULT '',
    next_due_date      TEXT NOT NULL DEFAULT ''
);
"""


def _db_path() -> str:
    path = Config.DATABASE
    os.makedirs(os.path.dirname(path), exist_ok=True)
    return path


def _connect() -> sqlite3.Connection:
    conn = sqlite3.connect(_db_path())
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA foreign_keys = ON")
    return conn


def get_db() -> sqlite3.Connection:
    """Return the per-request connection (Flask request context)."""
    if "db" not in g:
        g.db = _connect()
    return g.db


def close_db(_exc=None):
    db = g.pop("db", None)
    if db is not None:
        db.close()


def _seed(conn: sqlite3.Connection):
    """Seed demo users and transactions on a brand-new database."""
    now = money.now()

    conn.executemany(
        "INSERT INTO users (id, name, email, phone, address, pin_hash, role, created_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        [
            (1000, "OneSky Admin", "admin@onesky.example", "9999900000",
             "Bank Head Office, Sky Tower, Mumbai",
             money.hash_pin("1234", money.salt_for(1000)), "ADMIN", now),
            (1001, "Aarav Sharma", "aarav@example.com", "9876543210",
             "14, Rose Garden, Pune",
             money.hash_pin("0000", money.salt_for(1001)), "CUSTOMER", now),
        ],
    )

    conn.executemany(
        "INSERT INTO meta (key, value) VALUES (?, ?)",
        [("customer_seq", 1001), ("account_seq", 0), ("loan_seq", 0)],
    )

    # Aarav's savings + checking accounts.
    conn.execute(
        "INSERT INTO accounts (account_number, user_id, type, balance_paise,"
        " interest_rate, status, created_at, last_interest_applied)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        ("OB000001", 1001, "Savings", 9842736, 3.5, "Active", now, money.today()),
    )
    conn.execute(
        "INSERT INTO accounts (account_number, user_id, type, balance_paise,"
        " interest_rate, status, created_at, last_interest_applied)"
        " VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        ("OB000002", 1001, "Checking", 250000, 0.0, "Active", now, money.today()),
    )
    conn.execute("UPDATE meta SET value = value + 2 WHERE key = 'account_seq'")

    txs = [
        ("OB000001", "OPENING", 100000, 100000, "Opening deposit", now, "New account opened"),
        ("OB000001", "DEPOSIT", 100000, 200000, "Cash deposit", now, "Savings deposit"),
        ("OB000001", "WITHDRAW", 10000, 190000, "ATM withdrawal", now, "Cash withdrawal"),
        ("OB000001", "DEPOSIT", 100000, 290000, "Cheque deposit", now, "Self deposit"),
        ("OB000001", "TRANSFER_OUT", 50000, 240000, "OB000002", now, "Transfer to checking"),
        ("OB000002", "TRANSFER_IN", 50000, 300000, "OB000001", now, "Transfer from savings"),
        ("OB000001", "WITHDRAW", 15000, 225000, "Ravi Kumar", now, "Bill payment"),
        ("OB000001", "TRANSFER_OUT", 30000, 195000, "OB000002", now, "Monthly budget top-up"),
        ("OB000002", "TRANSFER_IN", 30000, 330000, "OB000001", now, "Monthly budget top-up"),
        ("OB000001", "DEPOSIT", 200000, 395000, "Skyline Corp Pvt Ltd", now, "Monthly salary"),
        ("OB000002", "WITHDRAW", 80000, 250000, "Verma Housing", now, "Monthly rent"),
        ("OB000001", "WITHDRAW", 5000, 390000, "Foodmart Supermarket", now, "Groceries"),
        ("OB000001", "DEPOSIT", 75000, 465000, "OneSky Bank", now, "Savings deposit"),
    ]
    conn.executemany(
        "INSERT INTO transactions (account_number, type, amount_paise,"
        " balance_after_paise, counterparty, timestamp, remark)"
        " VALUES (?, ?, ?, ?, ?, ?, ?)",
        txs,
    )

    conn.commit()


def init_db():
    """Create tables and seed demo data if the database does not exist yet."""
    if os.path.exists(_db_path()):
        return
    conn = _connect()
    try:
        conn.executescript(SCHEMA)
        _seed(conn)
    finally:
        conn.close()


def next_id(conn: sqlite3.Connection, key: str) -> int:
    """Atomically fetch-and-increment a sequence counter from the meta table."""
    cur = conn.execute("SELECT value FROM meta WHERE key = ?", (key,))
    row = cur.fetchone()
    value = row["value"] + 1 if row else 1
    conn.execute(
        "INSERT INTO meta (key, value) VALUES (?, ?)"
        " ON CONFLICT(key) DO UPDATE SET value = excluded.value",
        (key, value),
    )
    return value

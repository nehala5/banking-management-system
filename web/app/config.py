import os

BASE_DIR = os.path.abspath(os.path.dirname(os.path.dirname(__file__)))


class Config:
    # Change this before deploying anywhere real.
    SECRET_KEY = os.environ.get("SECRET_KEY", "onesky-bank-demo-secret-change-me")

    # SQLite database location (created automatically on first run).
    DATABASE = os.environ.get(
        "DATABASE_PATH", os.path.join(BASE_DIR, "data", "bank.db")
    )

    # Keep request bodies small — it is a form app, not an uploader.
    MAX_CONTENT_LENGTH = 1024 * 1024

"""In-app notifications: created alongside every important banking event."""
from .. import money
from ..database import get_db


def notify(user_id, title, message, kind="info"):
    """Record a notification for a user (called inside the same transaction)."""
    get_db().execute(
        "INSERT INTO notifications (user_id, title, message, kind, created_at)"
        " VALUES (?, ?, ?, ?, ?)",
        (user_id, title, message, kind, money.now()),
    )


def unread_count(user_id) -> int:
    row = get_db().execute(
        "SELECT COUNT(*) AS c FROM notifications WHERE user_id = ? AND is_read = 0",
        (user_id,),
    ).fetchone()
    return row["c"] if row else 0


def notifications_of(user_id, limit=100):
    return get_db().execute(
        "SELECT * FROM notifications WHERE user_id = ? ORDER BY id DESC LIMIT ?",
        (user_id, limit),
    ).fetchall()


def get_notification(notif_id, user_id):
    return get_db().execute(
        "SELECT * FROM notifications WHERE id = ? AND user_id = ?",
        (notif_id, user_id),
    ).fetchone()


def mark_read(notif_id, user_id):
    db = get_db()
    db.execute(
        "UPDATE notifications SET is_read = 1 WHERE id = ? AND user_id = ?",
        (notif_id, user_id),
    )
    db.commit()


def mark_all_read(user_id):
    db = get_db()
    db.execute(
        "UPDATE notifications SET is_read = 1 WHERE user_id = ? AND is_read = 0",
        (user_id,),
    )
    db.commit()

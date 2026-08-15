"""OneSky Bank web application factory."""
from flask import Flask, session

from . import money
from .config import Config
from .database import close_db, init_db


def create_app() -> Flask:
    app = Flask(__name__)
    app.config.from_object(Config)

    init_db()
    app.teardown_appcontext(close_db)

    app.jinja_env.filters["inr"] = money.format_paise
    app.jinja_env.filters["inr2"] = money.format_paise_plain

    @app.context_processor
    def inject_globals():
        from .services.auth_service import get_user

        user_id = session.get("user_id")
        user = get_user(user_id) if user_id else None
        return {
            "current_user": user,
            "greeting": money.greeting(),
        }

    from .routes import register_blueprints

    register_blueprints(app)
    return app

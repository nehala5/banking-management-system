"""Blueprint registry for the OneSky Bank web app."""
from .admin_routes import admin_bp
from .api_routes import api_bp
from .customer_routes import customer_bp
from .main_routes import main_bp


def register_blueprints(app):
    app.register_blueprint(main_bp)
    app.register_blueprint(customer_bp)
    app.register_blueprint(admin_bp)
    app.register_blueprint(api_bp)

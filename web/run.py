from app import create_app

app = create_app()

if __name__ == "__main__":
    print("OneSky Bank — web edition")
    print("  Visit http://127.0.0.1:5000")
    print("  Admin login -> ID 1000, PIN 1234")
    print("  Demo login  -> ID 1001, PIN 0000")
    app.run(debug=True)

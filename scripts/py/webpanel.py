from flask import Flask, render_template, request, redirect
import services

app = Flask(__name__)

@app.route("/")
def index():
    status = services.get_status()
    return render_template("index.html", services=status)

@app.route("/start/<name>")
def start(name):
    cfg = services.load_config()
    for s in cfg["services"]:
        if s["name"] == name:
            services.start_service(s)
    return redirect("/")

@app.route("/stop/<name>")
def stop(name):
    services.stop_service(name)
    return redirect("/")

@app.route("/restart/<name>")
def restart(name):
    cfg = services.load_config()
    for s in cfg["services"]:
        if s["name"] == name:
            services.restart_service(s)
    return redirect("/")

@app.route("/toggle/<name>")
def toggle(name):
    cfg = services.load_config()
    for s in cfg["services"]:
        if s["name"] == name:
            s["enabled"] = not s["enabled"]
    services.save_config(cfg)
    return redirect("/")

@app.route("/delete/<name>")
def delete(name):
    cfg = services.load_config()
    cfg["services"] = [s for s in cfg["services"] if s["name"] != name]
    services.save_config(cfg)
    return redirect("/")

@app.route("/add", methods=["POST"])
def add():
    cfg = services.load_config()
    cfg["services"].append({
        "name": request.form["name"],
        "command": request.form["command"],
        "enabled": True
    })
    services.save_config(cfg)
    return redirect("/")

@app.route("/logs/<name>")
def logs(name):
    log_lines = services.read_logs(name)
    return render_template("logs.html", name=name, logs=log_lines)

@app.route("/restart_all")
def restart_all():
    services.restart_all()
    return redirect("/")

@app.route("/stop_all")
def stop_all():
    services.stop_all()
    return redirect("/")

@app.route("/kill_all")
def kill_all():
    services.kill_all()
    return redirect("/")

if __name__ == "__main__":
    services.start_enabled()
    app.run(host="0.0.0.0", port=5000)
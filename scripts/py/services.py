import subprocess
import json
import os
import signal
import psutil
from datetime import datetime

CONFIG_FILE = "/home/tiledb/apps/tile_xvcpi/scripts/py/config.json"
WORKDIR = "/home/tiledb/apps/tile_xvcpi"
LOG_DIR = os.path.join(WORKDIR, "logs")
os.makedirs(LOG_DIR, exist_ok=True)

processes = {}

def kill_all():
    cfg = load_config()
    for s in cfg["services"]:
        name = s["name"]
        try:
            # Use pkill to match the exact command string
            subprocess.run(["sudo", "pkill", "-f", s["command"]])
            log_service(name, f"Killed all processes for {name}")
        except Exception as e:
            log_service(name, f"Failed to kill {name}: {e}")
    processes.clear()  # clear tracked Popen objects

def load_config():
    with open(CONFIG_FILE) as f:
        return json.load(f)

def save_config(cfg):
    with open(CONFIG_FILE, "w") as f:
        json.dump(cfg, f, indent=2)

def log_service(name, message):
    log_file = os.path.join(LOG_DIR, f"{name}.log")
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    with open(log_file, "a") as f:
        f.write(f"[{timestamp}] {message}\n")

def start_service(service):
    name = service["name"]
    if name in processes:
        return
    log_service(name, "Starting service")
    try:
        # Run with sudo
        p = subprocess.Popen(
            f"sudo {service['command']}",
            shell=True,
            cwd=WORKDIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        processes[name] = p
        log_service(name, f"Started with PID {p.pid}")
    except Exception as e:
        log_service(name, f"Failed to start: {e}")

def stop_service(name, command):
    """Stop all processes matching a service command."""
    try:
        # kill any process matching this command
        subprocess.run(["sudo", "pkill", "-f", command])
        log_service(name, f"Stopped all processes for {name}")
    except Exception as e:
        log_service(name, f"Failed to stop {name}: {e}")
    # remove from tracked processes
    if name in processes:
        del processes[name]

def restart_service(service):
    stop_service(service["name"], service["command"])  # kill first
    start_service(service)                              # then start

def start_enabled():
    cfg = load_config()
    for s in cfg["services"]:
        if s["enabled"]:
            start_service(s)

def restart_all():
    cfg = load_config()
    for s in cfg["services"]:
        if s["enabled"]:
            restart_service(s)   # safe: old processes killed first

def get_status():
    status = []
    cfg = load_config()
    for s in cfg["services"]:
        pid = processes[s["name"]].pid if s["name"] in processes else None
        running = False
        cpu = 0
        mem = 0
        if pid and psutil.pid_exists(pid):
            p = psutil.Process(pid)
            running = True
            cpu = p.cpu_percent(interval=0.1)
            mem = p.memory_info().rss / (1024 * 1024)
        status.append({
            "name": s["name"],
            "enabled": s["enabled"],
            "command": s["command"],
            "running": running,
            "cpu": cpu,
            "memory": mem
        })
    return status

def read_logs(name, lines=100):
    log_file = os.path.join(LOG_DIR, f"{name}.log")
    if os.path.exists(log_file):
        with open(log_file) as f:
            return f.readlines()[-lines:]
    return []
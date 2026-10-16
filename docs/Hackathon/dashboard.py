#!/usr/bin/env python3
"""Small local dashboard for the System V shared-memory demo."""
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs
import html
import json
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BINARY = ROOT / "shm_exchange"
HOST, PORT = "127.0.0.1", 8000

def run_command(*args):
    try:
        result = subprocess.run([str(BINARY), *args], cwd=ROOT, text=True,
                                capture_output=True, timeout=3)
        return result.returncode, result.stdout.strip(), result.stderr.strip()
    except Exception as exc:
        return 1, "", str(exc)

def get_status():
    code, out, err = run_command("status")
    if code != 0:
        return {"message": "Not initialized — run ./shm_exchange init",
                "version": 0, "writer_pid": 0, "updated_at": 0, "error": err or out}
    try:
        return json.loads(out)
    except json.JSONDecodeError:
        return {"message": out, "version": 0, "writer_pid": 0, "updated_at": 0}

PAGE = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta http-equiv="refresh" content="2">
<title>Shared Memory Exchange Dashboard</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#0b1220;color:#e8eefc;font:16px system-ui,Arial}
header{padding:28px 7%;background:linear-gradient(120deg,#142746,#0e7490)}
h1{margin:0 0 8px;font-size:clamp(24px,4vw,38px)}header p{margin:0;color:#d5e7ff}
main{max-width:1000px;margin:28px auto;padding:0 18px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:14px}
.card{background:#121e32;border:1px solid #263a57;border-radius:15px;padding:20px}
.label{color:#9fb3d1;font-size:13px;text-transform:uppercase;letter-spacing:1px}
.value{font-size:27px;font-weight:700;margin-top:8px;overflow-wrap:anywhere}
.message{font-size:20px;white-space:pre-wrap;overflow-wrap:anywhere}
form{display:flex;gap:10px;flex-wrap:wrap;margin-top:14px}
input{flex:1;min-width:220px;padding:13px;border-radius:9px;border:1px solid #39516f;background:#0b1220;color:white}
button{padding:13px 20px;border:0;border-radius:9px;background:#22c1a6;color:#06231f;font-weight:700;cursor:pointer}
.note{color:#a8bad5;font-size:14px;line-height:1.6}.status{color:#6ee7b7;font-weight:700}
footer{color:#8195b4;text-align:center;padding:24px}
</style></head><body>
<header><h1>Shared Memory Data Exchange</h1>
<p>Live IPC dashboard · System V shared memory · POSIX process-shared semaphore</p></header>
<main>
<div class="grid">
 <section class="card"><div class="label">Current version</div><div class="value">VERSION</div></section>
 <section class="card"><div class="label">Last writer PID</div><div class="value">PID</div></section>
 <section class="card"><div class="label">Last updated</div><div class="value" style="font-size:18px">UPDATED</div></section>
</div>
<section class="card" style="margin-top:14px"><div class="label">Message in shared memory</div>
<div class="message" style="margin-top:12px">MESSAGE</div>
<form method="post" action="/write"><input name="message" maxlength="511" required placeholder="Enter a message to share...">
<button type="submit">Write to shared memory</button></form>
<p class="note">The page refreshes every 2 seconds. The dashboard calls the C program, which reads/writes the shared-memory segment. Keep this server bound to 127.0.0.1 for a controlled local demo.</p>
</section>
<section class="card" style="margin-top:14px"><div class="label">System status</div><p class="status">STATUS</p><p class="note">Consistency: every read and write holds the same process-shared semaphore while copying the message and metadata.</p></section>
</main><footer>Hackathon prototype · Local machine only</footer></body></html>"""

class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/api/status":
            body = json.dumps(get_status()).encode()
            self.send_response(200); self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)
            return
        data = get_status()
        updated = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(data["updated_at"])) if data.get("updated_at") else "Not yet written"
        initialized = "Shared memory connected" if not data.get("error") else "Shared memory unavailable — initialize it first"
        page = PAGE.replace("VERSION", html.escape(str(data.get("version", 0))))
        page = page.replace("PID", html.escape(str(data.get("writer_pid", 0))))
        page = page.replace("UPDATED", html.escape(updated))
        page = page.replace("MESSAGE", html.escape(str(data.get("message", ""))))
        page = page.replace("STATUS", html.escape(initialized))
        body = page.encode()
        self.send_response(200); self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body)

    def do_POST(self):
        if self.path != "/write":
            self.send_error(404); return
        length = int(self.headers.get("Content-Length", "0"))
        form = parse_qs(self.rfile.read(min(length, 4096)).decode("utf-8", "replace"))
        message = form.get("message", [""])[0].strip()
        if not message:
            self.send_error(400, "Message cannot be empty"); return
        code, out, err = run_command("write", message[:511])
        body_text = out if code == 0 else (err or out or "Write failed")
        body = ("<html><body style='font:18px system-ui;background:#0b1220;color:white;padding:30px'>"
                "<h2>" + html.escape(body_text) + "</h2><p><a style='color:#6ee7b7' href='/'>Back to dashboard</a></p>"
                "</body></html>").encode()
        self.send_response(303); self.send_header("Location", "/")
        self.send_header("Content-Length", "0"); self.end_headers()

    def log_message(self, fmt, *args):
        print("[dashboard] " + (fmt % args))

if __name__ == "__main__":
    if not BINARY.exists():
        raise SystemExit("Missing ./shm_exchange. Compile first: gcc -Wall -Wextra -O2 shm_exchange.c -o shm_exchange -pthread")
    print(f"Dashboard running at http://{HOST}:{PORT} (Ctrl+C to stop)")
    HTTPServer((HOST, PORT), Handler).serve_forever()

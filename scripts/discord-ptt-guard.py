#!/usr/bin/env python3
"""
Discord PTT Guard — monitors Push-to-Talk mode via Discord IPC.
Sends a macOS notification (via noti) every minute while PTT is disabled.
Reacts instantly when PTT mode changes — no polling, no LevelDB delay.
"""

import socket
import struct
import json
import os
import sys
import time
import select
import subprocess
import logging
import atexit
import urllib.request
import urllib.parse
import urllib.error

REMIND_INTERVAL = 15
TOKEN_FILE = os.path.expanduser("~/.discord-ptt-token.json")
LOG_FILE = os.path.expanduser("~/discord-ptt-guard.log")
LOCKFILE = "/tmp/discord-ptt-guard.lock"
NOTI = "/opt/homebrew/bin/noti"

REFRESH_BUFFER = 86400       # refresh a day before expiry
REFRESH_RETRY_INTERVAL = 300  # don't hit the Discord token endpoint more than once per 5 min
AUTH_NOTIFY_INTERVAL = 3600   # don't notify about broken auth more than once per hour

logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] %(message)s',
    datefmt='%H:%M:%S',
    handlers=[logging.FileHandler(LOG_FILE)],
)
log = logging.getLogger(__name__)


# ── lockfile ──────────────────────────────────────────────────────────────────

def acquire_lock():
    try:
        with open(LOCKFILE) as f:
            pid = int(f.read().strip())
        os.kill(pid, 0)
        log.info(f"Already running (PID {pid}), exiting.")
        sys.exit(0)
    except (FileNotFoundError, ProcessLookupError, ValueError):
        pass
    with open(LOCKFILE, 'w') as f:
        f.write(str(os.getpid()))
    atexit.register(lambda: os.path.exists(LOCKFILE) and os.unlink(LOCKFILE))


# ── IPC helpers ───────────────────────────────────────────────────────────────

def pack(opcode, data):
    payload = json.dumps(data).encode()
    return struct.pack('<II', opcode, len(payload)) + payload


def recv_msg(sock):
    raw = b''
    while len(raw) < 8:
        chunk = sock.recv(8 - len(raw))
        if not chunk:
            raise ConnectionError("IPC socket closed")
        raw += chunk
    opcode, length = struct.unpack('<II', raw)
    data = b''
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise ConnectionError("IPC socket closed")
        data += chunk
    return opcode, json.loads(data.decode())


def find_socket():
    tmpdir = os.environ.get('TMPDIR', '/tmp').rstrip('/')
    for i in range(10):
        path = f'{tmpdir}/discord-ipc-{i}'
        if os.path.exists(path):
            return path
    return None


# ── notifications ─────────────────────────────────────────────────────────────

def notify(message, sound='Ping'):
    title = f"Discord PTT • {time.strftime('%H:%M:%S')}"
    subprocess.run(
        [NOTI, '-t', title, '-m', message],
        env={**os.environ, 'NOTI_NSN_SOUND': sound},
        capture_output=True,
    )


_last_auth_notify = 0

def notify_auth_broken(detail):
    global _last_auth_notify
    now = time.time()
    if now - _last_auth_notify < AUTH_NOTIFY_INTERVAL:
        return
    _last_auth_notify = now
    notify(f"Discord token isn't refreshing: {detail}. Re-authorization needed (discord-ptt-setup.py).", sound='Basso')


# ── token refresh ─────────────────────────────────────────────────────────────

def load_token():
    with open(TOKEN_FILE) as f:
        return json.load(f)


def save_token(cfg):
    with open(TOKEN_FILE, 'w') as f:
        json.dump(cfg, f, indent=2)
    os.chmod(TOKEN_FILE, 0o600)


def refresh_access_token(cfg):
    body = urllib.parse.urlencode({
        'client_id': cfg['client_id'],
        'client_secret': cfg['client_secret'],
        'grant_type': 'refresh_token',
        'refresh_token': cfg['refresh_token'],
    }).encode()
    req = urllib.request.Request(
        'https://discord.com/api/oauth2/token',
        data=body,
        headers={
            'Content-Type': 'application/x-www-form-urlencoded',
            'User-Agent': 'discord-ptt-guard/1.0',
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=15) as resp:
            data = json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        raise Exception(f"HTTP {e.code}: {e.read().decode()}")

    if 'access_token' not in data:
        raise Exception(f"Refresh failed: {data}")

    cfg['access_token'] = data['access_token']
    cfg['refresh_token'] = data.get('refresh_token', cfg['refresh_token'])
    cfg['expires_at'] = time.time() + data.get('expires_in', 604800)
    save_token(cfg)
    log.info(f"Token refreshed OK, valid until {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(cfg['expires_at']))}")
    return cfg


_last_refresh_attempt = 0

def try_refresh(cfg):
    """Tries to refresh the token, at most once per REFRESH_RETRY_INTERVAL. Returns (cfg, ok)."""
    global _last_refresh_attempt
    now = time.time()
    if now - _last_refresh_attempt < REFRESH_RETRY_INTERVAL:
        return cfg, False
    _last_refresh_attempt = now
    try:
        cfg = refresh_access_token(cfg)
        return cfg, True
    except Exception as e:
        log.error(f"Token refresh failed: {e}")
        notify_auth_broken(str(e))
        return cfg, False


# ── main session ──────────────────────────────────────────────────────────────

def run_session(client_id, token):
    sock_path = find_socket()
    if not sock_path:
        raise Exception("Discord IPC socket not found (is Discord running?)")

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(sock_path)

    # Handshake
    sock.sendall(pack(0, {"v": 1, "client_id": client_id}))
    _, resp = recv_msg(sock)
    if resp.get('evt') != 'READY':
        raise Exception(f"Handshake failed: {resp}")

    # Authenticate
    sock.sendall(pack(1, {"cmd": "AUTHENTICATE", "args": {"access_token": token}, "nonce": "auth"}))
    _, resp = recv_msg(sock)
    if resp.get('evt') == 'ERROR':
        raise Exception(f"Auth failed: {resp['data']['message']}")
    log.info("Authenticated OK")

    # Get current mode
    sock.sendall(pack(1, {"cmd": "GET_VOICE_SETTINGS", "args": {}, "nonce": "gvs"}))
    _, resp = recv_msg(sock)
    current_mode = resp.get('data', {}).get('mode', {}).get('type', 'UNKNOWN')
    log.info(f"Current mode: {current_mode}")

    # Subscribe to real-time updates
    sock.sendall(pack(1, {
        "cmd": "SUBSCRIBE",
        "evt": "VOICE_SETTINGS_UPDATE",
        "args": {},
        "nonce": "sub",
    }))
    recv_msg(sock)
    log.info("Subscribed to VOICE_SETTINGS_UPDATE")

    last_notified_at = 0

    # Notify immediately if PTT is already off
    if current_mode == 'VOICE_ACTIVITY':
        notify("Turn on Push to Talk!")
        last_notified_at = time.time()
        log.info("Notification sent (initial state: PTT off)")

    # Event loop
    while True:
        now = time.time()
        wait = max(0, REMIND_INTERVAL - (now - last_notified_at)) if last_notified_at else REMIND_INTERVAL

        readable, _, _ = select.select([sock], [], [], wait)

        if readable:
            _, resp = recv_msg(sock)
            evt = resp.get('evt')

            if evt == 'VOICE_SETTINGS_UPDATE':
                mode = resp.get('data', {}).get('mode', {}).get('type')
                log.info(f"Mode changed: {mode}")

                if mode == 'VOICE_ACTIVITY':
                    notify("Turn on Push to Talk!")
                    last_notified_at = time.time()
                    log.info("Notification sent")
                elif mode == 'PUSH_TO_TALK':
                    last_notified_at = 0
                    log.info("PTT enabled — resetting")
        else:
            # Timeout — send reminder
            if last_notified_at and time.time() - last_notified_at >= REMIND_INTERVAL:
                notify("Turn on Push to Talk!")
                last_notified_at = time.time()
                log.info("Reminder sent")


# ── entry point ───────────────────────────────────────────────────────────────

def main():
    acquire_lock()

    if not os.path.exists(TOKEN_FILE):
        log.error(f"Token not found at {TOKEN_FILE}. Run discord-ptt-setup.py first.")
        sys.exit(1)

    cfg = load_token()
    log.info(f"=== PTT Guard started (PID {os.getpid()}) ===")

    while True:
        # Proactive refresh — a day before access_token expiry
        if cfg.get('expires_at') is None or time.time() >= cfg['expires_at'] - REFRESH_BUFFER:
            log.info("Access token missing/about to expire — attempting proactive refresh")
            cfg, _ = try_refresh(cfg)

        try:
            run_session(cfg['client_id'], cfg['access_token'])
        except Exception as e:
            msg = str(e)
            if 'Auth failed' in msg or 'Invalid access token' in msg:
                log.info("Auth failed — attempting reactive refresh")
                cfg, _ = try_refresh(cfg)
            log.error(f"Session error: {e} — retry in 15s")
            time.sleep(15)


if __name__ == '__main__':
    main()

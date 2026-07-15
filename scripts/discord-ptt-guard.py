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

REMIND_INTERVAL = 15
TOKEN_FILE = os.path.expanduser("~/.discord-ptt-token.json")
LOG_FILE = os.path.expanduser("~/discord-ptt-guard.log")
LOCKFILE = "/tmp/discord-ptt-guard.lock"
NOTI = "/opt/homebrew/bin/noti"

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

def notify(message):
    title = f"Discord PTT • {time.strftime('%H:%M:%S')}"
    subprocess.run(
        [NOTI, '-t', title, '-m', message],
        env={**os.environ, 'NOTI_NSN_SOUND': 'Ping'},
        capture_output=True,
    )


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
        notify("Увімкни Push to Talk!")
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
                    notify("Увімкни Push to Talk!")
                    last_notified_at = time.time()
                    log.info("Notification sent")
                elif mode == 'PUSH_TO_TALK':
                    last_notified_at = 0
                    log.info("PTT увімкнено — скидаю")
        else:
            # Timeout — send reminder
            if last_notified_at and time.time() - last_notified_at >= REMIND_INTERVAL:
                notify("Увімкни Push to Talk!")
                last_notified_at = time.time()
                log.info("Reminder sent")


# ── entry point ───────────────────────────────────────────────────────────────

def main():
    acquire_lock()

    if not os.path.exists(TOKEN_FILE):
        log.error(f"Token not found at {TOKEN_FILE}. Run discord-ptt-setup.py first.")
        sys.exit(1)

    with open(TOKEN_FILE) as f:
        cfg = json.load(f)
    client_id = cfg['client_id']
    token = cfg['access_token']

    log.info(f"=== PTT Guard started (PID {os.getpid()}) ===")

    while True:
        try:
            run_session(client_id, token)
        except Exception as e:
            log.error(f"Session error: {e} — retry in 15s")
            time.sleep(15)


if __name__ == '__main__':
    main()

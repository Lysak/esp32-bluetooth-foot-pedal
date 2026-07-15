#!/usr/bin/env python3
"""
One-time setup: authorizes Discord RPC and saves access token.
Run once, then use discord-ptt-guard.py for ongoing monitoring.
"""

import socket
import struct
import json
import os
import sys
import subprocess
import urllib.parse

TOKEN_FILE = os.path.expanduser("~/.discord-ptt-token.json")


def pack(opcode, data):
    payload = json.dumps(data).encode()
    return struct.pack('<II', opcode, len(payload)) + payload


def recv(sock):
    raw = b''
    while len(raw) < 8:
        chunk = sock.recv(8 - len(raw))
        if not chunk:
            raise ConnectionError("Socket closed")
        raw += chunk
    opcode, length = struct.unpack('<II', raw)
    data = b''
    while len(data) < length:
        chunk = sock.recv(length - len(data))
        if not chunk:
            raise ConnectionError("Socket closed")
        data += chunk
    return opcode, json.loads(data.decode())


def find_socket():
    tmpdir = os.environ.get('TMPDIR', '/tmp').rstrip('/')
    for i in range(10):
        path = f'{tmpdir}/discord-ipc-{i}'
        if os.path.exists(path):
            return path
    return None


def main():
    env_file = os.path.join(os.path.dirname(__file__), '.env')
    client_id = None
    client_secret = None

    if os.path.exists(env_file):
        with open(env_file) as f:
            for line in f:
                line = line.strip()
                if line.startswith('DISCORD_CLIENT_ID='):
                    client_id = line.split('=', 1)[1]
                elif line.startswith('DISCORD_CLIENT_SECRET='):
                    client_secret = line.split('=', 1)[1]

    if not client_id:
        client_id = input("Discord App Client ID: ").strip()
    if not client_secret:
        client_secret = input("Discord App Client Secret: ").strip()

    print(f"Using Client ID: {client_id}")

    sock_path = find_socket()
    if not sock_path:
        print("ERROR: Discord IPC socket not found. Make sure Discord is running.")
        sys.exit(1)

    print(f"Connecting to {sock_path}...")
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(sock_path)

    # Handshake
    sock.sendall(pack(0, {"v": 1, "client_id": client_id}))
    _, resp = recv(sock)
    if resp.get('evt') != 'READY':
        print(f"ERROR: Unexpected handshake response: {resp}")
        sys.exit(1)
    print("Handshake OK")

    # Authorize — Discord shows a popup to the user
    print("Requesting authorization... Discord will show a popup, click Authorize.")
    sock.sendall(pack(1, {
        "cmd": "AUTHORIZE",
        "args": {
            "client_id": client_id,
            "scopes": ["rpc", "rpc.voice.read"],
            "prompt": "consent",
        },
        "nonce": "authorize-1",
    }))
    _, resp = recv(sock)

    if resp.get('evt') == 'ERROR':
        print(f"ERROR: {resp['data']['message']}")
        sys.exit(1)

    code = resp['data']['code']
    print(f"Got authorization code.")

    # Exchange code for access token
    print("Exchanging code for access token...")
    body = urllib.parse.urlencode({
        'client_id': client_id,
        'client_secret': client_secret,
        'grant_type': 'authorization_code',
        'code': code,
        'redirect_uri': 'http://localhost',
    }).encode()

    result = subprocess.run([
        'curl', '-s', '-X', 'POST',
        'https://discord.com/api/oauth2/token',
        '-H', 'Content-Type: application/x-www-form-urlencoded',
        '-d', urllib.parse.urlencode({
            'client_id': client_id,
            'client_secret': client_secret,
            'grant_type': 'authorization_code',
            'code': code,
            'redirect_uri': 'http://localhost',
        }),
    ], capture_output=True, text=True)

    token_data = json.loads(result.stdout)
    if 'access_token' not in token_data:
        print(f"ERROR: {token_data}")
        sys.exit(1)

    if 'access_token' not in token_data:
        print(f"ERROR: {token_data}")
        sys.exit(1)

    config = {
        'client_id': client_id,
        'access_token': token_data['access_token'],
        'refresh_token': token_data.get('refresh_token'),
    }
    with open(TOKEN_FILE, 'w') as f:
        json.dump(config, f, indent=2)
    os.chmod(TOKEN_FILE, 0o600)

    print(f"\nSetup complete! Token saved to {TOKEN_FILE}")
    print("Now run: python3 discord-ptt-guard.py")


if __name__ == '__main__':
    main()

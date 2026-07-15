# scripts

macOS helper scripts for the ESP32 foot pedal project.

---

## Discord PTT Guard

Monitors Discord's Push-to-Talk mode and sends a macOS notification when PTT is disabled. Reminds every 15 seconds until PTT is re-enabled.

### How it works

The script connects to Discord's local **IPC socket** (`$TMPDIR/discord-ipc-0`) and subscribes to the `VOICE_SETTINGS_UPDATE` event. Discord fires this event instantly when the user toggles any voice setting — including switching between Push-to-Talk and Voice Activity. There is no polling, no file reading, no CPU load while idle. The script wakes up only when Discord reports a change or when a 15-second reminder timer fires.

This replaces the earlier LevelDB-based approach (`discord-ptt-guard.sh`), which had a 1–5 minute write delay before the new mode was visible on disk.

### Architecture

```
Discord (running) ──IPC──► discord-ptt-guard.py
                               │
                    VOICE_SETTINGS_UPDATE event
                               │
                    mode == VOICE_ACTIVITY?
                        ├── yes → notify via noti (every 15 sec)
                        └── no  → reset, stay silent
```

The daemon runs as a **LaunchAgent** — starts on login, restarts automatically if it crashes.

### Files

| File | Purpose |
|---|---|
| `discord-ptt-setup.py` | One-time OAuth2 authorization. Run once, saves token to `~/.discord-ptt-token.json`. |
| `discord-ptt-guard.py` | Main daemon. Connects to Discord IPC, listens for voice setting changes, sends notifications. |
| `com.user.discord-ptt-guard.plist` | LaunchAgent definition. Installs into `~/Library/LaunchAgents/`. |
| `.env.example` | Template for Discord app credentials. Copy to `.env` and fill in. |
| `.env` | Actual credentials (not committed to git). |
| `discord-ptt-guard.sh` | Legacy polling script (reads LevelDB). Superseded by the Python IPC approach. |

### Dependencies

- [`noti`](https://github.com/variadico/noti) — macOS notification tool:
  ```bash
  brew install noti
  ```

### Setup

#### 1. Allow noti to send notifications

**System Settings → Notifications → noti** → set style to **Alerts**.

#### 2. Register a Discord application (once)

1. Go to [discord.com/developers/applications](https://discord.com/developers/applications) → **New Application** (name it anything, e.g. "PTT Guard")
2. In the left menu → **OAuth2** → under **Redirects** add `http://localhost` → Save Changes
3. Copy **Client ID** and **Client Secret** into `scripts/.env`:
   ```
   DISCORD_CLIENT_ID=your_client_id
   DISCORD_CLIENT_SECRET=your_client_secret
   ```

#### 3. Authorize (once)

Make sure Discord is running, then:

```bash
cp scripts/discord-ptt-setup.py ~/discord-ptt-setup.py
python3 ~/discord-ptt-setup.py
```

Discord will show an **Authorize** popup — click it. The script exchanges the code for an access token and saves it to `~/.discord-ptt-token.json`. You never need to do this again unless the token expires.

#### 4. Install the daemon

```bash
cp scripts/discord-ptt-guard.py ~/discord-ptt-guard.py
cp scripts/com.user.discord-ptt-guard.plist ~/Library/LaunchAgents/
launchctl load ~/Library/LaunchAgents/com.user.discord-ptt-guard.plist
```

The daemon starts immediately and on every login.

### Manual run

```bash
python3 ~/discord-ptt-guard.py
```

### Logs

```bash
tail -f ~/discord-ptt-guard.log
```

### Stop

```bash
launchctl unload ~/Library/LaunchAgents/com.user.discord-ptt-guard.plist
```

### What else the IPC socket can do

The same Discord IPC connection supports write commands, not just reads. Possible extensions:

- `SET_VOICE_SETTINGS` — switch PTT mode on/off programmatically (auto-restore PTT without user interaction)
- Mute / unmute microphone
- Deafen / undeafen
- Switch input/output device
- Adjust volume

This opens the path to having the ESP32 foot pedal control Discord directly via the IPC socket instead of simulating F13 keypresses.

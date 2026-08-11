#!/bin/bash
# Discord PTT Guard Watchdog
# Runs every minute via LaunchAgent StartInterval. If the discord-ptt-guard
# LaunchAgent is not loaded in launchd (crash, missed reload after a reboot,
# legacy `launchctl load` not surviving an OS update, etc.), it re-bootstraps it.

LABEL="com.user.discord-ptt-guard"
PLIST="$HOME/Library/LaunchAgents/${LABEL}.plist"
LOG="$HOME/discord-ptt-guard-watchdog.log"
UID_NUM=$(id -u)

if ! launchctl print "gui/${UID_NUM}/${LABEL}" >/dev/null 2>&1; then
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ${LABEL} not loaded — bootstrapping" >> "$LOG"
    launchctl bootstrap "gui/${UID_NUM}" "$PLIST" >> "$LOG" 2>&1
fi

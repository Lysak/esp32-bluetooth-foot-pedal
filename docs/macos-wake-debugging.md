# macOS Wake Debugging

This note is for future incidents where the Mac wakes unexpectedly and the pedal is only a suspect.

The goal is to separate:

- real Bluetooth HID wake
- USB wake
- power-button wake
- scheduled maintenance wake
- unrelated dark wakes that never became a full user-visible wake

## Fast Reality Check

Run these first:

```bash
pmset -g log | rg "Wake|DarkWake|Sleep|Wake reason|HID Activity|Bluetooth LE HID Activity|pwrbtn|USB Device|wifibt" | tail -n 200
pmset -g assertions
pmset -g sched
system_profiler SPBluetoothDataType
```

What they tell you:

- `pmset -g log`: sleep and wake history with the reason string.
- `pmset -g assertions`: what currently keeps the machine awake or marks recent user activity.
- `pmset -g sched`: scheduled future wakes from macOS services.
- `system_profiler SPBluetoothDataType`: whether the pedal is currently connected and how macOS classifies it.

## How To Read The Important Lines

### Bluetooth HID wake

Typical pattern:

```text
DarkWake ... due to ... wifibt ... bluetooth-pcie
bluetoothd Created UserIsActive "Bluetooth LE HID Activity"
Wake ... due to HID Activity
```

This is the strongest sign that a Bluetooth keyboard-like device caused the wake.

### Power-button wake

Typical pattern:

```text
Wake ... due to ... pwrbtn ... UserActivity Assertion
DriverReason:pwrbtn
```

That is not the pedal. It is a power-button style wake path as seen by macOS.

### USB wake or USB user activity

Typical pattern:

```text
WindowServer ... AppleUserHIDEventService product:USB Device eventType:17
Kernel Assertions: 0x4=USB
```

That points to a USB-originated HID path, not Bluetooth LE HID.

### Scheduled maintenance wake

Typical pattern:

```text
DarkWake ... rtc/Maintenance
pmset -g sched
```

That is macOS maintenance, analytics, calendar, or similar scheduled work.

### DarkWake only

If you see only `DarkWake` and no later `Wake ... to FullWake`, the machine may have woken briefly for background work but not fully for user activity.

## Commands For A Specific Time Window

If you know the approximate time, narrow the search:

```bash
pmset -g log | rg "2026-07-03 18:22|2026-07-03 19:07|2026-07-04 09:10|2026-07-04 13:28"
```

Or use the unified log:

```bash
/usr/bin/log show --style compact --last 24h --predicate 'process == "powerd" OR process == "bluetoothd" OR eventMessage CONTAINS[c] "Wake" OR eventMessage CONTAINS[c] "UserIsActive"'
```

`/usr/bin/log` is used explicitly because `zsh` also has a built-in `log` command.

## What Was Observed On This Mac

On this machine, recent history already showed multiple distinct wake classes:

- full wakes with `bluetoothd Created UserIsActive "Bluetooth LE HID Activity"` followed by `Wake ... due to HID Activity`
- full wakes with `DriverReason:pwrbtn`
- `WindowServer` user activity tagged as `AppleUserHIDEventService product:USB Device eventType:17`
- scheduled wakes visible in `pmset -g sched`

That means "the pedal did it" must be proven from logs for the exact timestamp, not assumed.

## Practical Conclusion

If a real Bluetooth keyboard is paired to a Mac, then yes, it can normally wake the Mac. A BLE pedal that advertises itself as a keyboard is in the same class from macOS's point of view.

Current firmware direction in this repo:

- do not auto-advertise on boot;
- do not auto-advertise after disconnect;
- only a physical pedal press may expose the keyboard again.

That policy reduces host-side wake opportunities, but any moment when the pedal is actively advertising or connected as a keyboard is still a period where macOS may treat it as wake-capable HID.

The important distinction is not "keyboard vs pedal" but:

- Bluetooth HID activity
- USB HID activity
- power button
- scheduled maintenance

## Recommended Incident Workflow

1. Record the exact local time when the unexpected wake happened.
2. Run the four fast commands from this document.
3. Save the matching `pmset -g log` lines around that timestamp.
4. Check whether the pedal is shown as connected in `system_profiler SPBluetoothDataType`.
5. Only then decide whether to change firmware, Bluetooth pairing, or macOS settings.

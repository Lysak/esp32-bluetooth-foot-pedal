# esp32-bluetooth-foot-pedal

ESP32 foot pedal for macOS that behaves as a Bluetooth keyboard and exposes status, diagnostics, and counters to Home Assistant through ESPHome.

## Components

| Component | AliExpress |
|---|---|
| ESP-32S / ESP-WROOM-32 development board, 30P/38P | 📱 https://s.click.aliexpress.com/e/_c3SP1ult<br>🖥 https://s.click.aliexpress.com/e/_c30P7tWr |
| ESP32-S3 development board | 📱 https://s.click.aliexpress.com/e/_c4o9jJr1<br>🖥 https://s.click.aliexpress.com/e/_c4MoKrXD |
| Momentary Foot Switch, SPDT, 1NO+1NC | 📱&nbsp;https://s.click.aliexpress.com/e/_c3Symxzd<br>🖥&nbsp;https://s.click.aliexpress.com/e/_c3CmBoX9 |

This repository pins ESPHome in `.esphome-version` and Python in `.esphome-python-version` to stay compatible with Home Assistant `2024.3.3` on a 1 GB Orange Pi host. Do not run ad-hoc `esphome` or `uvx esphome ...` commands from this repo; use the `make` targets so the pinned toolchain is always applied.

Version 1 scope:

- one pedal on `GPIO27`
- one BLE keyboard binding: `F13`
- local press/release handling on the ESP32
- Wi-Fi, ESPHome API, OTA, and Home Assistant visibility

Current implementation:

- real BLE HID keyboard transport is handled by the local vendored `espidf_ble_keyboard` component
- the local `esp32_bluetooth_foot_pedal_ble_hid` component owns pedal-state statistics, reconnect-sensitive hold behavior, and the Home Assistant-facing counters
- Home Assistant is observational only; it does not relay the key press to the Mac

## Behavior

```text
pedal pressed while connected    -> hold F13
pedal pressed while disconnected -> start BLE advertising, reconnect, then hold F13 if still pressed
pedal released -> release all keys
```

The firmware no longer auto-advertises on boot or after disconnect. A real physical pedal press is the only event that makes the BLE keyboard connectable again.

## macOS note

WARNING: if you bind Push to Talk in Discord to `F13`, macOS can play its system `alert sound` when Discord is not focused and the key press is sent to the background app.

To disable that sound on macOS:

1. Open `System Settings` -> `Sound`.
2. In `Sound Effects`, set `Alert volume` to `0`.
3. Turn off `Play user interface sound effects`.

## Hardware

- ESP-32S / ESP-WROOM-32 dev board
- momentary SPDT foot pedal, using `COM + NO`
- Mac as the BLE keyboard target
- 5 V power supply, optionally through the DIY UPS below

## Uninterruptible power (optional)

A DIY UPS keeps the pedal connected to the Mac during a mains outage.

Parts: TP4056 Type-C charger with protection (`IN`/`B`/`OUT` pads), MT3608 boost, YX850 auto-switch, one 18650 cell, 470–1000 µF capacitor rated 10 V or higher.

```text
                ┌──────────────────────────────────────────► YX850 IN  (main)
5 V PSU (+/−) ──┤
                └─► TP4056 IN+/IN−
                    TP4056 B+/B−     ◄──► 18650
                    TP4056 OUT+/OUT− ──► MT3608 VIN+/VIN−
                                         MT3608 VOUT+/VOUT− ──► YX850 BAT (backup)
                                                                YX850 OUT ──► capacitor ──► ESP32 5V/VIN + GND
```

All grounds are common. YX850 pad labels differ between lots; check yours.

Build:

1. Charge the 18650 to 4.2 V with the TP4056 before wiring the rest.
2. Feed the MT3608 from TP4056 `OUT+/OUT−`, not `B+/B−`, so the over-discharge protection stays in the path.
3. Set the MT3608 with no load attached. It often ships at 12 V or more, which kills the ESP32. Target about 0.2 V below the PSU (e.g. 4.9 V for a 5.1 V PSU) so the YX850 never drains the cell while mains is present.
4. Solder the capacitor on YX850 `OUT`, observing polarity (stripe = minus). It rides through the switchover gap; without it the ESP32 may reboot and drop BLE.
5. Use 1 mm² (≈18 AWG) or thicker copper wire for power lines; 30 AWG is only fine for signals. Never solder directly to the cell: use holder tabs or nickel strip, then heat-shrink every joint.

Check before connecting the pedal: YX850 `OUT` must stay at 4.5 V or more both with the PSU on and unplugged, because the ESP32 regulator needs about 4.4 V. Raise the MT3608 slightly if it reads lower.

Test with the pedal: unplug the PSU, then plug it back in. The pedal must stay connected to the Mac and `uptime` in Home Assistant must not reset. After a few hours on mains the cell should still read about 4.2 V; if it drops, lower the MT3608 by 0.1–0.2 V. If the ESP32 still reboots on switchover, add a second 1000 µF capacitor in parallel.

Runtime: the pedal draws about 0.6 W (estimated, not measured), so a ~3000 mAh 18650 lasts roughly 12 h. Disconnect the UPS before flashing over USB; OTA is fine.

## Firmware layout

```text
esphome/      ESPHome entry file, packages, board config, and local secrets file
components/   local external components
docs/         focused operator and architecture notes
```

## Scripts

- [scripts/README.md](scripts/README.md): macOS helper scripts, including a Discord Push to Talk watchdog.

## Markdown files

- [idea.md](idea.md): product direction, MVP, hardware baseline, and persistence policy.
- [implementation.md](implementation.md): architecture and implementation order from repository bootstrap to reliability hardening.
- [esp32-bluetooth-foot-pedal-project-brief.md](esp32-bluetooth-foot-pedal-project-brief.md): original working brief used to shape the repository.
- [docs/architecture.md](docs/architecture.md): short runtime split between BLE HID and Home Assistant telemetry.
- [docs/flashing.md](docs/flashing.md): how to prepare secrets, compile, flash, and monitor the board.
- [docs/home-assistant.md](docs/home-assistant.md): expected Home Assistant entities and integration role.
- [docs/macos-pairing.md](docs/macos-pairing.md): pairing and press-only reconnect expectations for macOS.
- [docs/macos-wake-debugging.md](docs/macos-wake-debugging.md): how to inspect macOS wake reasons and tell Bluetooth HID wakes from unrelated ones.
- [docs/testing.md](docs/testing.md): bench-test flow with and without the physical pedal.
- [docs/wiring.md](docs/wiring.md): first-version wiring for `GPIO27`.
- [components/esp32_bluetooth_foot_pedal_ble_hid/README.md](components/esp32_bluetooth_foot_pedal_ble_hid/README.md): purpose and limits of the local statistics component.

## Setup

1. Copy `esphome/secrets.example.yaml` to `esphome/secrets.yaml`.
2. Fill in Wi-Fi, API, and OTA secrets.
3. Use `make esphome-version` and `make esphome-python-version` if you want to confirm the pinned toolchain.
4. Build or flash with `make`.

## Commands

```bash
make help
make esphome-version
make esphome-python-version
make esphome-compile
make esphome-flash DEVICE=/dev/tty.usbserial-XXXX
make gen-api-key
make clean
```

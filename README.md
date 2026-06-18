# esp32-bluetooth-foot-pedal

ESP32 foot pedal for macOS that behaves as a Bluetooth keyboard and exposes status, diagnostics, and counters to Home Assistant through ESPHome.

Version 1 scope:

- one pedal on `GPIO27`
- one BLE keyboard binding: `F13`
- local press/release handling on the ESP32
- Wi-Fi, ESPHome API, OTA, and Home Assistant visibility

Current implementation:

- real BLE HID keyboard transport is handled by the local vendored `espidf_ble_keyboard` component
- the local `esp32_bluetooth_foot_pedal_ble_hid` component owns pedal-state statistics and the Home Assistant-facing counters
- Home Assistant is observational only; it does not relay the key press to the Mac

## Behavior

```text
pedal pressed  -> hold F13
pedal released -> release all keys
```

## Hardware

- ESP-32S / ESP-WROOM-32 dev board
- momentary SPDT foot pedal, using `COM + NO`
- Mac as the BLE keyboard target
- USB 5 V power

## Firmware layout

```text
esphome/      ESPHome entry file, packages, board config, and local secrets file
components/   local external components
docs/         focused operator and architecture notes
```

## Markdown files

- [idea.md](idea.md): product direction, MVP, hardware baseline, and persistence policy.
- [implementation.md](implementation.md): architecture and implementation order from repository bootstrap to reliability hardening.
- [esp32-bluetooth-foot-pedal-project-brief.md](esp32-bluetooth-foot-pedal-project-brief.md): original working brief used to shape the repository.
- [docs/architecture.md](docs/architecture.md): short runtime split between BLE HID and Home Assistant telemetry.
- [docs/flashing.md](docs/flashing.md): how to prepare secrets, compile, flash, and monitor the board.
- [docs/home-assistant.md](docs/home-assistant.md): expected Home Assistant entities and integration role.
- [docs/macos-pairing.md](docs/macos-pairing.md): pairing and reconnect expectations for macOS.
- [docs/testing.md](docs/testing.md): bench-test flow with and without the physical pedal.
- [docs/wiring.md](docs/wiring.md): first-version wiring for `GPIO27`.
- [components/esp32_bluetooth_foot_pedal_ble_hid/README.md](components/esp32_bluetooth_foot_pedal_ble_hid/README.md): purpose and limits of the local statistics component.

## Setup

1. Copy `esphome/secrets.example.yaml` to `esphome/secrets.yaml`.
2. Fill in Wi-Fi, API, and OTA secrets.
3. Build or flash with `make`.

## Commands

```bash
make help
make esphome-compile
make esphome-flash DEVICE=/dev/tty.usbserial-XXXX
make gen-api-key
make clean
```

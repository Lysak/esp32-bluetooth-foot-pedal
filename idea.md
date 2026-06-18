# esp32-bluetooth-foot-pedal

## Project name

**Repository name:** `esp32-bluetooth-foot-pedal`

**Device / project name:** ESP32 Bluetooth Foot Pedal

**Short description:**

DIY foot pedal based on ESP32 that works as a Bluetooth keyboard for macOS and is also visible in Home Assistant through ESPHome.

---

## Core idea

The pedal is a one-button Bluetooth keyboard for Mac.

Base behavior:

```text
pedal pressed  -> send and hold F13
pedal released -> release F13
```

Primary use case:

- push-to-talk
- push-to-dictation
- mute / unmute shortcut binding through a Mac app

Version 1 is intentionally narrow:

- one physical pedal
- one default key: `F13`
- Bluetooth keyboard behavior for macOS
- Home Assistant visibility through ESPHome
- basic counters and diagnostics

Version 1 does not try to implement:

- many shortcut profiles
- complex combinations editor
- battery mode
- deep sleep
- multiple pedals
- web UI

---

## Product direction

The device must behave as an independent local input device.

Required architecture:

```text
Mechanical pedal
      |
      v
ESP32 GPIO input
      |
      +---------------------> Local BLE HID keyboard -> macOS
      |
      +---------------------> ESPHome Native API over Wi-Fi -> Home Assistant
```

Rules:

- the BLE keyboard path is the primary path
- the Home Assistant path is secondary
- pedal-to-keyboard action must never depend on Home Assistant automation
- the pedal must still work when Wi-Fi is down
- the pedal must still work when Home Assistant is offline

---

## MVP scope

The first real working version must include:

1. ESP32 boots reliably.
2. It advertises as a Bluetooth keyboard.
3. It pairs with macOS.
4. Pressing the pedal sends `F13` key-down.
5. Releasing the pedal sends key-up / release-all.
6. Home Assistant sees the device through ESPHome.
7. Home Assistant shows pedal state, BLE state, uptime, Wi-Fi signal, and counters.
8. Statistics survive reboot using safe low-frequency persistence.

---

## Hardware baseline

| Category | Component | Status | Notes |
|---|---|---|---|
| Controller | ESP-32S / ESP-WROOM-32 development board, 30P/38P | Available | Main controller |
| Pedal switch | Momentary foot pedal switch, SPDT, `1 NO + 1 NC` | Target hardware | Use low-voltage dry contacts only |
| Host | MacBook / macOS | Available | Primary BLE HID target |
| Power | USB 5 V | Available | Permanent power for version 1 |

Initial wiring:

```text
Pedal NO contact -> ESP32 GPIO
Pedal COM        -> GND
```

Electrical logic:

```text
Released -> HIGH
Pressed  -> LOW
```

Firmware assumptions:

- `INPUT_PULLUP`
- inverted logic
- `30 ms` debounce
- one safe GPIO chosen in firmware configuration

Until the physical pedal arrives, pedal presses may be simulated by briefly connecting the chosen GPIO to GND with a jumper wire.

---

## Home Assistant role

Home Assistant is for:

- visibility
- telemetry
- diagnostics
- OTA support
- optional future configuration

Home Assistant is not the event relay for keyboard actions.

Required entities for version 1:

- pedal state
- BLE connected state
- total presses since boot
- persistent total presses
- last hold duration
- total held time since boot
- current profile or current binding
- uptime
- Wi-Fi RSSI
- restart action
- reset statistics action

---

## Data ownership and persistence

ESP32 is the source of truth.

Store in RAM during runtime:

- current pedal state
- current BLE state
- session press count
- last hold duration
- session held time
- dirty counters waiting to be persisted

Persist only when needed:

- lifetime press count
- lifetime held time
- selected binding / profile

Recommended persistence strategy:

- save every `10 minutes` or every `100` new presses, whichever happens first
- optionally save on a clean restart
- never save on every press

This keeps flash wear low enough for long service life while keeping recent-data loss acceptable after sudden power removal.

---

## Safety requirements

The firmware must avoid stuck keys.

Required rules:

- send key-down only on debounced press transition
- send key-up or release-all only on debounced release transition
- call `releaseAll()` on release
- call `releaseAll()` on disconnect if possible
- call `releaseAll()` after reconnect before accepting a new press
- if reconnect happens while the pedal is physically held, require release and fresh press before sending a new key-down

---

## macOS target behavior

macOS is the primary target for version 1.

The device must be testable for:

- initial pairing
- reconnect after ESP32 reboot
- reconnect after Mac sleep/wake
- key-down / key-up correctness
- long hold behavior
- repeated presses
- disconnect safety

Version 1 should not claim support for other operating systems until tested.

---

## Inspiration boundary

This project may use existing open-source pedal projects as inspiration for:

- one-key keyboard semantics
- hold / release behavior
- reproducible documentation
- simple hardware design

But this project should keep its own identity:

- name: `esp32-bluetooth-foot-pedal`
- ESP32 instead of RP2040
- BLE HID instead of USB HID
- ESPHome + Home Assistant integration
- no copied branding or naming


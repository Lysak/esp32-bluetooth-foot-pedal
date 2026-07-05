# Implementation Plan

## Goal

Build a production-quality first version of `esp32-bluetooth-foot-pedal` that works as:

- a Bluetooth keyboard for macOS
- an ESPHome device visible in Home Assistant
- a stable single-pedal `F13` controller for push-to-talk style workflows

The implementation must keep the BLE HID path independent from Wi-Fi and Home Assistant.

---

## Final version 1 scope

Version 1 includes:

1. One ESP32 board.
2. One pedal input.
3. One default binding: `F13`.
4. BLE keyboard pairing on macOS plus press-only reconnect after disconnect.
5. ESPHome Wi-Fi, API, OTA, and diagnostics.
6. Home Assistant entities for status and counters.
7. Low-wear persistence for lifetime statistics.
8. Documentation for flashing, pairing, wiring, and testing.

Version 1 excludes:

1. Multi-profile shortcut editor.
2. Complex modifier combinations UI.
3. Battery mode.
4. Timer-driven sleep mode.
5. Multiple pedals.
6. Web server or captive configuration UI beyond standard ESPHome fallback behavior.

---

## Architecture

### Runtime split

```text
Pedal GPIO -> debounce -> BLE HID state machine -> macOS
                     \
                      -> telemetry publication -> ESPHome / Home Assistant
```

The left side is latency-sensitive.
The right side is observational.

### Ownership

- one local component owns BLE HID state
- one input path owns pedal transitions
- telemetry is published from state changes, not by polling loops
- persistence is separate from key-send logic

---

## Repository layout

Use this structure:

```text
esp32-bluetooth-foot-pedal/
├── README.md
├── idea.md
├── implementation.md
├── esp32-bluetooth-foot-pedal-project-brief.md
├── .gitignore
├── esphome/
│   ├── esp32-bluetooth-foot-pedal.yaml
│   ├── boards/
│   │   └── esp32-wroom-32.yaml
│   ├── common/
│   │   ├── base.yaml
│   │   ├── diagnostics.yaml
│   │   ├── pedal.yaml
│   │   └── statistics.yaml
│   ├── secrets.example.yaml
│   └── secrets.yaml         # local only, ignored by git
├── components/
│   └── esp32_bluetooth_foot_pedal_ble_hid/
│       ├── __init__.py
│       ├── esp32_bluetooth_foot_pedal_ble_hid.h
│       ├── esp32_bluetooth_foot_pedal_ble_hid.cpp
│       └── README.md
├── docs/
│   ├── architecture.md
│   ├── wiring.md
│   ├── flashing.md
│   ├── macos-pairing.md
│   ├── home-assistant.md
│   └── testing.md
└── Makefile
```

Reference source:

- follow the neighbor repo structure from `esp32-ambient-matrix-lamp` where it helps
- reuse the simple split of `esphome/boards`, `esphome/common`, and project docs
- do not copy lamp-specific code or naming

---

## Build order

Implement in this order.

### Phase 0: Repository skeleton

Create:

- `idea.md`
- `implementation.md`
- `.gitignore`
- `esphome/` tree
- `components/` tree
- `docs/` placeholders
- `Makefile`

Success criteria:

- repository shape is stable
- secrets and build artifacts are ignored
- a committed secrets template exists for new installs
- the firmware entry point path is fixed early

### Phase 1: BLE HID proof of life inside the real target architecture

Build the smallest working firmware that already includes ESPHome and Home Assistant support, but proves the local keyboard path first.

Implement:

- ESPHome base config
- ESP-IDF framework selection
- custom BLE HID component scaffold
- pedal GPIO input
- debounce handling
- `F13` key-down on press
- `releaseAll()` or key-up on release
- BLE connection state

Success criteria:

- firmware compiles
- ESP32 advertises as a Bluetooth keyboard
- macOS can pair
- test jumper on GPIO can produce press/release

### Phase 2: ESPHome and Home Assistant integration

Add:

- Wi-Fi
- Native API
- OTA
- logger
- uptime sensor
- Wi-Fi RSSI sensor
- pedal binary sensor
- BLE connected binary sensor

Success criteria:

- Home Assistant sees the device
- HA entity updates do not block key behavior
- BLE behavior still works with Wi-Fi enabled

### Phase 3: Statistics and persistence

Add:

- presses since boot
- lifetime persistent press count
- last hold duration
- total held time since boot
- low-wear save scheduler
- reset statistics action

Default persistence policy:

- save every `10 minutes` or `100 presses`

Success criteria:

- stats update live
- persistent counters survive reboot
- no write-per-press behavior exists

### Phase 4: Reliability hardening

Add:

- press-only reconnect policy
- disconnect safety
- held-during-reconnect safety
- startup diagnostics
- heap and failure logging where useful

Success criteria:

- no logically stuck key after disconnect tests
- the pedal does not auto-readvertise after disconnect
- reconnect can start only from a fresh physical press
- reboot does not leave Mac with held key state

### Phase 5: Documentation and operator workflow

Add:

- wiring guide
- flashing guide
- macOS pairing guide
- Home Assistant setup guide
- hardware test checklist
- troubleshooting notes
- Makefile shortcuts

Success criteria:

- a clean machine can follow the docs
- first flash, pair, and HA onboarding steps are reproducible

---

## BLE HID implementation strategy

Use ESPHome with `ESP-IDF` and a repository-owned local component.

Planned sequence:

1. Inspect existing ESPHome BLE keyboard component references from the brief.
2. Decide whether one can be safely adapted into a local component.
3. If not, build a minimal project-specific component for:
   - advertise
   - connect
   - send key down
   - send key up
   - release all
   - report connection state

Why this is the right path:

- ESPHome gives Home Assistant, OTA, config structure, and diagnostics
- the local component keeps BLE logic under project control
- a local component avoids fragile runtime dependence on a moving remote branch

---

## ESPHome configuration strategy

Follow the layout style already used in `esp32-ambient-matrix-lamp`:

- board-specific YAML in `esphome/boards/`
- reusable config chunks in `esphome/common/`
- one top-level project entry file

Planned config split:

- `boards/esp32-wroom-32.yaml`
  - board and framework
- `common/base.yaml`
  - project name, Wi-Fi, API, OTA, logger, packages
- `common/diagnostics.yaml`
  - uptime, RSSI, restart, maybe heap
- `common/pedal.yaml`
  - GPIO input, debounce config, pedal state publishing
- `common/statistics.yaml`
  - counters, numbers, reset actions, persistence config

This keeps the final firmware understandable and easy to extend later.

---

## GPIO plan

Default hardware assumptions for implementation:

- board: ESP-32S / ESP-WROOM-32 development board
- default pin: `GPIO27`
- input mode: `INPUT_PULLUP`
- logic: inverted
- debounce: `30 ms`

Reason for `GPIO27`:

- commonly safe and flexible on classic ESP32 dev boards
- not one of the usual first pins avoided for boot strapping concerns
- already a familiar pattern from the neighbor ESP32 project

If board-specific testing shows a better pin, change it in one configuration location only.

---

## Data and persistence plan

### RAM data

Maintain in RAM:

- current pedal state
- current BLE state
- current hold start timestamp
- session press count
- session held duration
- dirty increments since last save

### Persistent data

Persist:

- lifetime press count
- lifetime held duration
- selected binding if stored locally

### Save trigger

Save when:

- `10 minutes` elapsed since the previous save
- or `100` new presses accumulated
- or an explicit clean restart is requested

This policy is chosen to reduce flash wear while keeping data loss after power loss acceptable.

---

## Test plan before the physical pedal arrives

The project can start immediately.

Bench testing method:

- flash the ESP32 by USB
- pair it with macOS
- connect `GPIO27` to `GND` manually with a jumper wire
- verify key-down on contact
- verify key-up on release
- verify Home Assistant state changes

This is enough to validate:

- BLE keyboard enumeration
- macOS pairing
- press/release logic
- entity publication
- basic persistence behavior

Not yet validated without the real pedal:

- real contact bounce characteristics
- enclosure wiring noise
- long-term pedal hardware reliability

---

## Immediate next implementation tasks

After this document is accepted, start here:

1. Create repository skeleton and ignore rules.
2. Add ESPHome base YAML files.
3. Add local BLE HID component scaffold.
4. Add Makefile commands for compile and flash.
5. Compile the first firmware.
6. Flash to ESP32 and test pairing on macOS.
7. Add Home Assistant entities.

This is the shortest path to a real working device.

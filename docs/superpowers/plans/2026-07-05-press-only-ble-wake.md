# Press-Only BLE Wake Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the pedal advertise and reconnect only after a physical press, never automatically on boot or disconnect.

**Architecture:** Add explicit advertising controls to the vendored BLE keyboard transport, then let the pedal-state component own the press-to-advertise and held-during-reconnect behavior. Keep GPIO debounce and key semantics intact while removing background BLE reappearance.

**Tech Stack:** ESPHome YAML, custom ESPHome C++ components, ESP-IDF BLE GATT/GAP, OTA upload via `uvx esphome`

---

### Task 1: Add BLE advertising controls to the transport component

**Files:**
- Modify: `components/espidf_ble_keyboard/espidf_ble_keyboard.h`
- Modify: `components/espidf_ble_keyboard/espidf_ble_keyboard.cpp`
- Modify: `components/espidf_ble_keyboard/__init__.py`

- [ ] Add YAML-exposed flags for `auto_advertise_on_boot` and `auto_advertise_on_disconnect`, plus public methods to start and stop advertising manually.
- [ ] Gate the current boot-time and disconnect-time `do_start_advertising()` calls behind those flags.
- [ ] Queue a manual advertising request if a press arrives before all GATT services are ready.

### Task 2: Move reconnect ownership into the pedal-state component

**Files:**
- Modify: `components/esp32_bluetooth_foot_pedal_ble_hid/esp32_bluetooth_foot_pedal_ble_hid.h`
- Modify: `components/esp32_bluetooth_foot_pedal_ble_hid/esp32_bluetooth_foot_pedal_ble_hid.cpp`
- Modify: `components/esp32_bluetooth_foot_pedal_ble_hid/__init__.py`

- [ ] Inject the BLE keyboard component into the pedal-state component.
- [ ] On pedal press: if connected, send key-down immediately; if disconnected, start advertising and mark a pending held press.
- [ ] In `loop()`, detect BLE connection transitions and send key-down after reconnect only when the pedal is still physically held.
- [ ] On pedal release: release keys if connected, clear pending reconnect state, and stop advertising if reconnect has not completed yet.

### Task 3: Simplify the YAML pedal flow

**Files:**
- Modify: `esphome/common/pedal.yaml`

- [ ] Disable `auto_advertise_on_boot` and `auto_advertise_on_disconnect` for this firmware.
- [ ] Remove direct `press_key()` and `release_all_keys()` calls from the YAML lambda so the pedal-state component becomes the single owner of reconnect-sensitive HID behavior.

### Task 4: Update canonical docs and design docs

**Files:**
- Modify: `README.md`
- Modify: `docs/architecture.md`
- Modify: `docs/testing.md`
- Modify: `docs/macos-pairing.md`
- Modify: `docs/macos-wake-debugging.md`
- Modify: `idea.md`
- Modify: `implementation.md`
- Modify: `esp32-bluetooth-foot-pedal-project-brief.md`

- [ ] Replace the always-available BLE wording with press-only advertising behavior.
- [ ] Document that a first press while disconnected may spend a short time reconnecting before the held key takes effect.
- [ ] Remove or rewrite statements that forbid first-press wake or that require persistent BLE availability.

### Task 5: Build and flash verification

**Files:**
- Modify: `esphome/.device_ip` only if a refreshed live IP is discovered and persisted by repo workflow

- [ ] Run `make clean`.
- [ ] Run `make esphome-compile`.
- [ ] Verify the live device target before upload.
- [ ] Run `make esphome-flash DEVICE=<resolved-ip-or-mdns-name>` if the device is reachable.
- [ ] Record any limits if OTA flashing is blocked by reachability or authentication.

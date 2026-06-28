# Held Time Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Persist pedal hold history hourly in NVS and expose current/previous month held-time totals in Home Assistant using the `Europe/Kyiv` calendar.

**Architecture:** Extend the custom foot-pedal component with an NVS-backed paged event journal plus durable monthly aggregates. Inject the Home Assistant time source into the component so release events can be timestamped and month totals can be computed in local time while preserving a low write rate.

**Tech Stack:** ESPHome YAML, custom ESPHome Python codegen, ESP32 C++, ESP-IDF NVS

---

### Task 1: Wire a time source into the custom component

**Files:**
- Modify: `components/esp32_bluetooth_foot_pedal_ble_hid/__init__.py`
- Modify: `components/esp32_bluetooth_foot_pedal_ble_hid/esp32_bluetooth_foot_pedal_ble_hid.h`
- Modify: `esphome/common/base.yaml`
- Modify: `esphome/common/pedal.yaml`

- [ ] Add a `time_id` config option and pass the Home Assistant clock pointer into the component.
- [ ] Store the time source pointer in the component and use it to validate whether calendar timestamps are available.

### Task 2: Add durable event history and monthly aggregates

**Files:**
- Modify: `components/esp32_bluetooth_foot_pedal_ble_hid/esp32_bluetooth_foot_pedal_ble_hid.h`
- Modify: `components/esp32_bluetooth_foot_pedal_ble_hid/esp32_bluetooth_foot_pedal_ble_hid.cpp`

- [ ] Add NVS metadata, paged event storage, and an hourly flush policy.
- [ ] Load existing metadata/pages on boot and recover aggregate counters.
- [ ] On release, buffer an event with start/end epoch plus hold duration and update current/previous month totals in local time.

### Task 3: Expose new Home Assistant entities

**Files:**
- Modify: `esphome/common/statistics.yaml`

- [ ] Add template sensors/text sensors for current month held time, previous month held time, last persisted event timestamp, and total persisted event count.

### Task 4: Verify build health

**Files:**
- Modify: `components/esp32_bluetooth_foot_pedal_ble_hid/*`
- Modify: `esphome/common/*.yaml`

- [ ] Run `make esphome-compile` and confirm the firmware still compiles cleanly with the new persistence logic.

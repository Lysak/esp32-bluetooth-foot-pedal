# esp32-bluetooth-foot-pedal — Codex Project Brief

## Role and task

Act as a senior embedded systems engineer experienced with ESP32, ESP-IDF, ESPHome external components, Bluetooth Low Energy HID, macOS compatibility, Home Assistant integration, testing, documentation, and open-source licensing.

Create a production-oriented open-source GitHub project for a single-button Bluetooth foot pedal based on ESP32. The project name and repository name are:

```text
esp32-bluetooth-foot-pedal
```

The device must work primarily as an independent Bluetooth HID keyboard for macOS. ESPHome and Home Assistant are secondary, optional capabilities for telemetry, diagnostics, configuration, and OTA updates. The pedal must continue to perform its core keyboard function when Home Assistant, Wi-Fi, the ESPHome API, or the Home Assistant server is unavailable.

Do not treat Home Assistant as an event relay between the pedal and the Mac.

---

# 1. Product concept

Build a low-cost foot pedal using:

- a cheap normally-open or changeover mechanical foot switch from AliExpress;
- an ESP32 board, initially targeting a classic ESP32-WROOM-32 development board;
- USB power, with no battery requirement in the first version;
- Bluetooth Low Energy HID keyboard connectivity to a MacBook;
- ESPHome integration with Home Assistant as an additional independent subsystem.

The primary use case is push-to-talk or push-to-dictate:

```text
Pedal pressed  -> send and hold a configured keyboard key or shortcut
Pedal released -> release the configured key or shortcut
```

Examples:

- Discord Push-to-Talk;
- Discord mute toggle as an optional profile;
- macOS dictation shortcut;
- Superwhisper shortcut;
- Handy shortcut;
- arbitrary configurable keyboard combinations.

The first version may default to an otherwise unused function key such as `F13`, allowing the user to bind it in macOS or Discord.

---

# 2. Inspiration and lessons from Footy

Use the open-source **Footy Foot Pedal** project by CJ Pais as design inspiration, but do not copy its name, branding, mechanical design, or implementation blindly.

Reference:

- https://workshop.cjpais.com/projects/footy

Footy is effectively a one-key keyboard operated by foot. Its useful design principles that should be carried into esp32-bluetooth-foot-pedal are:

1. **Treat the pedal as a real keyboard key, not merely as a remote button.**
2. **Support key-down while the pedal is held and key-up when released.**
3. **Make the device easy for another person to reproduce.**
4. **Provide clear firmware, wiring, flashing, and build documentation.**
5. **Allow the key binding or shortcut to be changed.**
6. **Use debouncing appropriate for a mechanical switch.**
7. **Ensure all pressed keys are released safely after errors, reconnects, mode changes, or reboot.**
8. **Keep the interaction immediate and predictable.**
9. **Design the project as an accessibility and productivity tool, not only as a novelty.**

Footy uses RP2040, USB-C, QMK, VIA, a Cherry MX switch, and a 3D-printed enclosure. esp32-bluetooth-foot-pedal intentionally differs:

- ready-made low-cost industrial-style foot switch instead of a custom printed enclosure;
- ESP32 instead of RP2040;
- BLE HID instead of USB HID as the primary transport;
- ESPHome/Home Assistant telemetry;
- no QMK dependency;
- no VIA dependency;
- optional profiles and diagnostics tailored to macOS and Home Assistant.

esp32-bluetooth-foot-pedal must be an original implementation and documentation set.

---

# 3. Non-negotiable architecture

Use this architecture:

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

The two branches must be operationally independent.

## Core path

```text
Pedal -> GPIO -> debounce -> BLE HID press/release -> MacBook
```

This path must:

- run locally on the ESP32;
- never wait for Home Assistant;
- never require Wi-Fi;
- never call a Home Assistant automation to produce the keyboard action;
- remain usable when Home Assistant is offline;
- remain usable when Wi-Fi is unavailable;
- minimize latency;
- preserve correct press-and-hold semantics.

## Optional Home Assistant path

```text
ESP32 -> Wi-Fi -> ESPHome Native API -> Home Assistant
```

This path is for:

- status;
- telemetry;
- diagnostics;
- optional profile selection;
- optional configuration;
- OTA updates;
- counters and historical analysis.

Failures in this path must not block or delay the BLE HID path.

---

# 4. ESPHome strategy

ESPHome does not provide a standard built-in BLE HID keyboard component suitable for this requirement. Implement the project using:

- ESPHome with the ESP-IDF framework;
- an external BLE HID keyboard component;
- preferably a project-owned external component under the repository rather than a fragile remote dependency;
- clear abstraction between ESPHome YAML and the BLE HID implementation.

Relevant references:

- ESPHome external components: https://esphome.io/components/external_components/
- ESPHome Native API: https://esphome.io/components/api/
- ESP32 Wi-Fi/Bluetooth coexistence: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/coexist.html
- Candidate implementation reference: https://github.com/markusg1234/ESPHome-espidf_ble_keyboard

First inspect the candidate external component and its license, API, maintenance status, ESPHome-version compatibility, ESP-IDF compatibility, BLE bonding behavior, and macOS behavior. Do not import it blindly.

Prefer one of these approaches, in order:

1. Adapt a compatible GPL BLE HID implementation into a repository-owned ESPHome external component.
2. Implement a minimal project-specific ESPHome external component using ESP-IDF BLE HID APIs.
3. Use a remote external component only temporarily for a proof of concept, with the dependency pinned to an immutable commit.

Do not depend on a moving `main` or `master` branch in production configuration.

---

# 5. Required BLE HID behavior

The Mac must see esp32-bluetooth-foot-pedal as a Bluetooth keyboard.

Implement:

- BLE advertising;
- initial pairing;
- bonding where supported;
- automatic reconnection;
- connection state reporting;
- configurable BLE device name, default `esp32-bluetooth-foot-pedal`;
- keyboard report transmission;
- key press;
- key release;
- release-all;
- modifier support;
- support for a single key or a shortcut with modifiers;
- safe behavior when the pedal is pressed while no Mac is connected;
- safe recovery after reboot or reconnect.

## Press semantics

When the debounced pedal state changes from released to pressed:

1. increment the local press counter;
2. record the press start timestamp;
3. if BLE is connected, send key-down for the configured key or shortcut;
4. expose the pedal state to ESPHome/Home Assistant independently.

When the state changes from pressed to released:

1. if BLE is connected, send key-up or release-all;
2. calculate hold duration;
3. update total active time;
4. publish telemetry asynchronously.

Never repeatedly send key-down while the pedal remains held unless required by the BLE implementation. State transitions must drive actions.

## Safety requirements

Call or emulate `releaseAll()`:

- on pedal release;
- before changing profile;
- on BLE disconnect if possible;
- after BLE reconnect before accepting a new press;
- during shutdown/reboot hooks when possible;
- after an internal timeout or inconsistent state;
- before entering any future sleep mode.

Prevent a logically stuck modifier or PTT key.

If the pedal is already physically held when BLE reconnects, choose and document one deterministic policy. Recommended default:

- send `releaseAll()` on reconnect;
- do not begin a new key-down until the pedal is released and pressed again.

This avoids unexpectedly opening the microphone after reconnection.

---

# 6. GPIO and mechanical switch requirements

Assume the pedal contains a simple mechanical microswitch.

Initial wiring:

```text
Pedal switch contact 1 -> ESP32 GPIO
Pedal switch contact 2 -> GND
```

Configure the GPIO with an internal pull-up and inverted logic:

```text
Released -> HIGH
Pressed  -> LOW
```

Select a safe configurable GPIO, avoiding boot-strapping pins where practical. Make the pin a substitution or package variable rather than hard-coding it deeply.

Implement configurable debounce, initially around 20–40 ms for both press and release. The design must avoid duplicate press counts from contact bounce.

Support both normally-open and normally-closed wiring through configuration if practical, but optimize documentation for normally-open operation.

Do not require an external visible LED in version 1. The ESP32 board may remain completely hidden inside the pedal enclosure.

---

# 6.1 Actual hardware currently available

Use the user's currently available hardware as the baseline for the first implementation and test passes:

| Category | Component | Status | Notes |
|---|---|---|---|
| Controller | ESP-32S / ESP-WROOM-32 development board, 30P/38P | Available | Classic ESP32 with Bluetooth and Wi-Fi |
| Pedal switch | Momentary foot controller pedal switch, SPDT, `1 NO + 1 NC`, rated `AC 250V 10A` | Planned for target hardware | Use the low-voltage switch contacts only; do not use any AC mains wiring in this project |
| Host computer | MacBook / macOS | Available | Primary BLE HID target |
| Power | USB 5 V | Available | Version 1 is permanently powered, no battery |

Implementation notes:

- prefer the `NO` contact pair for the first version;
- wire the pedal as a dry contact only: one contact to GPIO, one contact to GND;
- allow the firmware to support `NC` as an optional configuration later;
- because the pedal hardware is not physically present yet, Phase 1 testing may simulate pedal presses using a temporary jumper wire or a bench button between the chosen GPIO and GND.

---

# 7. Home Assistant and ESPHome entities

Expose useful information without making the project unnecessarily complex.

## Required entities

1. **Pedal state** — binary sensor: pressed/released.
2. **BLE connection state** — binary sensor: Mac connected/disconnected.
3. **Total presses since boot** — sensor.
4. **Persistent total press count** — sensor, implemented with flash-wear awareness.
5. **Last hold duration** — sensor in seconds.
6. **Total active/held time since boot** — sensor.
7. **Current profile** — select or text sensor.
8. **ESP uptime** — diagnostic sensor.
9. **Wi-Fi signal strength** — diagnostic sensor.
10. **ESPHome status** — online/offline through the normal integration.
11. **Restart control** — diagnostic button.
12. **Reset statistics** — guarded button or action.

## Optional entities

- presses today;
- active time today;
- longest press;
- average press duration;
- last press timestamp;
- firmware version;
- BLE reconnect count;
- ESP32 reboot reason;
- free heap;
- minimum free heap;
- current BLE device name;
- selected keyboard key/shortcut;
- enable/disable keyboard output while preserving telemetry;
- pairing reset button;
- safe-mode status.

Home Assistant may derive daily statistics with utility meters, history statistics, helpers, or automations. Avoid forcing all analytics into ESP32 firmware when Home Assistant can calculate them more safely.

---

# 8. Statistics and flash-wear policy

Do not write to flash after every pedal press.

Use this policy:

- maintain high-frequency counters in RAM;
- publish current values to Home Assistant;
- persist aggregate values at a controlled interval, for example every 5–15 minutes, or after a configurable number of presses;
- optionally persist on a clean restart hook when possible;
- document that sudden power loss may lose a small recent increment window;
- avoid excessive `restore_value` writes.

Provide a clear persistence strategy and justify it.

Home Assistant should be able to maintain long-term history independently of ESP32 flash storage.

## Recommended version 1 persistence strategy

Use this default strategy for long flash life:

- keep all fast-changing counters in RAM during normal operation;
- flush persistent statistics only when either `10 minutes` have passed since the last save or `100 new presses` have accumulated since the last save, whichever happens first;
- also flush on an explicit restart action when the firmware has a clean opportunity to do so;
- do not write on every press, every release, or every Home Assistant state update.

Rationale:

- `10 minutes` limits loss after sudden power removal to a small recent window;
- `100 presses` prevents unusually heavy use from delaying persistence too long;
- this keeps write frequency low enough that flash wear should remain negligible for a many-year service life target.

Version 1 should persist only the values that actually need reboot survival:

- lifetime press count;
- lifetime accumulated hold time;
- selected profile;
- optional last known configuration values that the device must restore locally.

Version 1 should not persist highly derived analytics on every change, for example:

- presses today;
- average hold duration;
- hourly or daily breakdowns;
- rolling history windows.

Those derived views may be calculated in Home Assistant for display purposes, while the source counters remain owned by the ESP32.

---

# 9. Profiles and configuration

Implement a profile abstraction even if the first release includes only a few profiles.

Suggested profiles:

1. `F13` — default universal binding.
2. `Discord PTT` — configurable key.
3. `macOS Dictation` — configurable shortcut.
4. `Superwhisper` — default example `Left Option + Space`, but do not assume it is universal.
5. `Handy` — example shortcut based on the original inspiration, configurable.
6. `Custom` — key plus zero or more modifiers.

A profile defines:

- profile name;
- HID key code;
- modifiers;
- behavior type: hold or tap;
- optional tap duration;
- whether the action is enabled.

Version 1 must prioritize **hold behavior** because PTT and push-to-dictate require key-down while pressed and key-up when released.

Profile changes may be initiated from Home Assistant, but the selected profile must be stored locally and continue working without Home Assistant.

Any profile change must first release all HID keys.

---

# 10. Sleep and power policy

The first version is USB-powered. Therefore:

- do not use ESP32 deep sleep;
- do not intentionally disconnect BLE after inactivity;
- do not require a first press to wake and a second press to activate;
- preserve immediate response to the first press;
- keep the BLE connection available;
- allow normal ESP-IDF radio power-saving mechanisms only if they do not cause missed first presses or unacceptable reconnect latency.

Explain in documentation:

- deep sleep shuts down Bluetooth and Wi-Fi;
- the first press would only wake the ESP32;
- reconnection to macOS would take time;
- Home Assistant would show the device as unavailable;
- deep sleep is unsuitable for the USB-powered version.

Design the code so a future battery-powered hardware variant could introduce a separate power policy, but do not complicate version 1 with it.

---

# 11. Wi-Fi and Bluetooth coexistence

ESP32 uses a shared 2.4 GHz radio for Wi-Fi and Bluetooth. Keep Home Assistant traffic lightweight.

Requirements:

- BLE HID latency has priority conceptually over telemetry;
- do not publish entities on every loop iteration;
- publish only on state changes or reasonable intervals;
- avoid a web server unless needed for debugging;
- avoid Bluetooth proxy mode;
- avoid BLE scanning;
- avoid audio, displays, voice assistant, or other high-memory components;
- monitor free heap and reconnect reliability;
- verify BLE performance while ESPHome Native API and Wi-Fi are connected.

Add a soak test covering simultaneous BLE use and Home Assistant connectivity.

---

# 12. Failure behavior

Define and test the following matrix:

| Condition | Expected result |
|---|---|
| Home Assistant is offline | BLE keyboard continues to work |
| Wi-Fi is unavailable | BLE keyboard continues to work |
| ESPHome API disconnects | BLE keyboard continues to work |
| Mac is disconnected | Local state and statistics continue; no invalid HID send |
| Mac reconnects | Release-all safety runs; next clean press works |
| ESP32 reboots | It advertises/reconnects and does not leave a stuck key |
| Pedal is held during reboot | No automatic unsafe key-down after reconnect |
| Rapid contact bounce | Exactly one logical press is counted |
| Wi-Fi reconnect storm | Pedal latency remains acceptable |
| Home Assistant changes profile | Existing key is released before switch |
| External component fails to initialize | Device remains diagnosable; failure is logged |

Never block the GPIO/BLE path while waiting for Wi-Fi, DNS, API, or Home Assistant.

---

# 13. macOS validation requirements

macOS support is mandatory, not assumed.

Document and test:

- initial pairing through macOS Bluetooth settings;
- reconnection after ESP32 reboot;
- reconnection after Mac sleep/wake;
- reconnection after Bluetooth is toggled off/on on the Mac;
- keyboard key-down/key-up behavior;
- modifier combinations;
- long hold behavior;
- rapid repeated presses;
- recovery from disconnect while pressed;
- removal and re-pairing;
- BLE bonding persistence;
- device naming;
- compatibility with Discord PTT;
- compatibility with a generic macOS shortcut test utility or keyboard event viewer.

Do not claim macOS compatibility until these acceptance tests are documented as passed on real hardware.

---

# 14. Repository structure

Create a clean repository similar to:

```text
esp32-bluetooth-foot-pedal/
├── README.md
├── LICENSE
├── CHANGELOG.md
├── CONTRIBUTING.md
├── CODE_OF_CONDUCT.md
├── docs/
│   ├── architecture.md
│   ├── build-guide.md
│   ├── wiring.md
│   ├── macos-pairing.md
│   ├── home-assistant.md
│   ├── troubleshooting.md
│   ├── testing.md
│   ├── licensing.md
│   └── images/
├── esphome/
│   ├── esp32-bluetooth-foot-pedal.yaml
│   ├── esp32-bluetooth-foot-pedal.example.yaml
│   ├── secrets.example.yaml
│   └── packages/
│       ├── base.yaml
│       ├── diagnostics.yaml
│       ├── statistics.yaml
│       └── profiles.yaml
├── components/
│   └── esp32_bluetooth_foot_pedal_ble_hid/
│       ├── __init__.py
│       ├── esp32_bluetooth_foot_pedal_ble_hid.h
│       ├── esp32_bluetooth_foot_pedal_ble_hid.cpp
│       └── README.md
├── tests/
│   ├── test-plan.md
│   ├── hardware-test-checklist.md
│   └── compile/
├── scripts/
│   ├── validate.sh
│   └── compile.sh
└── .github/
    ├── workflows/
    │   ├── validate.yml
    │   └── compile.yml
    ├── ISSUE_TEMPLATE/
    └── pull_request_template.md
```

Adjust this structure if ESPHome conventions require a better arrangement, but preserve separation between:

- ESPHome configuration;
- BLE HID external component;
- documentation;
- tests;
- CI.

---

# 15. Secrets and configuration

Do not commit real Wi-Fi credentials, API encryption keys, OTA passwords, or device-specific secrets.

Provide:

```text
secrets.example.yaml
```

with placeholders.

Use substitutions for:

- device name;
- friendly name;
- GPIO pin;
- debounce duration;
- BLE name;
- default profile;
- persistence interval;
- logging level.

Provide sane defaults and explain every configurable value.

---

# 16. Logging and diagnostics

Logging must be useful but must not destabilize BLE timing.

Log important events:

- boot;
- BLE initialization;
- advertising started;
- Mac connected/disconnected;
- pairing or bonding result;
- pedal logical press/release;
- profile changed;
- release-all safety action;
- Wi-Fi connected/disconnected;
- Home Assistant API connected/disconnected;
- persistence save;
- reboot reason;
- free heap warnings.

Avoid logging every loop iteration or emitting excessive logs during a held pedal state.

Do not log secrets.

---

# 17. Licensing

Before selecting a final license, inspect every dependency and copied/adapted code path.

Expected licensing direction:

- ESPHome firmware/runtime code includes GPL-licensed components;
- the candidate BLE keyboard component is GPL-3.0;
- therefore GPL-3.0-or-later is likely the simplest repository license if its code is adapted or included.

Tasks:

1. Verify current ESPHome licensing.
2. Verify the BLE HID implementation license.
3. Verify whether any code is copied, adapted, or only used as conceptual reference.
4. Preserve required copyright notices.
5. Add attribution where legally required.
6. Do not copy Footy source code, firmware, CAD, images, name, or documentation unless the applicable license explicitly permits it and attribution obligations are fulfilled.
7. State clearly that Footy was design inspiration, not the codebase.
8. Create `docs/licensing.md` with the dependency license analysis.

If dependency licensing is incompatible with the desired repository model, stop and report the conflict before implementation.

---

# 18. Development phases

Work in phases and commit after each coherent phase.

## Phase 0 — research and decision record

Produce:

- comparison of available ESPHome BLE HID components;
- macOS compatibility evidence;
- ESPHome-version compatibility;
- license comparison;
- selected implementation strategy;
- architecture decision record.

Do not start with a large implementation before this decision is documented.

## Phase 1 — minimal BLE proof of concept

Create the smallest firmware that:

- boots;
- advertises as `esp32-bluetooth-foot-pedal`;
- pairs with macOS;
- sends one configurable key;
- holds while the switch is pressed;
- releases when the switch is released;
- reconnects after reboot;
- performs release-all safety.

## Phase 2 — ESPHome integration

Add:

- Wi-Fi;
- Native API;
- OTA;
- pedal binary sensor;
- BLE connection sensor;
- uptime and Wi-Fi diagnostics.

Prove that BLE still works when Home Assistant is offline.

## Phase 3 — statistics

Add:

- presses since boot;
- persistent total presses;
- last hold duration;
- active time;
- controlled persistence;
- reset statistics action.

## Phase 4 — profiles

Add:

- default `F13` profile;
- configurable shortcuts;
- Home Assistant profile selection;
- local persistence;
- release-all before profile changes.

## Phase 5 — reliability

Add:

- reconnect handling;
- Mac sleep/wake testing;
- held-during-disconnect safety;
- long-duration soak testing;
- Wi-Fi/BLE coexistence validation;
- heap monitoring;
- recovery documentation.

## Phase 6 — release preparation

Add:

- complete README;
- wiring diagram;
- build guide;
- macOS pairing guide;
- Home Assistant screenshots/placeholders;
- troubleshooting;
- CI compile checks;
- versioning;
- release notes.

---

# 19. Acceptance criteria for version 1.0

Version 1.0 is acceptable only when all of these are true:

1. A single pedal press results in exactly one logical press transition.
2. Holding the pedal keeps the configured HID key held.
3. Releasing the pedal releases the key reliably.
4. No key or modifier remains stuck after disconnect or reconnect testing.
5. The Mac reconnects after an ESP32 reboot without requiring firmware changes.
6. Home Assistant can be completely offline and the pedal still works.
7. Wi-Fi can be disabled and the pedal still works.
8. Home Assistant shows pedal state and BLE connection state when available.
9. Home Assistant shows press statistics and hold duration.
10. Statistics persistence does not write flash after every press.
11. OTA works without breaking future BLE startup.
12. The firmware compiles reproducibly from a clean checkout.
13. Secrets are not committed.
14. The repository includes wiring and build instructions.
15. Licensing and attribution are documented.
16. Real macOS hardware testing is documented.
17. Deep sleep is not enabled in the USB-powered release.
18. No Home Assistant automation is required for the keyboard action.

---

# 20. Deliverables expected from Codex

Create the complete initial repository, not merely a prose recommendation.

At minimum, deliver:

1. A research summary and architecture decision record.
2. Repository structure.
3. ESPHome YAML configuration.
4. Local or pinned BLE HID external component.
5. GPIO pedal handling with debounce.
6. BLE HID press/release/release-all implementation.
7. Home Assistant entities.
8. Statistics implementation with a flash-wear policy.
9. Profile abstraction.
10. Example secrets file.
11. Build and flashing guide.
12. Wiring documentation.
13. macOS pairing and testing guide.
14. Troubleshooting guide.
15. License and attribution files.
16. CI validation/compile workflow.
17. Hardware test checklist.
18. A clear list of unresolved hardware-dependent tests.

When hardware verification cannot be performed in the development environment, mark it explicitly as **REQUIRES REAL HARDWARE VALIDATION**. Do not fabricate successful test results.

---

# 21. Implementation quality rules

- Prefer a small, understandable implementation over a feature-heavy one.
- Avoid blocking delays in the pedal event path.
- Avoid dynamic allocation in high-frequency event handling where practical.
- Keep BLE HID state ownership in one component.
- Keep GPIO state handling deterministic.
- Separate telemetry publication from HID actions.
- Use descriptive names.
- Add comments explaining non-obvious BLE and reconnect behavior.
- Handle errors explicitly.
- Pin external dependencies.
- Do not silently ignore incompatible ESPHome versions.
- Include compile-time or startup diagnostics for unsupported configurations.
- Avoid unnecessary web server, BLE scanning, Bluetooth proxy, display, audio, or voice components.
- Ensure the project remains maintainable by one developer.

---

# 22. Initial implementation assumptions

Use these assumptions unless research proves they are unsuitable:

```text
Board: ESP-32S / ESP-WROOM-32 development board, 30P/38P
Framework: ESP-IDF through ESPHome
Power: permanent USB 5 V power
Pedal switch: momentary SPDT mechanical foot switch, use `NO` contact first
GPIO mode: INPUT_PULLUP, inverted
Default key: F13
Debounce: 30 ms press and release
BLE device name: esp32-bluetooth-foot-pedal
Home Assistant transport: ESPHome Native API
OTA: ESPHome OTA
Sleep: disabled
Primary target OS: macOS
Primary use case: Discord Push-to-Talk
Persistent statistics flush policy: every 10 minutes or every 100 new presses
Repository license: GPL-3.0-or-later, subject to dependency audit
```

Make hardware-specific values configurable.

---

# 23. First response expected from Codex

Before generating large amounts of code, respond with:

1. the selected BLE HID implementation approach;
2. why it is compatible with ESPHome and the selected ESP32 board;
3. known macOS risks;
4. dependency licenses;
5. the proposed repository tree;
6. a phase-by-phase implementation plan;
7. any assumptions that cannot be verified without hardware.

Then proceed to create the repository files.

Do not ask for confirmation unless a decision is impossible to make safely. Make reasonable documented assumptions and keep moving.

---

# 24. Project summary

esp32-bluetooth-foot-pedal is an ESP32-based Bluetooth foot pedal that behaves as a one-key BLE keyboard for macOS. Its local HID behavior is the primary product and must never depend on Wi-Fi or Home Assistant. ESPHome adds optional Home Assistant visibility, diagnostics, counters, profile selection, and OTA updates. The project adopts the strongest ideas from the Footy project—one-key keyboard semantics, press-and-hold behavior, configurability, reproducibility, safety, and open documentation—while using a different hardware platform, transport, enclosure strategy, firmware architecture, and integration model.

# esp32_bluetooth_foot_pedal_ble_hid

Local ESPHome external component for foot-pedal state and statistics.

Current stage:

- owns pedal session state and statistics surface
- exposes methods callable from ESPHome lambdas
- keeps the intended `press / hold / release` API for the project
- does not send the BLE key itself in the current version
- works alongside `espidf_ble_keyboard`, which currently handles the actual BLE HID keyboard transport

Reference direction:

- ESPHome local external component structure
- ESP-IDF BLE HID implementation approach
- candidate upstream reference: `markusg1234/ESPHome-espidf_ble_keyboard`

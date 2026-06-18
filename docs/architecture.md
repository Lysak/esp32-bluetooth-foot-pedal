# Architecture

The pedal has two parallel paths:

```text
GPIO pedal input -> local BLE HID behavior -> macOS
GPIO pedal input -> ESPHome telemetry -> Home Assistant
```

The first path is primary and must not depend on Wi-Fi or Home Assistant.

# Architecture

The pedal has two parallel paths:

```text
GPIO pedal input -> local BLE HID behavior -> macOS
GPIO pedal input -> ESPHome telemetry -> Home Assistant
```

The first path is primary and must not depend on Wi-Fi or Home Assistant.

BLE lifecycle policy:

- the pedal does not auto-advertise on boot;
- the pedal does not auto-advertise after disconnect;
- only a physical pedal press may start BLE advertising;
- if the pedal reconnects while still physically held, the HID key-down is sent after reconnect and remains held until release.

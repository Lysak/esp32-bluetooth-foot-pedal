# macOS Pairing

Validation flow:

1. Flash the firmware.
2. Open macOS Bluetooth settings.
3. Find `esp32-bluetooth-foot-pedal`.
4. Pair it as a keyboard.
5. Confirm that after boot the pedal does not reconnect on its own until a physical press starts advertising.
6. Trigger `GPIO27` to GND and keep it held if the pedal is currently disconnected.
7. Confirm macOS reconnects and receives `F13` once the BLE link is back.
8. Release the pedal and confirm the host sees key release.

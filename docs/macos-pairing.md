# macOS Pairing

Validation flow:

1. Flash the firmware.
2. Open macOS Bluetooth settings.
3. Find `esp32-bluetooth-foot-pedal`.
4. Pair it as a keyboard.
5. Reboot the ESP32 and confirm automatic reconnect.
6. Trigger `GPIO27` to GND briefly and confirm the host sees `F13`.

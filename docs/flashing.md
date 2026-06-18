# Flashing

1. Copy `esphome/secrets.example.yaml` to `esphome/secrets.yaml`.
2. Fill in Wi-Fi, API, and OTA secrets.
3. Plug the ESP32 in by USB.
4. Run:

```bash
make esphome-compile
make esphome-flash DEVICE=/dev/tty.usbserial-XXXX
```

To watch logs:

```bash
cd esphome && uvx esphome logs esp32-bluetooth-foot-pedal.yaml --device /dev/tty.usbserial-XXXX
```

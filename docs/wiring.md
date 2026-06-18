# Wiring

Initial version 1 wiring:

```text
Pedal NO contact -> ESP32 GPIO27
Pedal COM        -> ESP32 GND
```

The firmware assumes `INPUT_PULLUP` and inverted logic.

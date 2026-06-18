# Testing

Bench testing before the physical pedal arrives:

1. Flash the board.
2. Pair it with the Mac.
3. Simulate the pedal by shorting GPIO27 to GND briefly.
4. Verify the Mac receives `F13`.
5. Verify ESPHome entity updates in Home Assistant.

Real hardware validation is still required for:

- real pedal bounce behavior
- reconnect safety after disconnects
- long hold ergonomics with the real switch

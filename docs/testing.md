# Testing

Bench testing before the physical pedal arrives:

1. Flash the board.
2. Pair it with the Mac.
3. Simulate the pedal by shorting GPIO27 to GND briefly.
4. Verify the Mac receives `F13`.
5. Verify ESPHome entity updates in Home Assistant.
6. Turn Bluetooth off and back on, or disconnect the pedal from macOS.
7. Confirm the pedal does not reconnect by itself.
8. Hold the pedal down to trigger a new reconnect and confirm `F13` becomes held after reconnect completes.

Real hardware validation is still required for:

- real pedal bounce behavior
- press-only reconnect after disconnects
- long hold ergonomics with the real switch

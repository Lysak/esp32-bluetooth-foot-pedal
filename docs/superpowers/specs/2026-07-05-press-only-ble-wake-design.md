# Press-Only BLE Wake Design

## Goal

Prevent the pedal from waking or reappearing to macOS on its own after a BLE disconnect.

The pedal must only become connectable again after a real physical pedal press.

## Approved Behavior

- The BLE keyboard must not auto-advertise on boot.
- The BLE keyboard must not auto-advertise after disconnect.
- A physical pedal press is the only event allowed to start BLE advertising.
- If the pedal is already connected when pressed, send the configured HID key-down immediately.
- If the pedal is disconnected when pressed, start advertising and remember that the pedal is physically held.
- When the Mac reconnects while the pedal is still physically held, send the configured HID key-down and keep it held.
- On pedal release, always send release-all if connected and clear any pending held-after-reconnect state.
- If the pedal is released before the Mac reconnects, stop advertising again and do not reconnect later by itself.

## Non-Goals

- No idle timeout logic.
- No firmware-driven disconnect timer.
- No automatic reconnect attempt after disconnect.
- No background advertising window after disconnect.
- No requirement to mimic an undocumented Apple keyboard timeout exactly.

## Why This Changes The Existing Policy

The current repo policy assumed a USB-powered pedal should keep BLE available continuously and should not require a first press to wake.

That policy conflicts with the actual user goal here: avoiding unexpected Mac wake events from a keyboard-class BLE HID device.

The new policy intentionally accepts that a reconnect can take a short time and that the first physical press may serve as the wake-and-reconnect action.

## Firmware Design

### BLE keyboard component

Add explicit BLE advertising controls to `components/espidf_ble_keyboard/`:

- `auto_advertise_on_boot`
- `auto_advertise_on_disconnect`
- `start_advertising_now()`
- `stop_advertising_now()`

For this pedal firmware, both auto-advertise flags are disabled in YAML.

### Pedal state component

Extend `components/esp32_bluetooth_foot_pedal_ble_hid/` so it can:

- observe the BLE keyboard connection state;
- start advertising on physical press when disconnected;
- remember a pending held press during reconnect;
- send key-down after reconnect if the pedal is still held;
- stop advertising when the pedal is released before reconnect completes.

### YAML wiring

Keep the GPIO debounce flow unchanged, but route HID behavior through the updated pedal-state component so reconnect and held-key semantics stay consistent.

## Documentation Changes

Update the canonical docs and design docs that currently describe:

- no deep sleep;
- no intentional disconnect after inactivity;
- no first-press wake behavior;
- continuously available BLE connection policy.

The updated docs must describe the new press-only advertising policy instead.

## Verification

Minimum required verification:

- local compile succeeds;
- the resulting firmware can be uploaded to the live pedal over OTA if the device is reachable;
- after flashing, the pedal no longer auto-re-advertises after a disconnect until a new physical press occurs.

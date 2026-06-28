# Held Time Persistence Design

Goal: persist pedal hold history and expose monthly held-time aggregates in Home Assistant without wearing out ESP32 flash.

Design:
- Use `time.homeassistant` as the authoritative local clock and timezone source.
- Keep raw pedal press/release timing in RAM while the pedal is held.
- On release, create an event record with local start epoch, local release epoch, and hold duration in milliseconds.
- Append new events to a RAM buffer and flush them to NVS once per hour.
- Store event history in NVS as paged binary blobs so older pages are not rewritten on every flush.
- Track current-month and previous-month held-time totals in NVS using the local `Europe/Kyiv` calendar month.
- If a hold spans a month boundary, split the duration between months.

Constraints:
- Event history on ESP32 is finite because NVS space is finite.
- The implementation uses a large ring of NVS pages. When the configured page limit is reached, the oldest pages are overwritten.
- Sudden power loss can lose up to one hour of not-yet-flushed events.
- Events that happen before the Home Assistant clock becomes valid are not written to persistent history because they do not have trustworthy calendar timestamps.

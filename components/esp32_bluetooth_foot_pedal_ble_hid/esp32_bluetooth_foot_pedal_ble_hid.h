#pragma once

#include <cstdint>
#include <string>

#include "esphome/core/component.h"

namespace esphome {
namespace esp32_bluetooth_foot_pedal_ble_hid {

class Esp32BluetoothFootPedalBleHid : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_ble_device_name(const std::string &ble_device_name) { ble_device_name_ = ble_device_name; }
  void set_default_key_name(const std::string &default_key_name) { default_key_name_ = default_key_name; }

  void handle_pedal_press();
  void handle_pedal_release();
  void reset_statistics();

  bool is_ble_connected() const { return ble_connected_; }
  bool is_key_currently_held() const { return key_currently_held_; }
  bool has_pending_persistence() const { return persistence_dirty_; }

  uint32_t get_session_press_count() const { return session_press_count_; }
  uint32_t get_persistent_press_count() const { return persistent_press_count_; }
  float get_last_hold_duration_seconds() const;
  float get_session_held_duration_seconds() const;
  const std::string &get_default_key_name() const { return default_key_name_; }

 private:
  void send_key_down_();
  void send_release_all_();
  void maybe_flush_persistence_();
  static uint32_t now_ms_();

  std::string ble_device_name_{"esp32-bluetooth-foot-pedal"};
  std::string default_key_name_{"F13"};

  bool ble_connected_{false};
  bool key_currently_held_{false};
  bool pedal_pressed_{false};
  bool persistence_dirty_{false};

  uint32_t hold_started_at_ms_{0};
  uint32_t last_hold_duration_ms_{0};
  uint32_t session_press_count_{0};
  uint32_t persistent_press_count_{0};
  uint32_t pending_presses_since_persist_{0};
  uint32_t last_persist_at_ms_{0};
  uint64_t session_held_duration_ms_{0};
};

}  // namespace esp32_bluetooth_foot_pedal_ble_hid
}  // namespace esphome

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "esphome/components/espidf_ble_keyboard/espidf_ble_keyboard.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"

namespace esphome {
namespace esp32_bluetooth_foot_pedal_ble_hid {

struct PedalEventRecord {
  uint32_t started_at_epoch{0};
  uint32_t released_at_epoch{0};
  uint32_t hold_duration_ms{0};
};

class Esp32BluetoothFootPedalBleHid : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_ble_device_name(const std::string &ble_device_name) { ble_device_name_ = ble_device_name; }
  void set_default_key_name(const std::string &default_key_name) { default_key_name_ = default_key_name; }
  void set_time_source(time::RealTimeClock *time_source) { time_source_ = time_source; }
  void set_ble_keyboard(espidf_ble_keyboard::EspidfBleKeyboard *ble_keyboard) { ble_keyboard_ = ble_keyboard; }

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
  float get_persistent_held_duration_seconds() const;
  float get_current_month_held_duration_seconds() const;
  float get_previous_month_held_duration_seconds() const;
  uint32_t get_persisted_event_count() const { return persisted_event_count_; }
  std::string get_last_event_timestamp_iso() const;
  const std::string &get_default_key_name() const { return default_key_name_; }

 private:
  void send_key_down_();
  void send_release_all_();
  void ensure_nvs_ready_();
  void load_persistent_state_();
  void maybe_flush_persistence_();
  bool flush_persistence_();
  void append_event_(const PedalEventRecord &event);
  void update_monthly_held_time_(const PedalEventRecord &event);
  void add_held_duration_to_month_(uint32_t month_key, uint64_t duration_ms);
  void rotate_month_buckets_if_needed_();
  uint32_t get_local_month_key_(uint32_t epoch) const;
  uint32_t get_previous_month_key_(uint32_t month_key) const;
  uint32_t get_next_month_start_epoch_(uint32_t epoch) const;
  bool current_time_valid_() const;
  bool current_epoch_seconds_(uint32_t *epoch_seconds) const;
  bool clear_persistent_storage_();
  std::string format_local_timestamp_(uint32_t epoch) const;
  static uint32_t now_ms_();

  std::string ble_device_name_{"esp32-bluetooth-foot-pedal"};
  std::string default_key_name_{"F13"};
  time::RealTimeClock *time_source_{nullptr};

  bool ble_connected_{false};
  bool key_currently_held_{false};
  bool pedal_pressed_{false};
  bool persistence_dirty_{false};
  bool nvs_ready_{false};
  bool press_started_with_valid_time_{false};
  bool pending_key_down_after_reconnect_{false};
  bool hid_key_down_sent_{false};

  uint32_t hold_started_at_ms_{0};
  uint32_t hold_started_at_epoch_{0};
  uint32_t last_hold_duration_ms_{0};
  uint32_t session_press_count_{0};
  uint32_t persistent_press_count_{0};
  uint32_t last_persist_at_ms_{0};
  uint32_t persisted_event_count_{0};
  uint32_t current_month_key_{0};
  uint32_t previous_month_key_{0};
  uint32_t last_event_release_epoch_{0};
  uint64_t session_held_duration_ms_{0};
  uint64_t persistent_held_duration_ms_{0};
  uint64_t current_month_held_duration_ms_{0};
  uint64_t previous_month_held_duration_ms_{0};
  std::vector<PedalEventRecord> pending_events_;
  espidf_ble_keyboard::EspidfBleKeyboard *ble_keyboard_{nullptr};
};

}  // namespace esp32_bluetooth_foot_pedal_ble_hid
}  // namespace esphome

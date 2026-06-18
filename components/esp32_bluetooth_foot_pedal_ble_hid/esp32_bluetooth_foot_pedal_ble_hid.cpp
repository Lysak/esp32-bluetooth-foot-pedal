#include "esp32_bluetooth_foot_pedal_ble_hid.h"

#include "esp_timer.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp32_bluetooth_foot_pedal_ble_hid {

static const char *const TAG = "esp32_ble_foot_pedal";
static const uint32_t PERSIST_INTERVAL_MS = 10 * 60 * 1000;
static const uint32_t PERSIST_PRESS_THRESHOLD = 100;

void Esp32BluetoothFootPedalBleHid::setup() {
  this->last_persist_at_ms_ = now_ms_();
  ESP_LOGI(TAG, "BLE HID foot pedal component scaffold setup");
  ESP_LOGI(TAG, "Target BLE device name: %s", this->ble_device_name_.c_str());
  ESP_LOGW(TAG, "BLE HID transport is not complete yet; current build focuses on pedal state handling and ESPHome surface");
}

void Esp32BluetoothFootPedalBleHid::loop() { this->maybe_flush_persistence_(); }

void Esp32BluetoothFootPedalBleHid::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 Bluetooth Foot Pedal BLE HID");
  ESP_LOGCONFIG(TAG, "  BLE device name: %s", this->ble_device_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Default key name: %s", this->default_key_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Persistent flush policy: 10 minutes or 100 presses");
}

float Esp32BluetoothFootPedalBleHid::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void Esp32BluetoothFootPedalBleHid::handle_pedal_press() {
  if (this->pedal_pressed_) {
    return;
  }

  this->pedal_pressed_ = true;
  this->key_currently_held_ = true;
  this->hold_started_at_ms_ = now_ms_();
  this->session_press_count_++;
  this->persistent_press_count_++;
  this->pending_presses_since_persist_++;
  this->persistence_dirty_ = true;

  ESP_LOGI(TAG, "Pedal press #%u", this->session_press_count_);
  this->send_key_down_();
}

void Esp32BluetoothFootPedalBleHid::handle_pedal_release() {
  if (!this->pedal_pressed_) {
    return;
  }

  this->pedal_pressed_ = false;
  const uint32_t now = now_ms_();
  if (now >= this->hold_started_at_ms_) {
    this->last_hold_duration_ms_ = now - this->hold_started_at_ms_;
    this->session_held_duration_ms_ += this->last_hold_duration_ms_;
  } else {
    this->last_hold_duration_ms_ = 0;
  }

  this->key_currently_held_ = false;
  this->persistence_dirty_ = true;

  ESP_LOGI(TAG, "Pedal release, hold %.3fs", this->get_last_hold_duration_seconds());
  this->send_release_all_();
}

void Esp32BluetoothFootPedalBleHid::reset_statistics() {
  this->last_hold_duration_ms_ = 0;
  this->session_press_count_ = 0;
  this->persistent_press_count_ = 0;
  this->pending_presses_since_persist_ = 0;
  this->session_held_duration_ms_ = 0;
  this->persistence_dirty_ = true;
  this->last_persist_at_ms_ = now_ms_();
  ESP_LOGW(TAG, "Statistics reset requested");
}

float Esp32BluetoothFootPedalBleHid::get_last_hold_duration_seconds() const {
  return static_cast<float>(this->last_hold_duration_ms_) / 1000.0f;
}

float Esp32BluetoothFootPedalBleHid::get_session_held_duration_seconds() const {
  uint64_t total_ms = this->session_held_duration_ms_;
  if (this->pedal_pressed_) {
    const uint32_t now = now_ms_();
    if (now >= this->hold_started_at_ms_) {
      total_ms += now - this->hold_started_at_ms_;
    }
  }
  return static_cast<float>(total_ms) / 1000.0f;
}

void Esp32BluetoothFootPedalBleHid::send_key_down_() {
  ESP_LOGI(TAG, "TODO BLE key down: %s", this->default_key_name_.c_str());
}

void Esp32BluetoothFootPedalBleHid::send_release_all_() { ESP_LOGI(TAG, "TODO BLE release all"); }

void Esp32BluetoothFootPedalBleHid::maybe_flush_persistence_() {
  if (!this->persistence_dirty_) {
    return;
  }

  const uint32_t now = now_ms_();
  const bool interval_elapsed = (now - this->last_persist_at_ms_) >= PERSIST_INTERVAL_MS;
  const bool press_threshold_hit = this->pending_presses_since_persist_ >= PERSIST_PRESS_THRESHOLD;
  if (!interval_elapsed && !press_threshold_hit) {
    return;
  }

  this->last_persist_at_ms_ = now;
  this->pending_presses_since_persist_ = 0;
  this->persistence_dirty_ = false;
  ESP_LOGI(TAG, "TODO persist counters: total_presses=%u total_held=%.3fs",
           this->persistent_press_count_, this->get_session_held_duration_seconds());
}

uint32_t Esp32BluetoothFootPedalBleHid::now_ms_() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

}  // namespace esp32_bluetooth_foot_pedal_ble_hid
}  // namespace esphome

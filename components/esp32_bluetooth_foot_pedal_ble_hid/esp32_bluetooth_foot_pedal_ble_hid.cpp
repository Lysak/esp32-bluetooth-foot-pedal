#include "esp32_bluetooth_foot_pedal_ble_hid.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "esphome/core/log.h"

namespace esphome {
namespace esp32_bluetooth_foot_pedal_ble_hid {

static const char *const TAG = "esp32_ble_foot_pedal";
static const char *const NVS_NAMESPACE = "pedal_stats";
static const char *const NVS_META_KEY = "meta";
static const uint32_t PERSIST_INTERVAL_MS = 60 * 60 * 1000;
static const uint32_t STORAGE_VERSION = 1;
static const uint16_t MAX_EVENT_PAGES = 64;
static const uint16_t EVENTS_PER_PAGE = 24;

struct PersistentStatsState {
  uint32_t version{STORAGE_VERSION};
  uint32_t persistent_press_count{0};
  uint32_t persisted_event_count{0};
  uint32_t current_month_key{0};
  uint32_t previous_month_key{0};
  uint32_t last_event_release_epoch{0};
  uint64_t persistent_held_duration_ms{0};
  uint64_t current_month_held_duration_ms{0};
  uint64_t previous_month_held_duration_ms{0};
  uint16_t head_page{0};
  uint16_t tail_page{0};
  uint16_t page_count{0};
  uint16_t reserved{0};
};

struct EventPageBlob {
  uint16_t count{0};
  uint16_t reserved{0};
  PedalEventRecord events[EVENTS_PER_PAGE]{};
};

static std::string page_key_for_index(uint16_t page_index) {
  char key[16];
  std::snprintf(key, sizeof(key), "pg%u", static_cast<unsigned>(page_index));
  return std::string(key);
}

void Esp32BluetoothFootPedalBleHid::setup() {
  this->ensure_nvs_ready_();
  this->load_persistent_state_();
  this->last_persist_at_ms_ = now_ms_();
  ESP_LOGI(TAG, "BLE HID foot pedal component scaffold setup");
  ESP_LOGI(TAG, "Target BLE device name: %s", this->ble_device_name_.c_str());
  ESP_LOGI(TAG, "Persistent flush policy: hourly journal writes with monthly held-time aggregates");
  ESP_LOGW(TAG, "BLE HID transport is not complete yet; current build focuses on pedal state handling and ESPHome surface");
}

void Esp32BluetoothFootPedalBleHid::loop() {
  this->rotate_month_buckets_if_needed_();
  this->maybe_flush_persistence_();
}

void Esp32BluetoothFootPedalBleHid::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 Bluetooth Foot Pedal BLE HID");
  ESP_LOGCONFIG(TAG, "  BLE device name: %s", this->ble_device_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Default key name: %s", this->default_key_name_.c_str());
  ESP_LOGCONFIG(TAG, "  Persistent flush policy: hourly");
  ESP_LOGCONFIG(TAG, "  Persisted total presses: %u", this->persistent_press_count_);
  ESP_LOGCONFIG(TAG, "  Persisted total held time: %.3fs", this->get_persistent_held_duration_seconds());
  ESP_LOGCONFIG(TAG, "  Persisted event history: %u entries (paged ring)", this->persisted_event_count_);
}

float Esp32BluetoothFootPedalBleHid::get_setup_priority() const { return setup_priority::AFTER_WIFI; }

void Esp32BluetoothFootPedalBleHid::handle_pedal_press() {
  if (this->pedal_pressed_) {
    return;
  }

  this->pedal_pressed_ = true;
  this->key_currently_held_ = true;
  this->hold_started_at_ms_ = now_ms_();
  this->press_started_with_valid_time_ = this->current_epoch_seconds_(&this->hold_started_at_epoch_);
  this->session_press_count_++;
  this->persistent_press_count_++;
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
  this->persistent_held_duration_ms_ += this->last_hold_duration_ms_;
  this->persistence_dirty_ = true;

  uint32_t released_at_epoch = 0;
  const bool release_time_valid = this->current_epoch_seconds_(&released_at_epoch);
  if (this->press_started_with_valid_time_ && release_time_valid && released_at_epoch >= this->hold_started_at_epoch_) {
    PedalEventRecord event{};
    event.started_at_epoch = this->hold_started_at_epoch_;
    event.released_at_epoch = released_at_epoch;
    event.hold_duration_ms = this->last_hold_duration_ms_;
    this->append_event_(event);
    this->update_monthly_held_time_(event);
    this->last_event_release_epoch_ = released_at_epoch;
  } else if (this->last_hold_duration_ms_ > 0) {
    ESP_LOGW(TAG, "Skipping persistent event history for this hold because local time is not available yet");
  }

  ESP_LOGI(TAG, "Pedal release, hold %.3fs", this->get_last_hold_duration_seconds());
  this->send_release_all_();
}

void Esp32BluetoothFootPedalBleHid::reset_statistics() {
  this->last_hold_duration_ms_ = 0;
  this->session_press_count_ = 0;
  this->persistent_press_count_ = 0;
  this->session_held_duration_ms_ = 0;
  this->persistent_held_duration_ms_ = 0;
  this->current_month_key_ = 0;
  this->previous_month_key_ = 0;
  this->current_month_held_duration_ms_ = 0;
  this->previous_month_held_duration_ms_ = 0;
  this->persisted_event_count_ = 0;
  this->last_event_release_epoch_ = 0;
  this->pending_events_.clear();
  this->persistence_dirty_ = false;
  this->last_persist_at_ms_ = now_ms_();
  this->clear_persistent_storage_();
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

float Esp32BluetoothFootPedalBleHid::get_persistent_held_duration_seconds() const {
  return static_cast<float>(this->persistent_held_duration_ms_) / 1000.0f;
}

float Esp32BluetoothFootPedalBleHid::get_current_month_held_duration_seconds() const {
  return static_cast<float>(this->current_month_held_duration_ms_) / 1000.0f;
}

float Esp32BluetoothFootPedalBleHid::get_previous_month_held_duration_seconds() const {
  return static_cast<float>(this->previous_month_held_duration_ms_) / 1000.0f;
}

std::string Esp32BluetoothFootPedalBleHid::get_last_event_timestamp_iso() const {
  if (!this->pending_events_.empty()) {
    return this->format_local_timestamp_(this->pending_events_.back().released_at_epoch);
  }
  return this->format_local_timestamp_(this->last_event_release_epoch_);
}

void Esp32BluetoothFootPedalBleHid::send_key_down_() {
  ESP_LOGI(TAG, "TODO BLE key down: %s", this->default_key_name_.c_str());
}

void Esp32BluetoothFootPedalBleHid::send_release_all_() { ESP_LOGI(TAG, "TODO BLE release all"); }

void Esp32BluetoothFootPedalBleHid::ensure_nvs_ready_() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_LOGW(TAG, "NVS requires erase before initialization; resetting NVS flash");
    ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize NVS: %s", esp_err_to_name(ret));
    this->nvs_ready_ = false;
    return;
  }
  this->nvs_ready_ = true;
}

void Esp32BluetoothFootPedalBleHid::load_persistent_state_() {
  if (!this->nvs_ready_) {
    return;
  }

  nvs_handle_t handle;
  esp_err_t open_ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (open_ret != ESP_OK) {
    ESP_LOGW(TAG, "Unable to open NVS namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(open_ret));
    return;
  }

  PersistentStatsState state{};
  size_t state_size = sizeof(state);
  esp_err_t get_ret = nvs_get_blob(handle, NVS_META_KEY, &state, &state_size);
  if (get_ret == ESP_OK && state_size == sizeof(state) && state.version == STORAGE_VERSION) {
    this->persistent_press_count_ = state.persistent_press_count;
    this->persisted_event_count_ = state.persisted_event_count;
    this->persistent_held_duration_ms_ = state.persistent_held_duration_ms;
    this->current_month_key_ = state.current_month_key;
    this->previous_month_key_ = state.previous_month_key;
    this->current_month_held_duration_ms_ = state.current_month_held_duration_ms;
    this->previous_month_held_duration_ms_ = state.previous_month_held_duration_ms;
    this->last_event_release_epoch_ = state.last_event_release_epoch;
    ESP_LOGI(TAG, "Loaded persistent stats: presses=%u events=%u held=%.3fs",
             this->persistent_press_count_, this->persisted_event_count_, this->get_persistent_held_duration_seconds());
  } else if (get_ret != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGW(TAG, "Persistent stats metadata missing or incompatible; starting fresh");
  }

  nvs_close(handle);
}

void Esp32BluetoothFootPedalBleHid::maybe_flush_persistence_() {
  if (!this->persistence_dirty_) {
    return;
  }

  const uint32_t now = now_ms_();
  if ((now - this->last_persist_at_ms_) < PERSIST_INTERVAL_MS) {
    return;
  }

  if (this->flush_persistence_()) {
    this->last_persist_at_ms_ = now;
    this->persistence_dirty_ = false;
  }
}

bool Esp32BluetoothFootPedalBleHid::flush_persistence_() {
  if (!this->nvs_ready_) {
    return false;
  }

  nvs_handle_t handle;
  esp_err_t open_ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (open_ret != ESP_OK) {
    ESP_LOGE(TAG, "Unable to open NVS for flush: %s", esp_err_to_name(open_ret));
    return false;
  }

  PersistentStatsState state{};
  size_t state_size = sizeof(state);
  esp_err_t get_ret = nvs_get_blob(handle, NVS_META_KEY, &state, &state_size);
  if (get_ret != ESP_OK || state_size != sizeof(state) || state.version != STORAGE_VERSION) {
    state = PersistentStatsState{};
  }

  for (const auto &event : this->pending_events_) {
    EventPageBlob page{};

    if (state.page_count == 0) {
      state.head_page = 0;
      state.tail_page = 0;
      state.page_count = 1;
    } else {
      const std::string tail_key = page_key_for_index(state.tail_page);
      size_t page_size = sizeof(page);
      if (nvs_get_blob(handle, tail_key.c_str(), &page, &page_size) != ESP_OK || page_size != sizeof(page)) {
        page = EventPageBlob{};
      }
    }

    if (page.count >= EVENTS_PER_PAGE) {
      state.tail_page = (state.tail_page + 1) % MAX_EVENT_PAGES;
      if (state.page_count < MAX_EVENT_PAGES) {
        state.page_count++;
      } else {
        state.head_page = (state.head_page + 1) % MAX_EVENT_PAGES;
      }
      page = EventPageBlob{};
    }

    page.events[page.count++] = event;
    const std::string page_key = page_key_for_index(state.tail_page);
    esp_err_t page_ret = nvs_set_blob(handle, page_key.c_str(), &page, sizeof(page));
    if (page_ret != ESP_OK) {
      ESP_LOGE(TAG, "Failed to write event page %u: %s", state.tail_page, esp_err_to_name(page_ret));
      nvs_close(handle);
      return false;
    }
  }

  state.version = STORAGE_VERSION;
  state.persistent_press_count = this->persistent_press_count_;
  state.persisted_event_count = this->persisted_event_count_;
  state.current_month_key = this->current_month_key_;
  state.previous_month_key = this->previous_month_key_;
  state.last_event_release_epoch = this->last_event_release_epoch_;
  state.persistent_held_duration_ms = this->persistent_held_duration_ms_;
  state.current_month_held_duration_ms = this->current_month_held_duration_ms_;
  state.previous_month_held_duration_ms = this->previous_month_held_duration_ms_;

  esp_err_t meta_ret = nvs_set_blob(handle, NVS_META_KEY, &state, sizeof(state));
  if (meta_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to write persistent stats metadata: %s", esp_err_to_name(meta_ret));
    nvs_close(handle);
    return false;
  }

  esp_err_t commit_ret = nvs_commit(handle);
  nvs_close(handle);
  if (commit_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to commit persistent stats: %s", esp_err_to_name(commit_ret));
    return false;
  }

  ESP_LOGI(TAG, "Persisted %u buffered events; total presses=%u total held=%.3fs",
           static_cast<unsigned>(this->pending_events_.size()), this->persistent_press_count_,
           this->get_persistent_held_duration_seconds());
  this->pending_events_.clear();
  return true;
}

void Esp32BluetoothFootPedalBleHid::append_event_(const PedalEventRecord &event) {
  this->pending_events_.push_back(event);
  this->persisted_event_count_++;
}

void Esp32BluetoothFootPedalBleHid::update_monthly_held_time_(const PedalEventRecord &event) {
  if (event.hold_duration_ms == 0) {
    return;
  }

  if (event.released_at_epoch <= event.started_at_epoch) {
    this->add_held_duration_to_month_(this->get_local_month_key_(event.released_at_epoch), event.hold_duration_ms);
    return;
  }

  uint32_t segment_start = event.started_at_epoch;
  uint32_t remaining_ms = event.hold_duration_ms;
  const uint32_t total_span_seconds = std::max<uint32_t>(1, event.released_at_epoch - event.started_at_epoch);

  while (segment_start < event.released_at_epoch && remaining_ms > 0) {
    const uint32_t month_key = this->get_local_month_key_(segment_start);
    uint32_t segment_end = this->get_next_month_start_epoch_(segment_start);
    if (segment_end == 0 || segment_end > event.released_at_epoch) {
      segment_end = event.released_at_epoch;
    }
    if (segment_end <= segment_start) {
      break;
    }

    const uint32_t segment_seconds = segment_end - segment_start;
    uint64_t assigned_ms = static_cast<uint64_t>(event.hold_duration_ms) * segment_seconds / total_span_seconds;
    if (segment_end == event.released_at_epoch || assigned_ms > remaining_ms) {
      assigned_ms = remaining_ms;
    }

    this->add_held_duration_to_month_(month_key, assigned_ms);
    remaining_ms -= static_cast<uint32_t>(assigned_ms);
    segment_start = segment_end;
  }

  if (remaining_ms > 0) {
    this->add_held_duration_to_month_(this->get_local_month_key_(event.released_at_epoch), remaining_ms);
  }
}

void Esp32BluetoothFootPedalBleHid::add_held_duration_to_month_(uint32_t month_key, uint64_t duration_ms) {
  if (month_key == 0 || duration_ms == 0) {
    return;
  }

  if (this->current_month_key_ == month_key) {
    this->current_month_held_duration_ms_ += duration_ms;
    return;
  }
  if (this->previous_month_key_ == month_key) {
    this->previous_month_held_duration_ms_ += duration_ms;
    return;
  }
  if (this->current_month_key_ == 0 || month_key > this->current_month_key_) {
    this->previous_month_key_ = this->current_month_key_;
    this->previous_month_held_duration_ms_ = this->current_month_held_duration_ms_;
    this->current_month_key_ = month_key;
    this->current_month_held_duration_ms_ = duration_ms;
    return;
  }
  if (this->previous_month_key_ == 0 || month_key > this->previous_month_key_) {
    this->previous_month_key_ = month_key;
    this->previous_month_held_duration_ms_ = duration_ms;
  }
}

void Esp32BluetoothFootPedalBleHid::rotate_month_buckets_if_needed_() {
  uint32_t now_epoch = 0;
  if (!this->current_epoch_seconds_(&now_epoch)) {
    return;
  }

  const uint32_t now_month_key = this->get_local_month_key_(now_epoch);
  if (now_month_key == 0) {
    return;
  }
  if (this->current_month_key_ == 0) {
    this->current_month_key_ = now_month_key;
    this->previous_month_key_ = this->get_previous_month_key_(now_month_key);
    return;
  }
  if (now_month_key == this->current_month_key_) {
    return;
  }

  if (this->get_previous_month_key_(now_month_key) == this->current_month_key_) {
    this->previous_month_key_ = this->current_month_key_;
    this->previous_month_held_duration_ms_ = this->current_month_held_duration_ms_;
  } else {
    this->previous_month_key_ = this->get_previous_month_key_(now_month_key);
    this->previous_month_held_duration_ms_ = 0;
  }
  this->current_month_key_ = now_month_key;
  this->current_month_held_duration_ms_ = 0;
  this->persistence_dirty_ = true;
}

uint32_t Esp32BluetoothFootPedalBleHid::get_local_month_key_(uint32_t epoch) const {
  if (epoch == 0) {
    return 0;
  }

  std::time_t timestamp = static_cast<std::time_t>(epoch);
  std::tm local_tm{};
  if (localtime_r(&timestamp, &local_tm) == nullptr) {
    return 0;
  }
  return static_cast<uint32_t>((local_tm.tm_year + 1900) * 100 + (local_tm.tm_mon + 1));
}

uint32_t Esp32BluetoothFootPedalBleHid::get_previous_month_key_(uint32_t month_key) const {
  if (month_key == 0) {
    return 0;
  }
  uint32_t year = month_key / 100;
  uint32_t month = month_key % 100;
  if (month <= 1) {
    year -= 1;
    month = 12;
  } else {
    month -= 1;
  }
  return year * 100 + month;
}

uint32_t Esp32BluetoothFootPedalBleHid::get_next_month_start_epoch_(uint32_t epoch) const {
  if (epoch == 0) {
    return 0;
  }

  std::time_t timestamp = static_cast<std::time_t>(epoch);
  std::tm local_tm{};
  if (localtime_r(&timestamp, &local_tm) == nullptr) {
    return 0;
  }
  local_tm.tm_sec = 0;
  local_tm.tm_min = 0;
  local_tm.tm_hour = 0;
  local_tm.tm_mday = 1;
  local_tm.tm_mon += 1;
  std::time_t next_month = mktime(&local_tm);
  if (next_month < 0) {
    return 0;
  }
  return static_cast<uint32_t>(next_month);
}

bool Esp32BluetoothFootPedalBleHid::current_time_valid_() const {
  return this->time_source_ != nullptr && this->time_source_->now().is_valid();
}

bool Esp32BluetoothFootPedalBleHid::current_epoch_seconds_(uint32_t *epoch_seconds) const {
  if (epoch_seconds == nullptr || !this->current_time_valid_()) {
    return false;
  }
  auto now = this->time_source_->now();
  if (!now.is_valid()) {
    return false;
  }
  *epoch_seconds = static_cast<uint32_t>(now.timestamp);
  return true;
}

bool Esp32BluetoothFootPedalBleHid::clear_persistent_storage_() {
  if (!this->nvs_ready_) {
    return false;
  }

  nvs_handle_t handle;
  esp_err_t open_ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
  if (open_ret != ESP_OK) {
    ESP_LOGE(TAG, "Unable to open NVS for reset: %s", esp_err_to_name(open_ret));
    return false;
  }

  for (uint16_t page_index = 0; page_index < MAX_EVENT_PAGES; page_index++) {
    const std::string page_key = page_key_for_index(page_index);
    nvs_erase_key(handle, page_key.c_str());
  }
  nvs_erase_key(handle, NVS_META_KEY);
  const esp_err_t commit_ret = nvs_commit(handle);
  nvs_close(handle);
  if (commit_ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to clear persistent storage: %s", esp_err_to_name(commit_ret));
    return false;
  }
  return true;
}

std::string Esp32BluetoothFootPedalBleHid::format_local_timestamp_(uint32_t epoch) const {
  if (epoch == 0) {
    return "unknown";
  }

  std::time_t timestamp = static_cast<std::time_t>(epoch);
  std::tm local_tm{};
  if (localtime_r(&timestamp, &local_tm) == nullptr) {
    return "unknown";
  }

  char buffer[32];
  if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local_tm) == 0) {
    return "unknown";
  }
  return std::string(buffer);
}

uint32_t Esp32BluetoothFootPedalBleHid::now_ms_() {
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

}  // namespace esp32_bluetooth_foot_pedal_ble_hid
}  // namespace esphome

#include "sendspin_source.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_SOURCE)

#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdlib>
#include <cstring>

namespace esphome::sendspin_ {

static const char *const TAG = "sendspin.source";

void SendspinSource::setup() {
  // The hub's setup() has already run and added the role.
  this->source_role_ = this->parent_->get_source_role();
  if (this->source_role_ == nullptr) {
    ESP_LOGE(TAG, "The Sendspin source role is missing");
    this->mark_failed();
    return;
  }

  this->mic_source_->add_data_callback([this](const std::vector<uint8_t> &data) { this->on_audio_(data); });

  // Line sense needs the level all the time; otherwise capture only runs while the server streams.
  if (this->line_sense_) {
    this->mic_source_->start();
  }
}

void SendspinSource::loop() {
  if (this->line_sense_) {
    this->update_signal_();
  }
}

void SendspinSource::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Sendspin Source:\n"
                "  Line sense: %s\n"
                "  Signal threshold: %d (16 bit peak)\n"
                "  Signal off delay: %" PRIu32 " ms",
                YESNO(this->line_sense_), this->signal_threshold_, this->signal_off_delay_ms_);
}

// THREAD CONTEXT: Main loop
void SendspinSource::on_streaming_started() {
  ESP_LOGI(TAG, "Streaming to the server started");
  this->streaming_.store(true, std::memory_order_release);
  if (!this->line_sense_) {
    this->mic_source_->start();
  }
}

// THREAD CONTEXT: Main loop
void SendspinSource::on_streaming_stopped() {
  ESP_LOGI(TAG, "Streaming to the server stopped");
  this->streaming_.store(false, std::memory_order_release);
  if (!this->line_sense_) {
    this->mic_source_->stop();
  }
}

// THREAD CONTEXT: Microphone task
void SendspinSource::on_audio_(const std::vector<uint8_t> &data) {
  if (data.empty()) {
    return;
  }

  if (this->line_sense_ && !this->signal_seen_.load(std::memory_order_relaxed)) {
    const size_t samples = data.size() / sizeof(int16_t);
    for (size_t i = 0; i < samples; i++) {
      int16_t sample;
      std::memcpy(&sample, data.data() + i * sizeof(int16_t), sizeof(int16_t));
      if (std::abs(static_cast<int32_t>(sample)) >= this->signal_threshold_) {
        this->signal_seen_.store(true, std::memory_order_relaxed);
        break;
      }
    }
  }

  if (this->streaming_.load(std::memory_order_acquire)) {
    // 0 stamps the write as ending now. A refused write is dropped; the library resumes from live
    // capture once its buffer drains.
    this->source_role_->write_audio(data.data(), data.size(), 0);
  }
}

// THREAD CONTEXT: Main loop
void SendspinSource::update_signal_() {
  const uint32_t now = millis();
  if (this->signal_seen_.exchange(false, std::memory_order_relaxed)) {
    this->last_signal_ms_ = now;
    this->signal_present_ = true;
  } else if (this->signal_present_ && (now - this->last_signal_ms_) >= this->signal_off_delay_ms_) {
    this->signal_present_ = false;
  }

  if (!this->signal_reported_ || this->signal_present_ != this->reported_present_) {
    this->source_role_->set_signal(this->signal_present_ ? sendspin::SourceSignal::PRESENT
                                                         : sendspin::SourceSignal::ABSENT);
    ESP_LOGD(TAG, "Line input signal %s", this->signal_present_ ? "present" : "absent");
    this->reported_present_ = this->signal_present_;
    this->signal_reported_ = true;
  }
}

}  // namespace esphome::sendspin_

#endif  // USE_ESP_IDF && USE_SENDSPIN_SOURCE

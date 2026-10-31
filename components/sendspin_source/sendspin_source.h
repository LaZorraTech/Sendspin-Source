#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP_IDF) && defined(USE_SENDSPIN_SOURCE)

#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/sendspin/sendspin_hub.h"

#include <sendspin/source_role.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace esphome::sendspin_ {

/// @brief Streams audio from an ESPHome microphone to the Sendspin server through the source role.
///
/// Local addition, not part of ESPHome. The microphone's data callback runs on the microphone's
/// task and is the single producer for SourceRole::write_audio(). Everything else (starting and
/// stopping the microphone, reporting the line signal) runs on the main loop.
class SendspinSource final : public SendspinChild, public sendspin::SourceRoleListener {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_microphone_source(microphone::MicrophoneSource *mic_source) { this->mic_source_ = mic_source; }
  void set_line_sense(bool line_sense) { this->line_sense_ = line_sense; }
  void set_signal_threshold(int16_t threshold) { this->signal_threshold_ = threshold; }
  void set_signal_off_delay(uint32_t delay_ms) { this->signal_off_delay_ms_ = delay_ms; }

 protected:
  // --- SourceRoleListener overrides (main loop thread) ---
  void on_streaming_started() override;
  void on_streaming_stopped() override;

  /// @brief Microphone task: forwards audio to the source role and watches the level.
  void on_audio_(const std::vector<uint8_t> &data);

  void update_signal_();

  microphone::MicrophoneSource *mic_source_{nullptr};
  sendspin::SourceRole *source_role_{nullptr};

  // Written on the main loop, read on the microphone task.
  std::atomic<bool> streaming_{false};
  // Set on the microphone task when a buffer goes over the threshold, cleared on the main loop.
  std::atomic<bool> signal_seen_{false};

  uint32_t last_signal_ms_{0};
  uint32_t signal_off_delay_ms_{10000};
  int16_t signal_threshold_{103};  // about -50 dBFS
  bool line_sense_{true};
  bool signal_present_{false};
  bool reported_present_{false};  // Last state sent with set_signal()
  bool signal_reported_{false};   // Whether set_signal() has been called yet
};

}  // namespace esphome::sendspin_

#endif  // USE_ESP_IDF && USE_SENDSPIN_SOURCE

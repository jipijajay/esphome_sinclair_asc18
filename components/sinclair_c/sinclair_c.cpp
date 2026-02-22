#include "sinclair_c.h"
#include "protocol.h"

namespace esphome {
namespace sinclair_c {

void SinclairC::setup() {
  ESP_LOGI("sinclair_c", "Sinclair Type‑C UART initialized");
}

void SinclairC::loop() {
  while (available()) {
    uint8_t b = read();
    parse_byte(b);
  }
}

void SinclairC::parse_byte(uint8_t byte) {
  static bool in_frame = false;

  // Frame start detection
  if (!in_frame) {
    if (byte == 0x7E) {
      rx_buffer_.clear();
      rx_buffer_.push_back(byte);
      in_frame = true;
    }
    return;
  }

  rx_buffer_.push_back(byte);

  // Minimum frame length check
  if (rx_buffer_.size() < 5) return;

  // Detect end of frame by timeout or known lengths
  if (rx_buffer_.size() >= protocol::expected_length(rx_buffer_)) {
    if (validate_crc(rx_buffer_)) {
      process_frame(rx_buffer_);
    } else {
      ESP_LOGW("sinclair_c", "CRC failed");
    }
    in_frame = false;
  }
}

void SinclairC::process_frame(const std::vector<uint8_t> &frame) {
  uint8_t cmd = frame[2];

  switch (cmd) {
    case 0x82:
      protocol::parse_status_short(frame, this);
      break;

    case 0x83:
      protocol::parse_status_long(frame, this);
      break;

    case 0x8F:
      protocol::parse_diag(frame, this);
      break;

    default:
      ESP_LOGW("sinclair_c", "Unknown command: 0x%02X", cmd);
  }
}

ClimateTraits SinclairC::traits() {
  auto traits = climate::ClimateTraits();
  traits.set_supports_current_temperature(true);
  traits.set_supports_action(true);
  traits.set_supported_modes({
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_AUTO,
  });
  traits.set_supported_fan_modes({
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
    climate::CLIMATE_FAN_AUTO,
  });
  return traits;
}

void SinclairC::control(const ClimateCall &call) {
  protocol::build_control_frame(call, this);
}

}  // namespace sinclair_c
}  // namespace esphome

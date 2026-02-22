#pragma once

#include "esphome.h"

namespace esphome {
namespace sinclair_asc18 {

static const uint8_t HDR1 = 0x7E;
static const uint8_t HDR2 = 0x7E;

static const uint8_t FRAME_CTRL_LEN = 46;

enum FanLevel : uint8_t {
  FAN_AUTO     = 0x00,
  FAN_LOW      = 0x01,
  FAN_MID_LOW  = 0x02,
  FAN_MID      = 0x03,
  FAN_MID_HIGH = 0x04,
  FAN_HIGH     = 0x05,
  FAN_TURBO    = 0x06,
};

class SinclairAsc18Climate : public climate::Climate, public Component, public uart::UARTDevice {
 public:
  explicit SinclairAsc18Climate(uart::UARTComponent *parent) : UARTDevice(parent) {}

  void setup() override {
    this->target_temperature = 20;
    this->current_temperature = NAN;
    this->mode = climate::CLIMATE_MODE_AUTO;
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
  }

  void loop() override {
    while (available()) {
      uint8_t b = read();
      parse_byte_(b);
    }
  }

  climate::ClimateTraits traits() override {
    climate::ClimateTraits t;
    t.set_supports_current_temperature(true);
    t.set_visual_min_temperature(16);
    t.set_visual_max_temperature(30);
    t.set_visual_temperature_step(1.0f);

    t.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY,
      climate::CLIMATE_MODE_AUTO,
    });

    t.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MIDDLE_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_MIDDLE_HIGH,
      climate::CLIMATE_FAN_HIGH,
      climate::CLIMATE_FAN_DIFFUSE,  // Turbo
    });

    return t;
  }

  void control(const climate::ClimateCall &call) override {
    if (call.get_mode().has_value())
      this->mode = *call.get_mode();

    if (call.get_target_temperature().has_value())
      this->target_temperature = *call.get_target_temperature();

    if (call.get_fan_mode().has_value())
      this->fan_mode = *call.get_fan_mode();

    send_control_frame_();
    publish_state();
  }

 protected:
  uint8_t buf_[128];
  uint8_t pos_ = 0;
  bool in_frame_ = false;
  uint8_t last_ = 0;

  void parse_byte_(uint8_t b) {
    if (!in_frame_) {
      if (last_ == HDR1 && b == HDR2) {
        in_frame_ = true;
        pos_ = 0;
      }
      last_ = b;
      return;
    }

    buf_[pos_++] = b;

    if (pos_ >= 64) {
      process_status_frame_(buf_, pos_);
      in_frame_ = false;
      pos_ = 0;
    }
  }

  void process_status_frame_(uint8_t *f, uint8_t len) {
    if (len < 40) return;

    // Solltemperatur = Byte 0x1A (26)
    uint8_t t = f[0x1A];
    if (t >= 0x10 && t <= 0x1E)
      this->target_temperature = t;

    // Fan-Level = Byte 0x2D (45)
    uint8_t fan_raw = f[0x2D];
    this->fan_mode = fan_from_raw_(fan_raw);

    publish_state();
  }

  climate::ClimateFanMode fan_from_raw_(uint8_t r) {
    switch (r) {
      case FAN_LOW:      return climate::CLIMATE_FAN_LOW;
      case FAN_MID_LOW:  return climate::CLIMATE_FAN_MIDDLE_LOW;
      case FAN_MID:      return climate::CLIMATE_FAN_MEDIUM;
      case FAN_MID_HIGH: return climate::CLIMATE_FAN_MIDDLE_HIGH;
      case FAN_HIGH:     return climate::CLIMATE_FAN_HIGH;
      case FAN_TURBO:    return climate::CLIMATE_FAN_DIFFUSE;
      default:           return climate::CLIMATE_FAN_AUTO;
    }
  }

  uint8_t fan_to_raw_(climate::ClimateFanMode m) {
    switch (m) {
      case climate::CLIMATE_FAN_LOW:         return FAN_LOW;
      case climate::CLIMATE_FAN_MIDDLE_LOW:  return FAN_MID_LOW;
      case climate::CLIMATE_FAN_MEDIUM:      return FAN_MID;
      case climate::CLIMATE_FAN_MIDDLE_HIGH: return FAN_MID_HIGH;
      case climate::CLIMATE_FAN_HIGH:        return FAN_HIGH;
      case climate::CLIMATE_FAN_DIFFUSE:     return FAN_TURBO;
      default:                               return FAN_AUTO;
    }
  }

  uint8_t mode_to_raw_() {
    switch (this->mode) {
      case climate::CLIMATE_MODE_COOL:     return 0x01;
      case climate::CLIMATE_MODE_HEAT:     return 0x02;
      case climate::CLIMATE_MODE_DRY:      return 0x03;
      case climate::CLIMATE_MODE_FAN_ONLY: return 0x04;
      case climate::CLIMATE_MODE_AUTO:     return 0x11;
      default:                             return 0x00;
    }
  }

  void send_control_frame_() {
    uint8_t f[FRAME_CTRL_LEN] = {0};

    // Grundstruktur aus deinen 46‑Byte‑Frames
    f[0] = 0x00;
    f[1] = 0xFF;
    f[2] = 0x01;
    f[3] = 0x28;
    f[4] = 0x30;
    f[5] = 0x18;
    f[6] = 0x82;
    f[7] = 0x17;
    f[8] = 0x04;
    f[9] = 0xB2;

    // Temperatur
    f[0x17] = (uint8_t)this->target_temperature;

    // Mode
    f[0x16] = mode_to_raw_();

    // Fan
    f[FRAME_CTRL_LEN - 3] = fan_to_raw_(this->fan_mode);

    // Padding
    for (int i = 24; i < FRAME_CTRL_LEN - 3; i++)
      f[i] = 0x5F;

    // CRC (simple sum)
    uint16_t s = 0;
    for (int i = 0; i < FRAME_CTRL_LEN - 2; i++)
      s += f[i];

    f[FRAME_CTRL_LEN - 2] = s & 0xFF;
    f[FRAME_CTRL_LEN - 1] = (~s) & 0xFF;

    // Senden
    write(HDR1);
    write(HDR2);
    write_array(f, FRAME_CTRL_LEN);
    flush();
  }
};

}  // namespace sinclair_asc18
}  // namespace esphome

#pragma once

#include "esphome.h"

namespace esphome {
namespace sinclair_c {

class SinclairC : public Component, public Climate, public UARTDevice {
 public:
  explicit SinclairC(UARTComponent *parent) : UARTDevice(parent) {}

  void setup() override;
  void loop() override;

  // Climate API
  ClimateTraits traits() override;
  void control(const ClimateCall &call) override;

 protected:
  void parse_byte(uint8_t byte);
  void process_frame(const std::vector<uint8_t> &frame);
  bool validate_crc(const std::vector<uint8_t> &frame);

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_frame_ts_{0};
};

}  // namespace sinclair_c
}  // namespace esphome

#pragma once

#include "esphome.h"

namespace esphome {
namespace sinclair_c {
namespace protocol {

size_t expected_length(const std::vector<uint8_t> &frame);

bool parse_status_short(const std::vector<uint8_t> &frame, Climate *cl);
bool parse_status_long(const std::vector<uint8_t> &frame, Climate *cl);
bool parse_diag(const std::vector<uint8_t> &frame, Climate *cl);

void build_control_frame(const ClimateCall &call, UARTDevice *dev);

uint16_t crc16(const uint8_t *data, size_t len);

}  // namespace protocol
}  // namespace sinclair_c
}  // namespace esphome

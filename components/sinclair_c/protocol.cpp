#include "protocol.h"

namespace esphome {
namespace sinclair_c {
namespace protocol {

size_t expected_length(const std::vector<uint8_t> &frame) {
  uint8_t cmd = frame[2];

  switch (cmd) {
    case 0x82: return 59;
    case 0x83: return 134;
    case 0x8F: return 133;
    default: return 0;
  }
}

bool parse_status_short(const std::vector<uint8_t> &frame, Climate *cl) {
  // Beispiel: Temperatur extrahieren
  uint8_t temp = frame[14];
  cl->current_temperature = temp;

  // Modus
  uint8_t mode = frame[20];
  switch (mode) {
    case 0x00: cl->mode = climate::CLIMATE_MODE_OFF; break;
    case 0x01: cl->mode = climate::CLIMATE_MODE_COOL; break;
    case 0x02: cl->mode = climate::CLIMATE_MODE_HEAT; break;
    case 0x03: cl->mode = climate::CLIMATE_MODE_AUTO; break;
  }

  cl->publish_state();
  return true;
}

bool parse_status_long(const std::vector<uint8_t> &frame, Climate *cl) {
  // Erweiterte Sensorwerte
  return true;
}

bool parse_diag(const std::vector<uint8_t> &frame, Climate *cl) {
  return true;
}

void build_control_frame(const ClimateCall &call, UARTDevice *dev) {
  std::vector<uint8_t> frame = {
    0x7E, 0x7E, 0x2F, 0x01,
    // Payload folgt
  };

  // Temperatur setzen
  if (call.get_target_temperature().has_value()) {
    uint8_t t = (uint8_t) call.get_target_temperature().value();
    frame.push_back(t);
  }

  // CRC
  uint16_t crc = crc16(frame.data(), frame.size());
  frame.push_back(crc & 0xFF);
  frame.push_back(crc >> 8);

  dev->write_array(frame.data(), frame.size());
}

uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc >>= 1;
    }
  }
  return crc;
}

}  // namespace protocol
}  // namespace sinclair_c
}  // namespace esphome

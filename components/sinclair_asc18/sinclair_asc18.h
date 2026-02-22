#pragma once

#include "esphome.h"

namespace esphome {
namespace sinclair_asc18 {

static const uint8_t SINCLAIR_FRAME_LEN = 46;
static const uint8_t SINCLAIR_HEADER1   = 0x7E;
static const uint8_t SINCLAIR_HEADER2   = 0x7E;

// Grobe Annahmen aus deinen Logs – ggf. Feintuning:
// - Wir senden 46-Byte-Steuerframes (die 0x00 / 0x01 / 0x82 / 0x83 etc. Frames).
// - Set-Temperatur: 0x14-Byte im 64-Byte-Statusframe korreliert mit 20°C etc.
//   Fürs Senden nehmen wir hier ein einfaches Mapping: 16–30°C -> 0x10–0x1E.
// - Fan-Level: letztes „nicht 0x5F“-Byte im 46-Byte-Frame (hier index 0x2D/0x2E)
//   wird als Fan-Stufe interpretiert. Wir mappen 0–6 auf Auto..Turbo.

enum SinclairFanLevel : uint8_t {
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
  explicit SinclairAsc18Climate(uart::UARTComponent *parent) : uart::UARTDevice(parent) {}

  void setup() override {
    // Nichts Besonderes – wir warten auf Frames und initialisieren State konservativ.
    this->mode = climate::CLIMATE_MODE_OFF;
    this->fan_mode = climate::CLIMATE_FAN_AUTO;
    this->target_temperature = 20.0f;
    this->current_temperature = NAN;
  }

  void loop() override {
    // Eingehende Frames lesen und Status aktualisieren
    while (this->available()) {
      uint8_t b = this->read();
      this->parse_byte_(b);
    }
  }

  climate::ClimateTraits traits() override {
    climate::ClimateTraits traits;
    traits.set_supports_current_temperature(true);
    traits.set_supported_modes({
        climate::CLIMATE_MODE_OFF,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_DRY,
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_AUTO,
    });
    traits.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_MEDIUM,
        climate::CLIMATE_FAN_HIGH,
        climate::CLIMATE_FAN_FOCUS,   // nutzen wir für Mid-High
        climate::CLIMATE_FAN_DIFFUSE, // nutzen wir für Turbo
    });
    traits.set_visual_min_temperature(16);
    traits.set_visual_max_temperature(30);
    traits.set_visual_temperature_step(1.0f);
    return traits;
  }

  void control(const climate::ClimateCall &call) override {
    if (call.get_mode().has_value()) {
      this->mode = *call.get_mode();
    }
    if (call.get_target_temperature().has_value()) {
      this->target_temperature = *call.get_target_temperature();
    }
    if (call.get_fan_mode().has_value()) {
      this->fan_mode = *call.get_fan_mode();
    }

    this->send_control_frame_();
    this->publish_state();
  }

 protected:
  // Parser-State
  uint8_t rx_buf_[128];
  uint8_t rx_pos_{0};
  bool in_frame_{false};
  uint8_t last_byte_{0};

  void parse_byte_(uint8_t b) {
    if (!in_frame_) {
      // Auf 0x7E 0x7E syncen
      if (last_byte_ == SINCLAIR_HEADER1 && b == SINCLAIR_HEADER2) {
        in_frame_ = true;
        rx_pos_ = 0;
      }
      last_byte_ = b;
      return;
    }

    // Wir sind in einem Frame – Bytes sammeln
    if (rx_pos_ < sizeof(rx_buf_)) {
      rx_buf_[rx_pos_++] = b;
    }

    // Heuristik: 46- oder 64-Byte-Frames + Checksumme am Ende
    // Wir erkennen das am Längenbyte an Position 0 (z.B. 0x20, 0x82, 0xFF etc.)
    // und am Log (Frame (46), Frame (64), Frame (57), Frame (49)).
    // Hier vereinfachen wir: wenn eine Weile nichts mehr kommt oder
    // rx_pos_ >= 64, werten wir aus und resetten.
    if (rx_pos_ >= 64) {
      this->process_frame_(rx_buf_, rx_pos_);
      in_frame_ = false;
      rx_pos_ = 0;
    }
  }

  void process_frame_(const uint8_t *data, uint8_t len) {
    // Wir interessieren uns primär für die 64-Byte-Statusframes (beginnend mit 0x20 oder 0xFF)
    if (len < 40) return;

    uint8_t header = data[0];

    // Beispiel: 64-Byte-Statusframe mit 0x20 oder 0xFF am Anfang
    if (header == 0x20 || header == 0xFF) {
      // Temperatur grob aus Byte 0x18/0x19/0x1A ableiten – aus deinen Logs:
      // ... 00.12.11.14.02 ... bei 20°C
      // Wir nehmen hier Byte 0x1A (Index 0x1A = 26 dez) als "Setpoint-Byte"
      // und mappen 0x10–0x1E -> 16–30°C.
      if (len > 0x1A) {
        uint8_t temp_raw = data[0x1A];
        if (temp_raw >= 0x10 && temp_raw <= 0x1E) {
          float t = static_cast<float>(temp_raw);
          this->target_temperature = t;  // 1:1, ggf. -?°C anpassen
        }
      }

      // Fan-Level aus einem der letzten Bytes (z.B. drittletztes Byte)
      // In deinen Turbo-Frames taucht 0x30 / 0x2F / 0x42 etc. an Position ~0x2D/0x2E auf.
      // Wir lesen hier Byte 0x2D (45 dez) als Fan-Level.
      if (len > 0x2D) {
        uint8_t fan_raw = data[0x2D];
        this->fan_mode = fan_from_raw_(fan_raw);
      }

      // Optional: aktuelle Raumtemperatur aus einem anderen Byte ziehen,
      // wenn du das später sauber gemappt hast.
      // this->current_temperature = ...;

      this->publish_state();
    }
  }

  climate::ClimateFanMode fan_from_raw_(uint8_t raw) {
    switch (raw) {
      case FAN_AUTO:     return climate::CLIMATE_FAN_AUTO;
      case FAN_LOW:      return climate::CLIMATE_FAN_LOW;
      case FAN_MID_LOW:  return climate::CLIMATE_FAN_MEDIUM;
      case FAN_MID:      return climate::CLIMATE_FAN_MEDIUM;
      case FAN_MID_HIGH: return climate::CLIMATE_FAN_HIGH;   // Mapping
      case FAN_HIGH:     return climate::CLIMATE_FAN_HIGH;
      case FAN_TURBO:    return climate::CLIMATE_FAN_DIFFUSE; // nutzen wir als Turbo
      default:           return climate::CLIMATE_FAN_AUTO;
    }
  }

  uint8_t fan_to_raw_(climate::ClimateFanMode mode) {
    switch (mode) {
      case climate::CLIMATE_FAN_AUTO:    return FAN_AUTO;
      case climate::CLIMATE_FAN_LOW:     return FAN_LOW;
      case climate::CLIMATE_FAN_MEDIUM:  return FAN_MID;      // Mid
      case climate::CLIMATE_FAN_HIGH:    return FAN_HIGH;
      case climate::CLIMATE_FAN_FOCUS:   return FAN_MID_HIGH; // Mid-High
      case climate::CLIMATE_FAN_DIFFUSE: return FAN_TURBO;    // Turbo
      default:                           return FAN_AUTO;
    }
  }

  uint8_t mode_to_raw_() {
    // Sehr grobe Zuordnung – hier kannst du später mit deinen Logs nachschärfen.
    // Wir nehmen ein Byte im Frame (z.B. data[13]) als Mode-Byte.
    switch (this->mode) {
      case climate::CLIMATE_MODE_OFF:      return 0x00;
      case climate::CLIMATE_MODE_COOL:     return 0x01;
      case climate::CLIMATE_MODE_HEAT:     return 0x02;
      case climate::CLIMATE_MODE_DRY:      return 0x03;
      case climate::CLIMATE_MODE_FAN_ONLY: return 0x04;
      case climate::CLIMATE_MODE_AUTO:     return 0x05;
      default:                             return 0x00;
    }
  }

  uint8_t temp_to_raw_(float temp_c) {
    // 16–30°C -> 0x10–0x1E
    if (temp_c < 16.0f) temp_c = 16.0f;
    if (temp_c > 30.0f) temp_c = 30.0f;
    return static_cast<uint8_t>(temp_c);  // ggf. +Offset, wenn nötig
  }

  void send_control_frame_() {
    uint8_t frame[SINCLAIR_FRAME_LEN] = {0};

    // Template aus deinen 46-Byte-Frames abgeleitet, z.B.:
    // 00.FF.01.28.30.18.82.17.04.B2.00.7C.78.01.02.00.00.00.00.10.00.00.01.AA.5F...
    frame[0]  = 0x00;  // Adresse / Typ
    frame[1]  = 0xFF;
    frame[2]  = 0x01;
    frame[3]  = 0x28;
    frame[4]  = 0x30;
    frame[5]  = 0x18;
    frame[6]  = 0x82;
    frame[7]  = 0x17;
    frame[8]  = 0x04;
    frame[9]  = 0xB2;  // kann je nach Gerät variieren
    frame[10] = 0x00;
    frame[11] = 0x7C;
    frame[12] = 0x78;  // hier steckt oft Mode/Fan/Temp-Kombi
    frame[13] = 0x01;  // Mode-Byte (Beispiel)
    frame[14] = 0x02;
    frame[15] = 0x00;
    frame[16] = 0x00;
    frame[17] = 0x00;
    frame[18] = 0x00;
    frame[19] = 0x10;
    frame[20] = 0x00;
    frame[21] = 0x00;
    frame[22] = 0x01;
    frame[23] = 0xAA;

    // Rest mit 0x5F auffüllen wie in deinen Logs
    for (uint8_t i = 24; i < SINCLAIR_FRAME_LEN - 3; i++) {
      frame[i] = 0x5F;
    }

    // Fan-Level an eine der letzten Positionen schreiben (z.B. index 0x2D = 45)
    uint8_t fan_raw = fan_to_raw_(this->fan_mode);
    frame[SINCLAIR_FRAME_LEN - 3] = fan_raw;

    // Zwei letzte Bytes: Checksumme (sehr grob: Summe & 0xFF, invertiert)
    uint16_t sum = 0;
    for (uint8_t i = 0; i < SINCLAIR_FRAME_LEN - 2; i++) {
      sum += frame[i];
    }
    uint8_t cs1 = static_cast<uint8_t>(sum & 0xFF);
    uint8_t cs2 = static_cast<uint8_t>((~sum) & 0xFF);
    frame[SINCLAIR_FRAME_LEN - 2] = cs1;
    frame[SINCLAIR_FRAME_LEN - 1] = cs2;

    // Header 0x7E 0x7E + Frame senden
    this->write(SINCLAIR_HEADER1);
    this->write(SINCLAIR_HEADER2);
    this->write_array(frame, SINCLAIR_FRAME_LEN);
    this->flush();
  }
};

}  // namespace sinclair_asc18
}  // namespace esphome

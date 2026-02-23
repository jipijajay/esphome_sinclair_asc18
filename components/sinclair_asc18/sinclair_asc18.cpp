#include "sinclair_asc18.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sinclair_asc18 {

static const char *const TAG = "sinclair_asc18";

void SinclairASC18Climate::setup() {
  ESP_LOGI(TAG, "Sinclair ASC-18 climate setup");
}

climate::ClimateTraits SinclairASC18Climate::traits() {
  climate::ClimateTraits traits;

  traits.set_supports_current_temperature(false);
  traits.set_supports_two_point_target_temperature(false);
  traits.set_supports_action(false);

  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_AUTO,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY,
  });

  traits.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,     // low
      climate::CLIMATE_FAN_MEDIUM,  // mid
      climate::CLIMATE_FAN_HIGH,    // high/turbo (feineres Mapping später)
  });

  traits.set_visual_min_temperature(16);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(1.0f);

  return traits;
}

void SinclairASC18Climate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    this->mode_ = *call.get_mode();
    this->mode = this->mode_;
  }

  if (call.get_fan_mode().has_value()) {
    this->fan_mode_ = *call.get_fan_mode();
    this->fan_mode = this->fan_mode_;
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
  }

  this->power_ = (this->mode_ != climate::CLIMATE_MODE_OFF);

  this->send_state_to_ac_();
  this->publish_state();
}

void SinclairASC18Climate::loop() {
  this->handle_incoming_();
}

void SinclairASC18Climate::handle_incoming_() {
  // Platzhalter: hier kannst du später dein vollständiges Frame-Parsing einbauen.
  while (this->available()) {
    uint8_t b = this->read();
    if (b == 0x7E) {
      ESP_LOGV(TAG, "Got 0x7E from AC (frame boundary)");
    }
  }
}

void SinclairASC18Climate::send_state_to_ac_() {
  // Aktuell: Dummy-Frame, damit alles kompiliert und UART aktiv ist.
  // Später: hier deine echte Frame-Struktur aus den Dumps nachbauen.

  uint8_t frame[8];

  frame[0] = 0x20;  // Dummy-Header

  // Mode-Mapping (nur Beispiel!)
  switch (this->mode_) {
    case climate::CLIMATE_MODE_COOL:
      frame[1] = 0x01;
      break;
    case climate::CLIMATE_MODE_HEAT:
      frame[1] = 0x02;
      break;
    case climate::CLIMATE_MODE_DRY:
      frame[1] = 0x03;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      frame[1] = 0x04;
      break;
    case climate::CLIMATE_MODE_AUTO:
      frame[1] = 0x00;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      frame[1] = 0x10;  // z.B. Power-Off-Flag – später anpassen
      break;
  }

  // Fan-Mapping (Auto, Low, Mid Low, Mid, Mid High, High, Turbo)
  // Hier grob auf 0..6 – später mit deinen Logs exakt mappen.
  uint8_t fan_code = 0x00;
  switch (this->fan_mode_) {
    case climate::CLIMATE_FAN_LOW:
      fan_code = 0x01;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_code = 0x03;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_code = 0x05;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_code = 0x00;
      break;
  }
  frame[2] = fan_code;

  uint8_t temp = static_cast<uint8_t>(this->target_temperature);
  frame[3] = temp;

  frame[4] = 0x00;
  frame[5] = 0x00;
  frame[6] = 0x00;

  uint8_t sum = 0;
  for (int i = 0; i < 7; i++) {
    sum += frame[i];
  }
  frame[7] = sum;

  ESP_LOGD(TAG, "Sending dummy frame: mode=%d fan=%d temp=%d",
           this->mode_, this->fan_mode_, temp);

  this->write_array(frame, sizeof(frame));
}

}  // namespace sinclair_asc18
}  // namespace esphome


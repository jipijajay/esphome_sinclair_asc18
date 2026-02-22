#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"

namespace esphome {
namespace sinclair_asc18 {

class SinclairASC18Climate : public climate::Climate,
                             public uart::UARTDevice,
                             public Component {
 public:
  void setup() override;
  void loop() override;
  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

  void set_uart_parent(uart::UARTComponent *parent) { this->set_parent(parent); }

 protected:
  void send_state_to_ac_();
  void handle_incoming_();

  bool power_{true};
  climate::ClimateMode mode_{climate::CLIMATE_MODE_COOL};
  climate::ClimateFanMode fan_mode_{climate::CLIMATE_FAN_AUTO};
};

}  // namespace sinclair_asc18
}  // namespace esphome

#include "esphome.h"
#include "IRremoteESP8266.h"
#include "IRsend.h"
#include "ir_Daikin.h"

// change this to 0 if you don't care about the clock on the AC
// if this is 1, a time component with id "esptime" needs to be available
// timezone needs to be correct
// you can change the id by replacing any mentions of it to a new one
// Example:
//
// time:
//   - platform: homeassistant
//     timezone: Europe/London
//     id: esptime
#define UPDATE_CLOCK 1

// pin connected to IR LED
// best to get those modules with 2 leds
// It should have 3 pins: VCC -> 5V, GND -> GND, DATA -> kIrLed pin
// This is the GPIO pin number, may differ from the one printed on the board
// GPIO pin does not have enough power to drive IR LED, use it as a switch only
const uint16_t kIrLed = 4;

// Change protocol here
// See: https://crankyoldgit.github.io/IRremoteESP8266/doxygen/html/annotated.html
// as Daikin64 is toggle rather than on/off, a few places was changed
// Orginal: https://github.com/esphome/feature-requests/issues/1054#issuecomment-765096913
// Remember to change kDaikin64* too, see https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Daikin.h for defined consts
IRDaikin64 ac(kIrLed);


class DaikinAC : public Component, public Climate {
  public:
    sensor::Sensor *sensor_{nullptr};

    void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
// Dont touch, magic
    void setup() override
    {
      if (this->sensor_) {
        this->sensor_->add_on_state_callback([this](float state) {
          this->current_temperature = state;
          this->publish_state();
        });
        this->current_temperature = this->sensor_->state;
      } else {
        this->current_temperature = NAN;
      }

      auto restore = this->restore_state_();
      if (restore.has_value()) {
        restore->apply(this);
      } else {
        this->mode = climate::CLIMATE_MODE_OFF;
        // TODO: roundf function doesnt exist? replace with proper one?
        // this->target_temperature = roundf(clamp(this->current_temperature, 16, 30));
        this->target_temperature = roundf(this->current_temperature);
        this->fan_mode = climate::CLIMATE_FAN_AUTO;
        this->swing_mode = climate::CLIMATE_SWING_OFF;
      }

      if (isnan(this->target_temperature)) {
        this->target_temperature = 25;
      }

      ac.begin();

      ESP_LOGD("DEBUG", "Daikin A/C remote is in the following state:");
      ESP_LOGD("DEBUG", "  %s\n", ac.toString().c_str());
    }

// Set capabilities of AC here, report to home assisatnt
    climate::ClimateTraits traits() {
      auto traits = climate::ClimateTraits();
      traits.set_supports_current_temperature(this->sensor_ != nullptr);
      // refactored because of deprecation notice
      // see: https://esphome.io/api/classesphome_1_1climate_1_1_climate_traits
      traits.set_supported_modes( {CLIMATE_MODE_OFF, CLIMATE_MODE_COOL, CLIMATE_MODE_FAN_ONLY, CLIMATE_MODE_DRY} );
      traits.set_supported_fan_modes( {CLIMATE_FAN_AUTO, CLIMATE_FAN_LOW, CLIMATE_FAN_MEDIUM, CLIMATE_FAN_HIGH} );
      traits.set_supported_swing_modes( {CLIMATE_SWING_OFF, CLIMATE_SWING_VERTICAL} );
      
      traits.set_supports_two_point_target_temperature(false);
      // possible max/min temp, and step
      traits.set_visual_max_temperature(30);
      traits.set_visual_min_temperature(16);
      traits.set_visual_temperature_step(1);

      return traits;
    }

  void control(const ClimateCall &call) override {
    // assume no need to toggle
    ac.setPowerToggle(0);
    if (call.get_mode().has_value()) {
      ClimateMode mode = *call.get_mode();
      if (mode == CLIMATE_MODE_OFF) {
        // if ac state is currently on, then toggle to be off
        if (id(ac_status).state)
          ac.setPowerToggle(1);
      } else if (mode == CLIMATE_MODE_COOL) {
        // if ac state is currently off, toggle
        // else we dont need to toggle
        if (!id(ac_status).state)
          ac.setPowerToggle(1);
        ac.setMode(kDaikin64Cool);
      } else if (mode == CLIMATE_MODE_DRY) {
        if (!id(ac_status).state)
          ac.setPowerToggle(1);
        ac.setMode(kDaikin64Dry);
      } else if (mode == CLIMATE_MODE_FAN_ONLY) {
        if (!id(ac_status).state)
          ac.setPowerToggle(1);
        ac.setMode(kDaikin64Fan);
      }
      this->mode = mode;
    }

    if (call.get_target_temperature().has_value()) {
      float temp = *call.get_target_temperature();
      ac.setTemp(temp);
      this->target_temperature = temp;
    }

    if (call.get_fan_mode().has_value()) {
      ClimateFanMode fan_mode = *call.get_fan_mode();
      if (fan_mode == CLIMATE_FAN_AUTO) {
        ac.setFan(kDaikin64FanAuto);
      } else if (fan_mode == CLIMATE_FAN_LOW) {
        ac.setFan(kDaikin64FanLow);
      } else if (fan_mode == CLIMATE_FAN_MEDIUM) {
        ac.setFan(kDaikin64FanMed);
      } else if (fan_mode == CLIMATE_FAN_HIGH) {
        ac.setFan(kDaikin64FanHigh);
      }
      this->fan_mode = fan_mode;
    }

    if (call.get_swing_mode().has_value()) {
      ClimateSwingMode swing_mode = *call.get_swing_mode();
      if (swing_mode == CLIMATE_SWING_OFF) {
        ac.setSwingVertical(false);
      } else if (swing_mode == CLIMATE_SWING_VERTICAL) {
        ac.setSwingVertical(true);
      }
      this->swing_mode = swing_mode;
    }

#if UPDATE_CLOCK
    // time in minutes past midnight. Dont wanna define yet another var
    ac.setClock(id(esptime).now().hour * 60 + id(esptime).now().minute);
#endif

    // a hack to transfer on/off state
    // since we cant set humidity via front-end, and
    // if it has this magic humidity value (0.78125%) it must be sent by us
    // Yes this is the actual value in float from IEEE754

    if (call.get_target_humidity().has_value()) {
      float humid = *call.get_target_humidity();
      if (humid == 0.0078125) {
        // just publish the state, dont send
        ESP_LOGD("DEBUG", "Updated Daikin A/C status without sending");
      } else { ac.send();}
    } else { ac.send();}

//    ac.send();

    this->publish_state();

    ESP_LOGD("DEBUG", "Daikin A/C remote is in the following state:");
    ESP_LOGD("DEBUG", "  %s\n", ac.toString().c_str());
  }
};

# esphome-daikin64
Integrating `daikin64` protocol with esphome to control daikin air conditioners.

This is how I integrated the `daikin64` IR protocol to esphome, since the official repo doesn't support it.
It has been tested on a esp8266. Other daikin protocols could possibily be implemented using a similar approach, such as `DAIKIN128`, `DAIKIN176`, `DAIKIN152`, etc.

## How to use
1. Find where esphome stores the yaml files for each of the other nodes
2. Create a directory named `include`
3. Place the `irdaikin.h` inside
4. Copy some or all of the `esphome.yaml` configs and compile.

## Configurations
### Different protocols
Change the protocol by changing the `IRDaikin64 ac(kIrLed);` line in the `.h` file. See comments for details.

### Using a proper state sensor for the AC
Delete everything under the `on_state` trigger. Remember to set the sensor's id to `ac_status`.

### Don't have a temperature sensor
Remove this line. If you don't define it, then it would default to a `null_ptr` and it would not report the ability to sense current temperature.
```
daikinac->set_sensor(id(living_room_temperature));
```

### I don't care about the internal clock of the air-conditioner
Replace the `1` to `0` in the `.h` file.
```
#define UPDATE_CLOCK 1
``` 

### Change the IR LED pin
Modify this line in the `.h` file. See comments for details.
```
const uint16_t kIrLed = 4;
```

## Special Thanks

- @iglu-sebastian for the [orginal version](https://github.com/esphome/feature-requests/issues/1054#issuecomment-765096913) and the idea
- @computingwithinfinitedata for the [orginal method and approach](https://github.com/esphome/feature-requests/issues/444#issuecomment-548166019)
- @crankyoldgit for the [IRremoteESP8266 library](https://github.com/crankyoldgit/IRremoteESP8266)

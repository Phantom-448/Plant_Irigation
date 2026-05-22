# Profile JSON Examples

This folder contains example profile files for the Smart Plant Irrigation system.

## How to use

1. Copy one of the `.json` files to the SD card profile folder:
   * `/sdcard/profiles/`
2. Make sure the file name ends with `.json`.
3. Restart the ESP32 or use the web dashboard to reload available profiles.
4. Select the profile name without the `.json` extension.

## Example fields

Each profile file supports the following fields:

* `check_interval_minutes` - sensor polling interval in minutes
* `base_watering_minutes` - base pump run time in minutes
* `min_soil_moisture_percent` - minimum soil moisture threshold in percent
* `hot_temp_threshold` - temperature threshold in degrees Celsius for extra watering
* `hot_temp_extra_minutes` - extra watering minutes when temperature is above threshold

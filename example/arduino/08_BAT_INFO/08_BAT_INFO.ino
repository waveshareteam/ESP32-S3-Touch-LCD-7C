#include "src/i2c/i2c.h"
#include "src/io_extension/io_extension.h"
#include "src/bat/battery_info.h"

#define BATTERY_CAPACITY_MAH 1500

void setup()
{
    BatteryStatus bat_sta;

    Serial.begin(115200);
    DEV_I2C_Init();
    delay(100);

    bat_sta = battery_init(BATTERY_CAPACITY_MAH);

    IO_EXTENSION_Init();
    display_init();

    if (bat_sta == BATTERY_NOT_CONNECT) {
        display_show_not_connect();
    } else if (bat_sta != BATTERY_OK) {
        display_show_error(battery_status_to_string(bat_sta));
    }
}

void loop()
{
    BatteryInfo bat_info;
    BatteryStatus bat_sta;

    bat_sta = battery_get_info(&bat_info);
    if (bat_sta == BATTERY_OK) {
        display_show_info(&bat_info);
    } else if (bat_sta == BATTERY_NOT_CONNECT) {
        display_show_not_connect();
    } else {
        display_show_error(battery_status_to_string(bat_sta));
    }
    delay(1000);
}

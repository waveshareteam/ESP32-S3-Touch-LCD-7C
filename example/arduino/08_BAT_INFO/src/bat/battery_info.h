#ifndef BATTERY_INFO_H
#define BATTERY_INFO_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t mv;        // 电压（mV）
    int16_t ma;         // 电流（mA），正值=充电，负值=放电
    uint16_t soc;       // 剩余电量百分比（%）
    uint16_t tte;       // 预计还能用多久（min）
    uint16_t ttf;       // 预计充满还需多久（min）
    uint16_t mah_rem;   // 剩余容量（mAh）
    uint16_t mah_fcc;   // 满充容量（mAh）
    uint16_t cycles;    // 循环次数（cycle count）
    uint16_t tc;        // 温度（°C）
} BatteryInfo;

typedef enum {
    BATTERY_OK = 0,
    BATTERY_INIT_FAILED,
    BATTERY_NOT_CONNECT,
} BatteryStatus;

BatteryStatus battery_init(uint16_t capacity_mah);
BatteryStatus battery_get_info(BatteryInfo *battery_info);
const char *battery_status_to_string(BatteryStatus status);
esp_err_t display_init(void);
void display_show_info(const BatteryInfo *battery_info);
void display_show_error(const char *message);
void display_show_not_connect(void);

#ifdef __cplusplus
}
#endif

#endif

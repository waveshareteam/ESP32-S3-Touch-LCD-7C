#include "battery_info.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Arduino.h"
#include "bq27220/bq27220.h"
#include "esp_log.h"
#include "../i2c/i2c.h"
#include "../rgb_lcd_port/gui_paint/gui_paint.h"
#include "../rgb_lcd_port/rgb_lcd_port.h"

#define BATTERY_TIME_INVALID 0xFFFF
#define BATTERY_RETRY_DELAY_MS 1500
#define DISPLAY_X 20
#define DISPLAY_Y 20
#define DISPLAY_LINE_STEP 40
#define DISPLAY_INFO_COLOR BLACK
#define DISPLAY_ERROR_COLOR RED

static const char *battery_tag = "battery_info";
static bq27220_handle_t battery_gauge = NULL;
static BatteryStatus battery_status = BATTERY_INIT_FAILED;
static uint16_t battery_capacity_mah = 0;
static UBYTE *display_image = NULL;
static battery_status_t battery_gauge_status = {};
static bool battery_need_init = false;
static uint32_t battery_retry_deadline_ms = 0;

static const parameter_cedv_t default_cedv = {
    .full_charge_cap = 650,
    .design_cap = 650,
    .reserve_cap = 0,
    .near_full = 200,
    .self_discharge_rate = 20,
    .EDV0 = 3490,
    .EDV1 = 3511,
    .EDV2 = 3535,
    .EMF = 3670,
    .C0 = 115,
    .R0 = 968,
    .T0 = 4547,
    .R1 = 4764,
    .TC = 11,
    .C1 = 0,
    .DOD0 = 4147,
    .DOD10 = 4002,
    .DOD20 = 3969,
    .DOD30 = 3938,
    .DOD40 = 3880,
    .DOD50 = 3824,
    .DOD60 = 3794,
    .DOD70 = 3753,
    .DOD80 = 3677,
    .DOD90 = 3574,
    .DOD100 = 3490,
};

static const gauging_config_t default_gauging_config = {
    .CCT = 1,
    .CSYNC = 0,
    .EDV_CMP = 0,
    .SC = 1,
    .FIXED_EDV0 = 0,
    .FCC_LIM = 1,
    .FC_FOR_VDQ = 1,
    .IGNORE_SD = 1,
    .SME0 = 0,
};

/**************** battery_service ****************/

static int16_t temp_to_c(uint16_t temp_0_1k)
{
    return (int16_t)(((int32_t)temp_0_1k - 2731) / 10);
}

static void battery_release_gauge(void)
{
    if (battery_gauge != NULL) {
        bq27220_delete(battery_gauge);
        battery_gauge = NULL;
    }
}

static BatteryStatus battery_init_failed(void)
{
    battery_release_gauge();
    memset(&battery_gauge_status, 0, sizeof(battery_gauge_status));
    battery_need_init = true;
    battery_retry_deadline_ms = millis() + BATTERY_RETRY_DELAY_MS;
    battery_status = BATTERY_INIT_FAILED;
    return battery_status;
}

static BatteryStatus battery_not_connect(void)
{
    battery_status = BATTERY_NOT_CONNECT;
    return battery_status;
}

static BatteryStatus battery_handle_comm_error(void)
{
    battery_release_gauge();
    memset(&battery_gauge_status, 0, sizeof(battery_gauge_status));
    battery_need_init = true;

    battery_retry_deadline_ms = millis() + BATTERY_RETRY_DELAY_MS;
    battery_status = BATTERY_INIT_FAILED;
    return battery_status;
}

static bool battery_read_u16_checked(uint16_t *value, uint16_t (*reader)(bq27220_handle_t))
{
    *value = reader(battery_gauge);
    return bq27220_get_last_error() == ESP_OK;
}

static bool battery_read_i16_checked(int16_t *value, int16_t (*reader)(bq27220_handle_t))
{
    *value = reader(battery_gauge);
    return bq27220_get_last_error() == ESP_OK;
}

BatteryStatus battery_init(uint16_t capacity_mah)
{
    parameter_cedv_t cedv = default_cedv;
    bq27220_config_t config = {};
    battery_status_t gauge_status = {};

    battery_capacity_mah = capacity_mah;
    battery_status = BATTERY_INIT_FAILED;

    battery_release_gauge();
    memset(&battery_gauge_status, 0, sizeof(battery_gauge_status));

    if (capacity_mah > 0) {
        cedv.full_charge_cap = capacity_mah;
        cedv.design_cap = capacity_mah;
        if (cedv.near_full > capacity_mah) {
            cedv.near_full = capacity_mah;
        }
    }

    config.i2c_bus = (i2c_bus_handle_t)DEV_I2C_Get_Bus_Device();
    config.cfg = &default_gauging_config;
    config.cedv = &cedv;
    if (config.i2c_bus == NULL) {
        ESP_LOGE(battery_tag, "i2c bus not ready");
        return battery_status;
    }

    battery_gauge = bq27220_init(&config);
    if (battery_gauge == NULL) {
        ESP_LOGE(battery_tag, "bq27220 init failed");
        battery_need_init = true;
        battery_retry_deadline_ms = millis() + BATTERY_RETRY_DELAY_MS;
        return battery_status;
    }

    if (bq27220_get_battery_status(battery_gauge, &gauge_status) != ESP_OK) {
        return battery_handle_comm_error();
    }
    battery_gauge_status = gauge_status;

    if (!gauge_status.BATTPRES) {
        return battery_init_failed();
    }

    battery_need_init = false;
    battery_status = BATTERY_OK;
    return battery_status;
}

BatteryStatus battery_get_info(BatteryInfo *battery_info)
{
    battery_status_t gauge_status = {};
    BatteryStatus init_status;

    if (battery_info == NULL) {
        battery_status = BATTERY_INIT_FAILED;
        return battery_status;
    }

    memset(battery_info, 0, sizeof(*battery_info));

    if (battery_gauge == NULL) {
        if (battery_capacity_mah == 0) {
            battery_status = BATTERY_INIT_FAILED;
            return battery_status;
        }
        if (battery_need_init && millis() < battery_retry_deadline_ms) {
            return battery_status;
        }
        init_status = battery_init(battery_capacity_mah);
        if (init_status != BATTERY_OK) {
            return init_status;
        }
    }

    if (bq27220_get_battery_status(battery_gauge, &gauge_status) != ESP_OK) {
        return battery_handle_comm_error();
    }
    battery_gauge_status = gauge_status;
    if (!gauge_status.BATTPRES) {
        return battery_init_failed();
    }

    if (!battery_read_i16_checked(&battery_info->ma, bq27220_get_current)) {
        return battery_handle_comm_error();
    }
    if (battery_info->ma == 0) {
        return battery_not_connect();
    }
    if (!battery_read_u16_checked(&battery_info->mv, bq27220_get_voltage) ||
        !battery_read_u16_checked(&battery_info->soc, bq27220_get_state_of_charge) ||
        !battery_read_u16_checked(&battery_info->tte, bq27220_get_time_to_empty) ||
        !battery_read_u16_checked(&battery_info->ttf, bq27220_get_time_to_full) ||
        !battery_read_u16_checked(&battery_info->mah_rem, bq27220_get_remaining_capacity) ||
        !battery_read_u16_checked(&battery_info->mah_fcc, bq27220_get_full_charge_capacity) ||
        !battery_read_u16_checked(&battery_info->cycles, bq27220_get_cycle_count)) {
        return battery_handle_comm_error();
    }

    uint16_t temp_0_1k = 0;
    if (!battery_read_u16_checked(&temp_0_1k, bq27220_get_temperature)) {
        return battery_handle_comm_error();
    }
    battery_info->tc = temp_to_c(temp_0_1k);

    battery_need_init = false;
    battery_status = BATTERY_OK;
    return battery_status;
}

const char *battery_status_to_string(BatteryStatus status)
{
    switch (status) {
        case BATTERY_OK:
            return "Battery ready";
        case BATTERY_NOT_CONNECT:
            return "Battery Not Connect";
        case BATTERY_INIT_FAILED:
        default:
            return "Battery init failed";
    }
}

/**************** battery_display ****************/

static void format_time(const BatteryInfo *battery_info, char *time_field, size_t field_size)
{
    const char *label = "TTE";
    uint16_t minutes = battery_info->tte;

    if (battery_info->ma > 0) {
        label = "TTF";
        minutes = battery_info->ttf;
    }

    if (minutes == BATTERY_TIME_INVALID || minutes == 0) {
        snprintf(time_field, field_size, "%s: --", label);
        return;
    }

    if (minutes >= 60) {
        snprintf(time_field, field_size, "%s: %uh %um", label, minutes / 60, minutes % 60);
        return;
    }

    snprintf(time_field, field_size, "%s: %umin", label, minutes);
}

static const char *get_state_text(const BatteryInfo *battery_info)
{
    if (battery_info->ma > 0) {
        return "Charging";
    }

    return "Discharging";
}

esp_err_t display_init(void)
{
    size_t image_size = (size_t)EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2U;

    if (display_image != NULL) {
        return ESP_OK;
    }

    waveshare_esp32_s3_rgb_lcd_init();
    waveshare_rgb_lcd_bl_on();

    display_image = (UBYTE *)malloc(image_size);
    if (display_image == NULL) {
        ESP_LOGE(battery_tag, "allocate framebuffer failed");
        return ESP_ERR_NO_MEM;
    }

    Paint_NewImage(display_image, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, 0, WHITE);
    Paint_SetScale(65);
    Paint_SetRotate(ROTATE_0);
    Paint_Clear(WHITE);
    waveshare_rgb_lcd_display(display_image);
    return ESP_OK;
}

void display_show_info(const BatteryInfo *battery_info)
{
    char line0[32];
    char line1[16];
    char line2[32];
    char line3[48];
    char line4[48];
    char line5[32];
    char line6[24];
    UWORD state_color = DISPLAY_INFO_COLOR;

    if (battery_info == NULL) {
        return;
    }

    if (display_image == NULL && display_init() != ESP_OK) {
        return;
    }

    if (battery_info->ma > 0) {
        state_color = GREEN;
    } else if (battery_info->ma < 0) {
        state_color = RED;
    }

    format_time(battery_info, line6, sizeof(line6));
    snprintf(line0, sizeof(line0), "State: %s", get_state_text(battery_info));
    snprintf(line1, sizeof(line1), "BAT: %u%%", battery_info->soc);
    snprintf(line2, sizeof(line2), "Volt: %u mV", battery_info->mv);
    snprintf(line3, sizeof(line3), "Curr: %d mA  Temp: %d C", battery_info->ma, battery_info->tc);
    snprintf(line4, sizeof(line4), "Rem: %u mAh  FCC: %u mAh", battery_info->mah_rem, battery_info->mah_fcc);
    snprintf(line5, sizeof(line5), "Cycle: %u", battery_info->cycles);

    Paint_SelectImage(display_image);
    Paint_Clear(WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y, line0, &Font24, state_color, WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 50, line1, &Font48, state_color, WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 120, line2, &Font20, DISPLAY_INFO_COLOR, WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 120 + DISPLAY_LINE_STEP, line3, &Font20, DISPLAY_INFO_COLOR, WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 120 + DISPLAY_LINE_STEP * 2, line4, &Font20, DISPLAY_INFO_COLOR, WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 120 + DISPLAY_LINE_STEP * 3, line5, &Font20, DISPLAY_INFO_COLOR, WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 120 + DISPLAY_LINE_STEP * 4, line6, &Font20, DISPLAY_INFO_COLOR, WHITE);
    waveshare_rgb_lcd_display(display_image);
}

void display_show_error(const char *message)
{
    const char *error_message = (message != NULL) ? message : "Unknown error";

    if (display_image == NULL && display_init() != ESP_OK) {
        return;
    }

    Paint_SelectImage(display_image);
    Paint_Clear(WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 40, "Battery Error", &Font24, DISPLAY_ERROR_COLOR, WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 110, error_message, &Font20, DISPLAY_ERROR_COLOR, WHITE);
    if (battery_capacity_mah > 0) {
        char line[32];
        snprintf(line, sizeof(line), "Cap: %u mAh", battery_capacity_mah);
        Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 160, line, &Font20, DISPLAY_INFO_COLOR, WHITE);
    }
    waveshare_rgb_lcd_display(display_image);
}

void display_show_not_connect(void)
{
    if (display_image == NULL && display_init() != ESP_OK) {
        return;
    }

    Paint_SelectImage(display_image);
    Paint_Clear(WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 40, "Battery Status", &Font24, DISPLAY_INFO_COLOR, WHITE);
    Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 110, "Battery Not Connect", &Font24, DISPLAY_ERROR_COLOR, WHITE);
    if (battery_capacity_mah > 0) {
        char line[32];
        snprintf(line, sizeof(line), "Cap: %u mAh", battery_capacity_mah);
        Paint_DrawString_EN(DISPLAY_X, DISPLAY_Y + 180, line, &Font20, DISPLAY_INFO_COLOR, WHITE);
    }
    waveshare_rgb_lcd_display(display_image);
}

/*****************************************************************************
 * | File      	 :   main.c
 * | Author      :   Waveshare team
 * | Function    :   Main function
 * | Info        :
 *                   LCD screen test, including drawing dots,
 *                   lines, rectangles, circles, characters and pictures
 *----------------
 * |This version :   V1.0
 * | Date        :   2024-11-19
 * | Info        :   Basic version
 *
 ******************************************************************************/
#include "gui_paint.h"    // Header for graphical drawing functions
#include "image.h"        // Header for image resources
#include "rgb_lcd_port.h" // Header for Waveshare RGB LCD driver

#include "bq27220.h"
#include "esp_log.h"
#include "i2c_bus.h"

#define ROTATE ROTATE_0 // rotate = 0, 90, 180, 270
#define TARGET_BAT_CAPACITY_MAH 1500
#define INVALID_SAMPLE_THRESHOLD 2

static i2c_bus_handle_t i2c_bus = NULL;
static bq27220_handle_t bq27220 = NULL;

static esp_err_t bsp_bat_init(uint16_t mah);

/**
 * @brief BSP display configuration structure
 */
typedef struct {
  uint16_t mv;      // 电压（mV）
  int16_t ma;       // 电流（mA），正值=充电，负值=放电
  int16_t max_load; // 负载电流（mA）
  int16_t mw;       // 功率（mW）
  uint16_t soc;     // 剩余电量百分比（%）
  uint16_t tte;     // 预计还能用多久（min）
  uint16_t ttf;     // 预计充满还需多久（min）
  uint16_t mah_rem; // 剩余容量（mAh）
  uint16_t mah_fcc; // 满充容量（mAh）
  uint16_t cycles;  // 循环次数（cycle count）
  uint16_t soh;     // 健康度（State of Health %）
  int16_t tc;       // 温度（°C）
} bsp_bat_info_t;

static bq27220_handle_t create_bq27220_with_profile(uint16_t mah,
                                                    uint16_t profile_bias) {
  static const gauging_config_t default_config = {
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

  parameter_cedv_t default_cedv = {
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

  default_cedv.full_charge_cap = mah + profile_bias;
  default_cedv.design_cap = mah + profile_bias;
  default_cedv.EMF += profile_bias;

  bq27220_config_t bq27220_cfg = {
      .i2c_bus = i2c_bus,
      .cfg = &default_config,
      .cedv = &default_cedv,
  };

  return bq27220_create(&bq27220_cfg);
}

static esp_err_t refresh_bq27220_profile(uint16_t mah) {
  if (bq27220 != NULL) {
    bq27220_delete(bq27220);
    bq27220 = NULL;
  }

  return bsp_bat_init(mah);
}

/**************************************************************************************************
 *
 * BQ27220 BAT Function
 *
 **************************************************************************************************/
esp_err_t bsp_bat_init(uint16_t mah) {
  bq27220_handle_t refresh_handle = NULL;

  // Two-step creation forces the component to rewrite the profile even when
  // the previous configuration matches the requested one.
  refresh_handle = create_bq27220_with_profile(mah, 1);
  if (refresh_handle != NULL) {
    ESP_LOGI("BQ27220",
             "Temporary profile applied, recreating with target profile");
    bq27220_delete(refresh_handle);
    refresh_handle = NULL;
  } else {
    ESP_LOGW(
        "BQ27220",
        "Temporary profile refresh failed, trying target profile directly");
  }

  bq27220 = create_bq27220_with_profile(mah, 0);
  if (bq27220 == NULL) {
    ESP_LOGE("BQ27220", "bq27220_create failed");
    return ESP_FAIL;
  }

  ESP_LOGI("BQ27220", "Battery profile refreshed with %umAh target", mah);
  return ESP_OK;
}

esp_err_t bsp_get_bat_info(bsp_bat_info_t *bat_info) {
  if (bat_info == NULL || bq27220 == NULL) {
    return ESP_ERR_INVALID_STATE;
  }

  bat_info->mv = bq27220_get_voltage(bq27220);
  bat_info->ma = bq27220_get_current(bq27220);
  bat_info->mah_rem = bq27220_get_remaining_capacity(bq27220);
  bat_info->mah_fcc = bq27220_get_full_charge_capacity(bq27220);
  bat_info->tc = bq27220_get_temperature(bq27220) / 10 -
                 273; // Convert from 0.1K to Celsius
  bat_info->cycles = bq27220_get_cycle_count(bq27220);
  bat_info->soc = bq27220_get_state_of_charge(bq27220);
  bat_info->mw = bq27220_get_average_power(bq27220);         // in mW
  bat_info->max_load = bq27220_get_maxload_current(bq27220); // in mA
  bat_info->tte = bq27220_get_time_to_empty(bq27220);
  bat_info->ttf = bq27220_get_time_to_full(bq27220);
  bat_info->soh = bq27220_get_state_of_health(bq27220);

  // ESP_LOGI(TAG, "Battery Info - Vol: %dmv, Current: %dmA, Power: %dmW,
  // Remaining Capacity: %dmAh, Full Charge Capacity: %dmAh, Temperature: %dC,
  // Cycle Count: %d, SOC: %d%%, Max Load: %dmA, Time to empty: %dmin, Time to
  // full: %dmin, SOH=%d%%",
  //          bat_info->mv, bat_info->ma, bat_info->mw, bat_info->mah_rem,
  //          bat_info->mah_fcc, bat_info->tc, bat_info->cycles, bat_info->soc,
  //          bat_info->max_load, bat_info->tte, bat_info->ttf, bat_info->soh);

  return ESP_OK;
}

void app_main() {
  uint8_t invalid_count = 0;
  i2c_config_t conf = {
      .mode = I2C_MODE_MASTER,
      .sda_io_num = EXAMPLE_I2C_MASTER_SDA,
      .sda_pullup_en = GPIO_PULLUP_ENABLE,
      .scl_io_num = EXAMPLE_I2C_MASTER_SCL,
      .scl_pullup_en = GPIO_PULLUP_ENABLE,
      .master.clk_speed = 100 * 1000,
  };
  i2c_bus = i2c_bus_create(EXAMPLE_I2C_MASTER_NUM, &conf);

  DEV_I2C_Init(i2c_bus_get_internal_bus_handle(i2c_bus));
  IO_EXTENSION_Init();

  refresh_bq27220_profile(TARGET_BAT_CAPACITY_MAH);

  // Initialize the Waveshare ESP32-S3 RGB LCD
  waveshare_esp32_s3_rgb_lcd_init();

  // Turn on the LCD backlight
  waveshare_rgb_lcd_bl_on();

  // Allocate memory for the screen's frame buffer
  UDOUBLE Imagesize = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES *
                      2; // Each pixel takes 2 bytes in RGB565
  UBYTE *BlackImage;
  if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) // Allocate memory
  {
    printf("Failed to apply for black memory...\r\n");
    exit(0); // Exit the program if memory allocation fails
  }

  // Create a new image canvas and set its background color to white
  Paint_NewImage(BlackImage, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, 0, WHITE);

  // Set the canvas scale
  Paint_SetScale(65);
  Paint_SetRotate(ROTATE);
  // Clear the canvas and fill it with a white background
  Paint_Clear(WHITE);

  bsp_bat_info_t bat_info;
  char bat[200];

  while (1) {
    if (bsp_get_bat_info(&bat_info) != ESP_OK) {
      invalid_count++;
      ESP_LOGW("BQ27220", "Invalid sample #%u, skip display refresh",
               invalid_count);

      if (invalid_count >= INVALID_SAMPLE_THRESHOLD) {
        ESP_LOGW("BQ27220", "Battery profile drift detected, reinitializing");
        if (refresh_bq27220_profile(TARGET_BAT_CAPACITY_MAH) == ESP_OK) {
          ESP_LOGI("BQ27220", "Battery profile restored");
        } else {
          ESP_LOGE("BQ27220", "Battery reinitialization failed");
        }
        invalid_count = 0;
      }

      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    invalid_count = 0;
    memset(bat, 0, sizeof(bat));
    Paint_Clear(WHITE);
    if (bat_info.ma > 0) {
      sprintf(bat,
              "%d%% | %dmV | %dmA | %dC | TTF=%d min | Rem=%dmAh | FCC=%dmAh | "
              "Cyc=%d | SOH=%d%%",
              bat_info.soc, bat_info.mv, bat_info.ma, bat_info.tc, bat_info.ttf,
              bat_info.mah_rem, bat_info.mah_fcc, bat_info.cycles,
              bat_info.soh);

      Paint_DrawString_EN(50, 130, bat, &Font48, GREEN, WHITE);

      waveshare_rgb_lcd_display(BlackImage);
      // 正在充电：显示 TTF（充满剩余时间）
      ESP_LOGI("BQ27220",
               "%d%% | %dmV | %dmA | %dC | TTF=%d min | Rem=%dmAh | FCC=%dmAh "
               "| Cyc=%d | SOH=%d%%",
               bat_info.soc, bat_info.mv, bat_info.ma, bat_info.tc,
               bat_info.ttf, bat_info.mah_rem, bat_info.mah_fcc,
               bat_info.cycles, bat_info.soh);
    } else {
      sprintf(bat,
              "%d%% | %dmV | %dmA | %dC | TTE=%d min | Rem=%dmAh | FCC=%dmAh | "
              "Cyc=%d | SOH=%d%%",
              bat_info.soc, bat_info.mv, bat_info.ma, bat_info.tc, bat_info.tte,
              bat_info.mah_rem, bat_info.mah_fcc, bat_info.cycles,
              bat_info.soh);

      Paint_DrawString_EN(50, 130, bat, &Font48, RED, WHITE);

      waveshare_rgb_lcd_display(BlackImage);

      // 正在放电：显示 TTE（还能用多久）
      ESP_LOGI("BQ27220",
               "%d%% | %dmV | %dmA | %dC | TTE=%d min | Rem=%dmAh | FCC=%dmAh "
               "| Cyc=%d | SOH=%d%%",
               bat_info.soc, bat_info.mv, bat_info.ma, bat_info.tc,
               bat_info.tte, bat_info.mah_rem, bat_info.mah_fcc,
               bat_info.cycles, bat_info.soh);
    }
    battery_status_t battery_status = {0};

    if (bq27220_get_battery_status(bq27220, &battery_status) == ESP_OK) {
      if (battery_status.BATTPRES) {
        ESP_LOGI("BQ27220", "battery present");
      } else {
        ESP_LOGW("BQ27220", "battery not present");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1000)); // 延时1.5秒
  }
}

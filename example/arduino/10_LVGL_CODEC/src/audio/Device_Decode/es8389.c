#include "es8389.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "../../io_extension/io_extension.h"

#define ES8389_I2C_ADDR_7BIT 0x10
#define ES8389_I2C_TIMEOUT_MS 100

typedef struct {
    uint8_t reg_addr;
    uint8_t reg_value;
} Es8389RegSetting;

static const char *tag = "es8389_audio";
static i2c_master_dev_handle_t es8389_dev_handle = NULL;
static bool es8389_ready = false;

static const Es8389RegSetting es8389_init_sequence[] = {
    {0xF3, 0x00},
    {0x00, 0x7E},
    {0xF3, 0x38},
    {0x24, 0x64},
    {0x25, 0x04},
    {0x45, 0x03},
    {0x60, 0x2A},
    {0x61, 0xC9},
    {0x62, 0x4F},
    {0x63, 0x06},
    {0x6B, 0x00},
    {0x6D, 0x16},
    {0x6E, 0xAA},
    {0x6F, 0x66},
    {0x70, 0x99},
    {0x23, 0x80},
    {0x72, 0x10},
    {0x73, 0x10},
    {0x10, 0xC4},
    {0x01, 0x08},
    {0xF1, 0x00},
    {0x12, 0x01},
    {0x13, 0x01},
    {0x14, 0x01},
    {0x15, 0x01},
    {0x16, 0x35},
    {0x17, 0x09},
    {0x18, 0x91},
    {0x19, 0x28},
    {0x1A, 0x01},
    {0x1B, 0x01},
    {0x1C, 0x11},
    {0x2A, 0x00},
    {0x20, 0x60},
    {0x40, 0x60},
    {0xF0, 0x1A},
    {0x02, 0x00},
    {0x04, 0x00},
    {0x05, 0x10},
    {0x06, 0x00},
    {0x07, 0xC0},
    {0x08, 0x00},
    {0x09, 0xC0},
    {0x0A, 0x80},
    {0x0B, 0x04},
    {0x0C, 0x01},
    {0x0D, 0x00},
    {0x0F, 0x10},
    {0x21, 0x1F},
    {0x22, 0x7F},
    {0x2F, 0xC0},
    {0x30, 0xF4},
    {0x31, 0x00},
    {0x44, 0x00},
    {0x41, 0x7F},
    {0x42, 0x7F},
    {0x43, 0x10},
    {0x49, 0x0F},
    {0x4C, 0xC0},
    {0x00, 0x00},
    {0x03, 0xC1},
    {0x00, 0x01},
    {0x4D, 0x00},
    {0x26, 0xBF},
    {0x27, 0xBF},
    {0x28, 0xBF},
    {0x46, 0xBF},
    {0x47, 0xBF},
    {0x48, 0xBE},
    {0x4D, 0x00},
    {0x69, 0x20},
    {0x61, 0xD9},
    {0x64, 0x8F},
    {0x10, 0xE4},
    {0x00, 0x01},
    {0x03, 0xC3},
    {0x24, 0x6A},
    {0x25, 0x0A},
};

static esp_err_t es8389_write_reg(uint8_t reg_addr, uint8_t reg_value)
{
    const uint8_t payload[] = {reg_addr, reg_value};
    return i2c_master_transmit(es8389_dev_handle, payload, sizeof(payload), ES8389_I2C_TIMEOUT_MS);
}

static esp_err_t es8389_add_device(i2c_master_bus_handle_t bus_handle)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ES8389_I2C_ADDR_7BIT,
        .scl_speed_hz = 100000,
    };

    return i2c_master_bus_add_device(bus_handle, &dev_cfg, &es8389_dev_handle);
}

void es8389_audio_set_pa(bool enable)
{
    IO_EXTENSION_Output(IO_EXTENSION_IO_3, enable ? 1 : 0);
}

esp_err_t es8389_audio_init(i2c_master_bus_handle_t bus_handle)
{
    if (es8389_ready) {
        return ESP_OK;
    }

    if (bus_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(es8389_add_device(bus_handle), tag, "add ES8389 device failed");
    const size_t setting_count = sizeof(es8389_init_sequence) / sizeof(es8389_init_sequence[0]);
    for (size_t i = 0; i < setting_count; ++i) {
        ESP_RETURN_ON_ERROR(
            es8389_write_reg(es8389_init_sequence[i].reg_addr, es8389_init_sequence[i].reg_value),
            tag,
            "write reg 0x%02X failed",
            es8389_init_sequence[i].reg_addr);
    }

    es8389_audio_set_pa(true);
    es8389_ready = true;
    ESP_LOGI(tag, "ES8389 ready");
    return ESP_OK;
}

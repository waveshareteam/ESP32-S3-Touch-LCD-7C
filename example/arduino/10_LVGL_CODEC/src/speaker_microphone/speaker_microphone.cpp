/*****************************************************************************
 * | File         :   speaker_microphone.cpp
 * | Author       :   Waveshare team
 * | Function     :   Hardware underlying interface
 * | Info         :   I2S + I2C based audio codec initialization for speaker and microphone
 * ----------------
 * | This version :   V1.0
 * | Date         :   2025-07-28
 * | Info         :   Basic version
 *
 ******************************************************************************/

#include "speaker_microphone.h"
#include "codec_dev.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "speaker_microphone";

static i2s_chan_handle_t i2s_tx_chan = NULL;
static i2s_chan_handle_t i2s_rx_chan = NULL;
static const audio_codec_data_if_t *i2s_data_if = NULL;
static i2c_master_bus_handle_t i2c_handle = NULL;
static bool i2c_initialized = false;

#define CODEC_INIT_RETRY_TIMES    3
#define CODEC_INIT_RETRY_DELAY_MS 20

esp_err_t codec_i2c_init()
{
    if (i2c_initialized) {
        return ESP_OK;
    }

    i2c_handle = DEV_I2C_Get_Bus_Device();
    i2c_initialized = true;

    return ESP_OK;
}

esp_err_t codec_audio_init()
{
    esp_err_t ret = ESP_FAIL;
    audio_codec_i2s_cfg_t i2s_cfg = {};
#if CODEC_DEFAULT_TDM
    i2s_tdm_config_t tdm_cfg_default = {};
    const i2s_tdm_config_t *p_i2s_cfg = NULL;
#else
    i2s_std_config_t std_cfg_default = {};
    const i2s_std_config_t *p_i2s_cfg = NULL;
#endif

    if (i2s_tx_chan && i2s_rx_chan) {
        return ESP_OK;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_GOTO_ON_ERROR(i2s_new_channel(&chan_cfg, &i2s_tx_chan, &i2s_rx_chan), err, TAG, "create i2s channel failed");

#if CODEC_DEFAULT_TDM
    const i2s_tdm_slot_mask_t slot_mask = I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3;
    tdm_cfg_default = I2S_DUPLEX_MONO_CFG_TDM(CODEC_DEFAULT_SAMPLE_RATE, slot_mask);
    tdm_cfg_default.slot_cfg.total_slot = 4;
    p_i2s_cfg = &tdm_cfg_default;
#else
    std_cfg_default = I2S_DUPLEX_MONO_CFG(CODEC_DEFAULT_SAMPLE_RATE);
    p_i2s_cfg = &std_cfg_default;
#endif

    if (i2s_tx_chan) {
        ESP_GOTO_ON_ERROR(i2s_channel_init_std_mode(i2s_tx_chan, p_i2s_cfg), err, TAG, "TX channel init failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_tx_chan), err, TAG, "TX enable failed");
    }

    if (i2s_rx_chan) {
        i2s_tdm_config_t tdm_cfg = {
            .clk_cfg = {
                .sample_rate_hz = CODEC_DEFAULT_SAMPLE_RATE,
                .clk_src = I2S_CLK_SRC_DEFAULT,
                .ext_clk_freq_hz = 0,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
                .bclk_div = 8,
            },
            .slot_cfg = {
                .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
                .slot_mode = I2S_SLOT_MODE_STEREO,
                .slot_mask = static_cast<i2s_tdm_slot_mask_t>(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
                .ws_width = I2S_TDM_AUTO_WS_WIDTH,
                .ws_pol = false,
                .bit_shift = true,
                .left_align = false,
                .big_endian = false,
                .bit_order_lsb = false,
                .skip_mask = false,
                .total_slot = 4
            },
            .gpio_cfg = {
                .mclk = I2S_MCLK,
                .bclk = I2S_SCLK,
                .ws = I2S_LCLK,
                .dout = I2S_GPIO_UNUSED,
                .din = I2S_DSIN,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv = false
                }
            }
        };
        ESP_GOTO_ON_ERROR(i2s_channel_init_tdm_mode(i2s_rx_chan, &tdm_cfg), err, TAG, "RX channel init failed");
        ESP_GOTO_ON_ERROR(i2s_channel_enable(i2s_rx_chan), err, TAG, "RX enable failed");
    }

    i2s_cfg.port = I2S_NUM;
    i2s_cfg.rx_handle = i2s_rx_chan;
    i2s_cfg.tx_handle = i2s_tx_chan;
    i2s_data_if = audio_codec_new_i2s_data(&i2s_cfg);
    assert(i2s_data_if);

    return ESP_OK;

err:
    if (i2s_tx_chan) {
        i2s_channel_disable(i2s_tx_chan);
        i2s_del_channel(i2s_tx_chan);
        i2s_tx_chan = NULL;
    }
    if (i2s_rx_chan) {
        i2s_channel_disable(i2s_rx_chan);
        i2s_del_channel(i2s_rx_chan);
        i2s_rx_chan = NULL;
    }
    i2s_data_if = NULL;

    return ret;
}

esp_err_t speaker_audio_poweramp_enable(bool enable)
{
    IO_EXTENSION_Output(IO_EXTENSION_IO_3, enable);
    return ESP_OK;
}

esp_err_t speaker_i2s_release(void)
{
    if (i2s_data_if) {
        audio_codec_delete_data_if(i2s_data_if);
        i2s_data_if = NULL;
    }

    if (i2s_tx_chan) {
        i2s_channel_disable(i2s_tx_chan);
        i2s_del_channel(i2s_tx_chan);
        i2s_tx_chan = NULL;
    }

    if (i2s_rx_chan) {
        i2s_channel_disable(i2s_rx_chan);
        i2s_del_channel(i2s_rx_chan);
        i2s_rx_chan = NULL;
    }

    return ESP_OK;
}

esp_codec_dev_handle_t speaker_init(void)
{
    esp_codec_dev_handle_t codec_dev = NULL;

    if (i2s_data_if == NULL) {
        USER_ERROR_CHECK_RETURN_NULL(codec_i2c_init());
        USER_ERROR_CHECK_RETURN_NULL(codec_audio_init());
    }
    USER_NULL_CHECK(i2s_data_if, NULL);

    speaker_audio_poweramp_enable(true);

    const audio_codec_gpio_if_t *gpio_if = audio_codec_new_gpio();
    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = EXAMPLE_I2C_MASTER_NUM,
        .addr = ES8389_CODEC_DEFAULT_ADDR,
        .bus_handle = i2c_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    USER_NULL_CHECK(i2c_ctrl_if, NULL);

    esp_codec_dev_hw_gain_t gain = {
        .pa_voltage = 5,
        .codec_dac_voltage = 3.3,
    };

    es8389_codec_cfg_t es8389_cfg = {
        .ctrl_if = i2c_ctrl_if,
        .gpio_if = gpio_if,
        .codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC,
        .pa_pin = GPIO_NUM_NC,
        .pa_reverted = false,
        .master_mode = false,
        .use_mclk = true,
        .digital_mic = false,
        .invert_mclk = false,
        .invert_sclk = false,
        .hw_gain = gain,
    };

    esp_codec_dev_cfg_t codec_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = NULL,
        .data_if = i2s_data_if,
    };

    int retry = 0;
    while (retry < CODEC_INIT_RETRY_TIMES) {
        const audio_codec_if_t *es8389_dev = es8389_codec_new(&es8389_cfg);
        if (es8389_dev) {
            codec_dev_cfg.codec_if = es8389_dev;
            codec_dev = esp_codec_dev_new(&codec_dev_cfg);
            if (codec_dev) {
                return codec_dev;
            }
            audio_codec_delete_codec_if(es8389_dev);
        }

        retry++;
        if (retry < CODEC_INIT_RETRY_TIMES) {
            ESP_LOGW(TAG, "speaker init retry %d/%d", retry + 1, CODEC_INIT_RETRY_TIMES);
            vTaskDelay(pdMS_TO_TICKS(CODEC_INIT_RETRY_DELAY_MS));
        }
    }

    speaker_audio_poweramp_enable(false);
    speaker_i2s_release();
    return NULL;
}

esp_codec_dev_handle_t microphone_init(void)
{
    esp_codec_dev_handle_t codec_dev = NULL;

    if (i2s_data_if == NULL) {
        USER_ERROR_CHECK_RETURN_NULL(codec_i2c_init());
        USER_ERROR_CHECK_RETURN_NULL(codec_audio_init());
    }
    USER_NULL_CHECK(i2s_data_if, NULL);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port = EXAMPLE_I2C_MASTER_NUM,
        .addr = ES7210_CODEC_ADDR,
        .bus_handle = i2c_handle,
    };
    const audio_codec_ctrl_if_t *i2c_ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    USER_NULL_CHECK(i2c_ctrl_if, NULL);

    es7210_codec_cfg_t es7210_cfg = {};
    es7210_cfg.ctrl_if = i2c_ctrl_if;
    es7210_cfg.mic_selected = ES7210_SEL_MIC1 | ES7210_SEL_MIC2 | ES7210_SEL_MIC3 | ES7210_SEL_MIC4;
    esp_codec_dev_cfg_t codec_es7210_dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_IN,
        .codec_if = NULL,
        .data_if = i2s_data_if,
    };

    int retry = 0;
    while (retry < CODEC_INIT_RETRY_TIMES) {
        const audio_codec_if_t *es7210_dev = es7210_codec_new(&es7210_cfg);
        if (es7210_dev) {
            codec_es7210_dev_cfg.codec_if = es7210_dev;
            codec_dev = esp_codec_dev_new(&codec_es7210_dev_cfg);
            if (codec_dev) {
                return codec_dev;
            }
            audio_codec_delete_codec_if(es7210_dev);
        }

        retry++;
        if (retry < CODEC_INIT_RETRY_TIMES) {
            ESP_LOGW(TAG, "microphone init retry %d/%d", retry + 1, CODEC_INIT_RETRY_TIMES);
            vTaskDelay(pdMS_TO_TICKS(CODEC_INIT_RETRY_DELAY_MS));
        }
    }

    return NULL;
}

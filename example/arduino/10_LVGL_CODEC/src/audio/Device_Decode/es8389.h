#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "../config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the ES8389 codec and enable the board power amplifier.
esp_err_t es8389_audio_init(i2c_master_bus_handle_t bus_handle);

// Control the external power amplifier through the IO expander.
void es8389_audio_set_pa(bool enable);

#ifdef __cplusplus
}
#endif

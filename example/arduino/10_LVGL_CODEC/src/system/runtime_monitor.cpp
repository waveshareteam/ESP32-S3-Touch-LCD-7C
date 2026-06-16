#include "runtime_monitor.h"

#include <Arduino.h>
#include "esp_heap_caps.h"

typedef struct {
    uint32_t period_ms;
} RuntimeMonitorCtx;

static RuntimeMonitorCtx monitor_ctx = {
    .period_ms = 5000,
};

static bool monitor_started = false;

static void print_runtime_stats(void)
{
    uint32_t psram_total = ESP.getPsramSize();
    uint32_t psram_free = ESP.getFreePsram();
    uint32_t psram_used = (psram_total >= psram_free) ? (psram_total - psram_free) : 0;
    size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    uint32_t flash_total = ESP.getFlashChipSize();
    uint32_t sketch_used = ESP.getSketchSize();
    uint32_t sketch_free = ESP.getFreeSketchSpace();

    Serial.printf(
        "[monitor] PSRAM used=%luKB free=%luKB total=%luKB largest=%uKB | Flash sketch=%luKB free=%luKB total=%luKB\r\n",
        (unsigned long)(psram_used / 1024),
        (unsigned long)(psram_free / 1024),
        (unsigned long)(psram_total / 1024),
        (unsigned int)(psram_largest / 1024),
        (unsigned long)(sketch_used / 1024),
        (unsigned long)(sketch_free / 1024),
        (unsigned long)(flash_total / 1024));
}

static void runtime_monitor_task(void *arg)
{
    RuntimeMonitorCtx *ctx = (RuntimeMonitorCtx *)arg;

    while (true) {
        print_runtime_stats();
        vTaskDelay(pdMS_TO_TICKS(ctx->period_ms));
    }
}

void runtime_monitor_start(uint32_t period_ms)
{
    if (monitor_started) {
        return;
    }

    if (period_ms > 0) {
        monitor_ctx.period_ms = period_ms;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(
        runtime_monitor_task,
        "runtime_monitor",
        3072,
        &monitor_ctx,
        1,
        NULL,
        tskNO_AFFINITY);

    if (ok == pdPASS) {
        monitor_started = true;
    } else {
        Serial.println("[monitor] start failed");
    }
}

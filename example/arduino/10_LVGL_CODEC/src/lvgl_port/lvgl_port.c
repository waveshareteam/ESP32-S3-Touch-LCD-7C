#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "lvgl_port.h"

static const char *TAG = "lv_port";
static SemaphoreHandle_t lvgl_mux = NULL;
static TaskHandle_t lvgl_task_handle = NULL;
static esp_timer_handle_t lvgl_tick_timer = NULL;

#define LVGL_PORT_COLOR_BYTES 2

#if LVGL_VERSION_MAJOR >= 9
static void flush_callback(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);

    esp_lcd_panel_draw_bitmap(panel_handle,
                              area->x1,
                              area->y1,
                              area->x2 + 1,
                              area->y2 + 1,
                              px_map);

    lv_disp_flush_ready(disp);
}

static lv_display_t *display_init(esp_lcd_panel_handle_t panel_handle)
{
    lv_display_t *display = NULL;
    void *buf1 = NULL;
    void *buf2 = NULL;
    uint32_t buffer_size_bytes = 0;
    lv_display_render_mode_t render_mode = LV_DISP_RENDER_MODE_PARTIAL;

    assert(panel_handle);

    display = lv_display_create(LVGL_PORT_H_RES, LVGL_PORT_V_RES);
    assert(display);

    lv_display_set_user_data(display, panel_handle);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);

#if LVGL_PORT_AVOID_TEAR_ENABLE
    buffer_size_bytes = LVGL_PORT_H_RES * LVGL_PORT_V_RES * LVGL_PORT_COLOR_BYTES;
#if LVGL_PORT_LCD_RGB_BUFFER_NUMS >= 2
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 2, &buf1, &buf2));
#else
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 1, &buf1));
    buf2 = NULL;
#endif
#else
    buffer_size_bytes = LVGL_PORT_H_RES * LVGL_PORT_BUFFER_HEIGHT * LVGL_PORT_COLOR_BYTES;
    buf1 = heap_caps_malloc(buffer_size_bytes, LVGL_PORT_BUFFER_MALLOC_CAPS);
    assert(buf1);
#endif

#if LVGL_PORT_FULL_REFRESH
    render_mode = LV_DISP_RENDER_MODE_FULL;
#elif LVGL_PORT_DIRECT_MODE
    render_mode = LV_DISP_RENDER_MODE_DIRECT;
#else
    render_mode = LV_DISP_RENDER_MODE_PARTIAL;
#endif

    lv_display_set_buffers(display, buf1, buf2, buffer_size_bytes, render_mode);
    lv_display_set_flush_cb(display, flush_callback);
    return display;
}

static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = lv_indev_get_user_data(indev);
    uint16_t touchpad_x = 0;
    uint16_t touchpad_y = 0;
    uint8_t touchpad_cnt = 0;
    bool touchpad_pressed = false;

    assert(tp);

    esp_lcd_touch_read_data(tp);
    touchpad_pressed = esp_lcd_touch_get_coordinates(tp, &touchpad_x, &touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x;
        data->point.y = touchpad_y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static lv_indev_t *indev_init(esp_lcd_touch_handle_t tp)
{
    lv_indev_t *indev = NULL;

    assert(tp);

    indev = lv_indev_create();
    assert(indev);

    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_user_data(indev, tp);
    lv_indev_set_read_cb(indev, touchpad_read);

    return indev;
}
#else
static lv_disp_draw_buf_t display_draw_buf;
static lv_disp_drv_t display_drv;
static lv_indev_drv_t indev_drv;

static void flush_callback(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)disp_drv->user_data;

    esp_lcd_panel_draw_bitmap(panel_handle,
                              area->x1,
                              area->y1,
                              area->x2 + 1,
                              area->y2 + 1,
                              color_map);

    lv_disp_flush_ready(disp_drv);
}

static lv_disp_t *display_init(esp_lcd_panel_handle_t panel_handle)
{
    void *buf1 = NULL;
    void *buf2 = NULL;
    uint32_t draw_buf_pixels = 0;

    assert(panel_handle);

#if LVGL_PORT_AVOID_TEAR_ENABLE
    draw_buf_pixels = LVGL_PORT_H_RES * LVGL_PORT_V_RES;
#if LVGL_PORT_LCD_RGB_BUFFER_NUMS >= 2
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 2, &buf1, &buf2));
#else
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 1, &buf1));
    buf2 = NULL;
#endif
#else
    draw_buf_pixels = LVGL_PORT_H_RES * LVGL_PORT_BUFFER_HEIGHT;
    buf1 = heap_caps_malloc(draw_buf_pixels * sizeof(lv_color_t), LVGL_PORT_BUFFER_MALLOC_CAPS);
    assert(buf1);
#endif

    lv_disp_draw_buf_init(&display_draw_buf, buf1, buf2, draw_buf_pixels);

    lv_disp_drv_init(&display_drv);
    display_drv.hor_res = LVGL_PORT_H_RES;
    display_drv.ver_res = LVGL_PORT_V_RES;
    display_drv.flush_cb = flush_callback;
    display_drv.draw_buf = &display_draw_buf;
    display_drv.user_data = panel_handle;
#if LVGL_PORT_FULL_REFRESH
    display_drv.full_refresh = 1;
#endif
#if LVGL_PORT_DIRECT_MODE
    display_drv.direct_mode = 1;
#endif

    return lv_disp_drv_register(&display_drv);
}

static void touchpad_read(lv_indev_drv_t *indev_drv_ptr, lv_indev_data_t *data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)indev_drv_ptr->user_data;
    uint16_t touchpad_x = 0;
    uint16_t touchpad_y = 0;
    uint8_t touchpad_cnt = 0;
    bool touchpad_pressed = false;

    assert(tp);

    esp_lcd_touch_read_data(tp);
    touchpad_pressed = esp_lcd_touch_get_coordinates(tp, &touchpad_x, &touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x;
        data->point.y = touchpad_y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static lv_indev_t *indev_init(esp_lcd_touch_handle_t tp)
{
    assert(tp);

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_drv.user_data = tp;

    return lv_indev_drv_register(&indev_drv);
}
#endif

static void tick_increment(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_PORT_TICK_PERIOD_MS);
}

static esp_err_t tick_init(void)
{
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &tick_increment,
        .name = "LVGL tick",
    };

    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    return esp_timer_start_periodic(lvgl_tick_timer, LVGL_PORT_TICK_PERIOD_MS * 1000);
}

static void lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
    TickType_t wait_ticks = pdMS_TO_TICKS(LVGL_PORT_TASK_MAX_DELAY_MS);

    (void)arg;

    while (1) {
        if (lvgl_port_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            lvgl_port_unlock();
        }

        if (task_delay_ms > LVGL_PORT_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_PORT_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_PORT_TASK_MIN_DELAY_MS;
        }

        wait_ticks = pdMS_TO_TICKS(task_delay_ms);
        if (wait_ticks == 0) {
            wait_ticks = 1;
        }

#if LVGL_PORT_AVOID_TEAR_ENABLE
        /* Wake on VSYNC when available, otherwise fall back to the timeout. */
        (void) ulTaskNotifyTake(pdTRUE, wait_ticks);
#else
        vTaskDelay(wait_ticks);
#endif
    }
}

esp_err_t lvgl_port_init(esp_lcd_panel_handle_t lcd_handle, esp_lcd_touch_handle_t tp_handle)
{
#if LVGL_VERSION_MAJOR >= 9
    lv_display_t *disp = NULL;
#else
    lv_disp_t *disp = NULL;
#endif

    lv_init();
    ESP_ERROR_CHECK(tick_init());

    disp = display_init(lcd_handle);
    assert(disp);

    if (tp_handle) {
        assert(indev_init(tp_handle));

#if EXAMPLE_LVGL_PORT_ROTATION_90
        esp_lcd_touch_set_swap_xy(tp_handle, true);
        esp_lcd_touch_set_mirror_y(tp_handle, true);
#elif EXAMPLE_LVGL_PORT_ROTATION_180
        esp_lcd_touch_set_mirror_x(tp_handle, true);
        esp_lcd_touch_set_mirror_y(tp_handle, true);
#elif EXAMPLE_LVGL_PORT_ROTATION_270
        esp_lcd_touch_set_swap_xy(tp_handle, true);
        esp_lcd_touch_set_mirror_x(tp_handle, true);
#endif
    }

    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mux);

    BaseType_t core_id = (LVGL_PORT_TASK_CORE < 0) ? tskNO_AFFINITY : LVGL_PORT_TASK_CORE;
    BaseType_t ret = xTaskCreatePinnedToCore(lvgl_port_task,
                                             "lvgl",
                                             LVGL_PORT_TASK_STACK_SIZE,
                                             NULL,
                                             LVGL_PORT_TASK_PRIORITY,
                                             &lvgl_task_handle,
                                             core_id);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

bool lvgl_port_lock(int timeout_ms)
{
    assert(lvgl_mux && "lvgl_port_init must be called first");

    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_port_unlock(void)
{
    assert(lvgl_mux && "lvgl_port_init must be called first");
    xSemaphoreGiveRecursive(lvgl_mux);
}

bool lvgl_port_notify_rgb_vsync(void)
{
    BaseType_t need_yield = pdFALSE;

#if LVGL_PORT_AVOID_TEAR_ENABLE
    if (lvgl_task_handle == NULL) {
        return false;
    }

    vTaskNotifyGiveFromISR(lvgl_task_handle, &need_yield);
#endif

    return (need_yield == pdTRUE);
}

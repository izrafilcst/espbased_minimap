/**
 * @file ui_engine.c
 * @brief Motor LVGL — inicialização, task de rendering e navegação.
 */

#include "ui_engine.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ui_engine";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static ui_screen_id_t s_current_screen = UI_SCREEN_SPLASH;
static TaskHandle_t   s_task_hdl       = NULL;

/* ─── Implementações (stubs) ────────────────────────────────────────────────*/

esp_err_t ui_engine_init(void)
{
    ESP_LOGI(TAG, "ui_engine_init: stub");
    return ESP_OK;
}

esp_err_t ui_engine_deinit(void)
{
    if (s_task_hdl) { vTaskDelete(s_task_hdl); s_task_hdl = NULL; }
    return ESP_OK;
}

esp_err_t ui_engine_switch_screen(ui_screen_id_t screen_id)
{
    if (screen_id >= UI_SCREEN_COUNT) return ESP_ERR_INVALID_ARG;
    s_current_screen = screen_id;
    return ESP_OK;
}

ui_screen_id_t ui_engine_current_screen(void)
{
    return s_current_screen;
}

void ui_engine_handle_button(uint8_t btn_id, uint8_t evt_type)
{
    (void)btn_id; (void)evt_type;
}

void ui_engine_lvgl_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "ui_engine_lvgl_task: stub — suspending");
    vTaskSuspend(NULL);
}

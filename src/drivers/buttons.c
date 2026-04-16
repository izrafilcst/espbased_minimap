/**
 * @file buttons.c
 * @brief Implementação do driver de botões com debounce e long-press.
 */

#include "buttons.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "buttons";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static QueueHandle_t s_event_queue = NULL;
static TaskHandle_t  s_task_hdl   = NULL;

/* ─── Implementações (stubs) ────────────────────────────────────────────────*/

esp_err_t buttons_init(void)
{
    ESP_LOGI(TAG, "buttons_init: stub");
    s_event_queue = xQueueCreate(16, sizeof(btn_event_t));
    return (s_event_queue != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t buttons_deinit(void)
{
    ESP_LOGI(TAG, "buttons_deinit: stub");
    if (s_task_hdl) {
        vTaskDelete(s_task_hdl);
        s_task_hdl = NULL;
    }
    if (s_event_queue) {
        vQueueDelete(s_event_queue);
        s_event_queue = NULL;
    }
    return ESP_OK;
}

QueueHandle_t buttons_get_queue(void)
{
    return s_event_queue;
}

bool buttons_is_pressed(btn_id_t id)
{
    (void)id;
    return false;
}

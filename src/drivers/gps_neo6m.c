/**
 * @file gps_neo6m.c
 * @brief Implementação do driver GPS NEO-6M via UART1.
 */

#include "gps_neo6m.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "gps_neo6m";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static gps_data_t       s_gps_data   = {0};
static SemaphoreHandle_t s_mutex     = NULL;
static TaskHandle_t      s_task_hdl  = NULL;

/* ─── Implementações (stubs) ────────────────────────────────────────────────*/

esp_err_t gps_init(void)
{
    ESP_LOGI(TAG, "gps_init: stub");
    s_mutex = xSemaphoreCreateMutex();
    return (s_mutex != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t gps_deinit(void)
{
    ESP_LOGI(TAG, "gps_deinit: stub");
    if (s_task_hdl) {
        vTaskDelete(s_task_hdl);
        s_task_hdl = NULL;
    }
    return ESP_OK;
}

esp_err_t gps_get_data(gps_data_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    if (!s_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(out, &s_gps_data, sizeof(gps_data_t));
    xSemaphoreGive(s_mutex);
    return s_gps_data.valid ? ESP_OK : ESP_ERR_NOT_FOUND;
}

bool gps_has_fix(void)
{
    return s_gps_data.valid && (s_gps_data.fix_quality > 0);
}

esp_err_t gps_set_baud_115200(void)
{
    ESP_LOGI(TAG, "gps_set_baud_115200: stub");
    return ESP_OK;
}

void gps_rx_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "gps_rx_task: stub — suspending");
    vTaskSuspend(NULL);
}

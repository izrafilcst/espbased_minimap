/**
 * @file mesh_beacon.c
 * @brief Beacon periódico com posição GPS do nó local.
 */

#include "mesh_beacon.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "mesh_beacon";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static uint8_t       s_src_id[MESH_SRC_ID_LEN] = {0};
static uint16_t      s_pkt_id                   = 0;
static TaskHandle_t  s_task_hdl                 = NULL;

/* ─── Implementações (stubs) ────────────────────────────────────────────────*/

esp_err_t mesh_beacon_init(void)
{
    ESP_LOGI(TAG, "mesh_beacon_init: stub");
    esp_read_mac(s_src_id, ESP_MAC_WIFI_STA);
    return ESP_OK;
}

esp_err_t mesh_beacon_deinit(void)
{
    if (s_task_hdl) { vTaskDelete(s_task_hdl); s_task_hdl = NULL; }
    return ESP_OK;
}

esp_err_t mesh_beacon_send_now(void)
{
    ESP_LOGI(TAG, "mesh_beacon_send_now: stub");
    return ESP_OK;
}

void mesh_beacon_get_src_id(uint8_t out[MESH_SRC_ID_LEN])
{
    memcpy(out, s_src_id, MESH_SRC_ID_LEN);
}

uint16_t mesh_beacon_next_pkt_id(void)
{
    return ++s_pkt_id;
}

void mesh_beacon_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "mesh_beacon_task: stub — suspending");
    vTaskSuspend(NULL);
}

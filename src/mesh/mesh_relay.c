/**
 * @file mesh_relay.c
 * @brief Relay de pacotes mesh com deduplicação.
 */

#include "mesh_relay.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "mesh_relay";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static QueueHandle_t s_rx_queue   = NULL;
static TaskHandle_t  s_task_hdl   = NULL;

/* Tabela de deduplicação simples */
typedef struct {
    uint8_t  src_id[MESH_SRC_ID_LEN];
    uint16_t pkt_id;
    uint32_t timestamp_ms;
} dedup_entry_t;

static dedup_entry_t s_dedup[MESH_DEDUP_TABLE_SIZE];

/* ─── Implementações (stubs) ────────────────────────────────────────────────*/

esp_err_t mesh_relay_init(void)
{
    ESP_LOGI(TAG, "mesh_relay_init: stub");
    s_rx_queue = xQueueCreate(8, sizeof(mesh_packet_t));
    memset(s_dedup, 0, sizeof(s_dedup));
    return (s_rx_queue != NULL) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t mesh_relay_deinit(void)
{
    if (s_task_hdl) { vTaskDelete(s_task_hdl); s_task_hdl = NULL; }
    if (s_rx_queue) { vQueueDelete(s_rx_queue); s_rx_queue = NULL; }
    return ESP_OK;
}

void mesh_relay_rx_cb(const uint8_t *data, size_t len, int8_t rssi, int8_t snr)
{
    (void)data; (void)len; (void)rssi; (void)snr;
}

bool mesh_relay_is_duplicate(const uint8_t src_id[MESH_SRC_ID_LEN],
                             uint16_t pkt_id)
{
    (void)src_id; (void)pkt_id;
    return false;
}

void mesh_relay_register(const uint8_t src_id[MESH_SRC_ID_LEN],
                         uint16_t pkt_id)
{
    (void)src_id; (void)pkt_id;
}

void mesh_relay_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "mesh_relay_task: stub — suspending");
    vTaskSuspend(NULL);
}

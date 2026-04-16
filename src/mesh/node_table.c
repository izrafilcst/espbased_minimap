/**
 * @file node_table.c
 * @brief Tabela de nós mesh visíveis alocada na PSRAM.
 */

#include "node_table.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "node_table";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static mesh_node_t       *s_table  = NULL;  /* Alocado na PSRAM */
static SemaphoreHandle_t  s_mutex  = NULL;

/* ─── Implementações (stubs) ────────────────────────────────────────────────*/

esp_err_t node_table_init(void)
{
    ESP_LOGI(TAG, "node_table_init: stub");
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;
    s_table = (mesh_node_t *)MT_MALLOC_PSRAM(
                  NODE_TABLE_MAX_NODES * sizeof(mesh_node_t));
    if (!s_table) return ESP_ERR_NO_MEM;
    memset(s_table, 0, NODE_TABLE_MAX_NODES * sizeof(mesh_node_t));
    return ESP_OK;
}

esp_err_t node_table_deinit(void)
{
    if (s_table) { MT_FREE(s_table); s_table = NULL; }
    if (s_mutex) { vSemaphoreDelete(s_mutex); s_mutex = NULL; }
    return ESP_OK;
}

esp_err_t node_table_update(const uint8_t src_id[MESH_SRC_ID_LEN],
                            const beacon_payload_t *beacon,
                            int8_t rssi, int8_t snr, uint8_t hop)
{
    (void)src_id; (void)beacon; (void)rssi; (void)snr; (void)hop;
    return ESP_OK;
}

void node_table_prune(void)
{
    /* stub */
}

esp_err_t node_table_get_all(mesh_node_t *out, size_t *out_count)
{
    if (!out || !out_count) return ESP_ERR_INVALID_ARG;
    *out_count = 0;
    return ESP_OK;
}

esp_err_t node_table_get_by_id(const uint8_t node_id[MESH_SRC_ID_LEN],
                                mesh_node_t *out)
{
    (void)node_id; (void)out;
    return ESP_ERR_NOT_FOUND;
}

size_t node_table_count(void)
{
    return 0;
}

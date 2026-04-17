/**
 * @file node_table.c
 * @brief Tabela de nós mesh visíveis, alocada na PSRAM.
 *
 * Até NODE_TABLE_MAX_NODES (20) entradas simultâneas.
 * Cada entrada atualizada ao receber um BEACON válido.
 * Entradas expiradas (> NODE_TABLE_TIMEOUT_MS sem beacon) são removidas
 * por node_table_prune(), chamado periodicamente pela task de relay.
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
static mesh_node_t       *s_table = NULL;
static SemaphoreHandle_t  s_mutex = NULL;

/* ─── Helper: timestamp atual em ms ────────────────────────────────────────*/
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * node_table_init
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t node_table_init(void)
{
    ESP_LOGI(TAG, "=== node_table_init START ===");

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Falha ao criar mutex");
        return ESP_ERR_NO_MEM;
    }

    s_table = (mesh_node_t *)MT_MALLOC_PSRAM(
                  NODE_TABLE_MAX_NODES * sizeof(mesh_node_t));
    if (!s_table) {
        ESP_LOGE(TAG, "Falha ao alocar tabela na PSRAM");
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
        return ESP_ERR_NO_MEM;
    }

    memset(s_table, 0, NODE_TABLE_MAX_NODES * sizeof(mesh_node_t));

    ESP_LOGI(TAG, "=== node_table_init OK (%u slots × %zu B na PSRAM) ===",
             NODE_TABLE_MAX_NODES, sizeof(mesh_node_t));
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * node_table_deinit
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t node_table_deinit(void)
{
    if (s_table) { MT_FREE(s_table); s_table = NULL; }
    if (s_mutex) { vSemaphoreDelete(s_mutex); s_mutex = NULL; }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * node_table_update
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t node_table_update(const uint8_t src_id[MESH_SRC_ID_LEN],
                            const beacon_payload_t *beacon,
                            int8_t rssi, int8_t snr, uint8_t hop)
{
    if (!s_table || !s_mutex || !beacon) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int      free_slot = -1;
    uint32_t ts        = now_ms();

    for (int i = 0; i < NODE_TABLE_MAX_NODES; i++) {
        if (!s_table[i].active) {
            if (free_slot < 0) free_slot = i;
            continue;
        }
        if (memcmp(s_table[i].node_id, src_id, MESH_SRC_ID_LEN) == 0) {
            /* Nó já existe — atualiza */
            memcpy(&s_table[i].beacon, beacon, sizeof(beacon_payload_t));
            s_table[i].rssi         = rssi;
            s_table[i].snr          = snr;
            s_table[i].last_hop     = hop;
            s_table[i].last_seen_ms = ts;
            xSemaphoreGive(s_mutex);
            return ESP_OK;
        }
    }

    /* Nó novo */
    if (free_slot < 0) {
        xSemaphoreGive(s_mutex);
        ESP_LOGW(TAG, "Tabela cheia — nó %.2X:%.2X:%.2X:%.2X:%.2X:%.2X descartado",
                 src_id[0], src_id[1], src_id[2],
                 src_id[3], src_id[4], src_id[5]);
        return ESP_ERR_NO_MEM;
    }

    memcpy(s_table[free_slot].node_id, src_id, MESH_SRC_ID_LEN);
    memcpy(&s_table[free_slot].beacon, beacon, sizeof(beacon_payload_t));
    s_table[free_slot].rssi         = rssi;
    s_table[free_slot].snr          = snr;
    s_table[free_slot].last_hop     = hop;
    s_table[free_slot].last_seen_ms = ts;
    s_table[free_slot].active       = true;

    ESP_LOGI(TAG, "Novo nó adicionado slot %d (%.2X:%.2X:%.2X:%.2X:%.2X:%.2X)",
             free_slot,
             src_id[0], src_id[1], src_id[2],
             src_id[3], src_id[4], src_id[5]);

    xSemaphoreGive(s_mutex);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * node_table_prune — remove entradas expiradas
 * ═══════════════════════════════════════════════════════════════════════════ */
void node_table_prune(void)
{
    if (!s_table || !s_mutex) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    uint32_t ts = now_ms();
    for (int i = 0; i < NODE_TABLE_MAX_NODES; i++) {
        if (!s_table[i].active) continue;
        uint32_t age = ts - s_table[i].last_seen_ms;
        if (age > NODE_TABLE_TIMEOUT_MS) {
            ESP_LOGI(TAG, "Nó slot %d expirado (%lu ms sem beacon)", i, (unsigned long)age);
            memset(&s_table[i], 0, sizeof(mesh_node_t));
        }
    }

    xSemaphoreGive(s_mutex);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * node_table_get_all — snapshot thread-safe
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t node_table_get_all(mesh_node_t *out, size_t *out_count)
{
    if (!out || !out_count) return ESP_ERR_INVALID_ARG;
    if (!s_table || !s_mutex) { *out_count = 0; return ESP_ERR_INVALID_STATE; }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    size_t count = 0;
    for (int i = 0; i < NODE_TABLE_MAX_NODES; i++) {
        if (s_table[i].active) {
            memcpy(&out[count++], &s_table[i], sizeof(mesh_node_t));
        }
    }

    xSemaphoreGive(s_mutex);
    *out_count = count;
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * node_table_get_by_id
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t node_table_get_by_id(const uint8_t node_id[MESH_SRC_ID_LEN],
                                mesh_node_t *out)
{
    if (!node_id || !out)              return ESP_ERR_INVALID_ARG;
    if (!s_table || !s_mutex)          return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < NODE_TABLE_MAX_NODES; i++) {
        if (s_table[i].active &&
            memcmp(s_table[i].node_id, node_id, MESH_SRC_ID_LEN) == 0) {
            memcpy(out, &s_table[i], sizeof(mesh_node_t));
            xSemaphoreGive(s_mutex);
            return ESP_OK;
        }
    }

    xSemaphoreGive(s_mutex);
    return ESP_ERR_NOT_FOUND;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * node_table_count
 * ═══════════════════════════════════════════════════════════════════════════ */
size_t node_table_count(void)
{
    if (!s_table || !s_mutex) return 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    size_t n = 0;
    for (int i = 0; i < NODE_TABLE_MAX_NODES; i++) {
        if (s_table[i].active) n++;
    }
    xSemaphoreGive(s_mutex);
    return n;
}

/**
 * @file mesh_relay.c
 * @brief Relay de pacotes mesh com deduplicação.
 *
 * Fluxo:
 *   1. ISR DIO0 do LoRa → lora_rx_cb → mesh_relay_rx_cb (enfileira raw bytes)
 *   2. mesh_relay_task  → deserializa → verifica dedup
 *   3. Se BEACON: atualiza node_table
 *   4. Se hop < MAX_HOPS e não é duplicata: incrementa hop, re-serializa, lora_send
 *   5. Periodicamente chama node_table_prune()
 */

#include "mesh_relay.h"
#include "node_table.h"
#include "mesh_protocol.h"
#include "meshtracker_config.h"
#include "drivers/lora_sx1278.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "mesh_relay";

/* ─── Estrutura enfileirada pela callback de RX ────────────────────────── */
typedef struct {
    uint8_t data[MESH_MAX_PKT_LEN];
    size_t  len;
    int8_t  rssi;
    int8_t  snr;
} rx_item_t;

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static QueueHandle_t s_rx_queue  = NULL;
static TaskHandle_t  s_task_hdl  = NULL;

typedef struct {
    uint8_t  src_id[MESH_SRC_ID_LEN];
    uint16_t pkt_id;
    uint32_t timestamp_ms;
} dedup_entry_t;

static dedup_entry_t s_dedup[MESH_DEDUP_TABLE_SIZE];

/* ─── Helper: timestamp ─────────────────────────────────────────────────────*/
static inline uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_relay_is_duplicate
 * ═══════════════════════════════════════════════════════════════════════════ */
bool mesh_relay_is_duplicate(const uint8_t src_id[MESH_SRC_ID_LEN],
                             uint16_t pkt_id)
{
    uint32_t ts = now_ms();
    for (int i = 0; i < MESH_DEDUP_TABLE_SIZE; i++) {
        if (s_dedup[i].timestamp_ms == 0) continue;
        /* Expirado */
        if ((ts - s_dedup[i].timestamp_ms) > MESH_DEDUP_TTL_MS) {
            s_dedup[i].timestamp_ms = 0;
            continue;
        }
        if (s_dedup[i].pkt_id == pkt_id &&
            memcmp(s_dedup[i].src_id, src_id, MESH_SRC_ID_LEN) == 0) {
            return true;
        }
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_relay_register
 * ═══════════════════════════════════════════════════════════════════════════ */
void mesh_relay_register(const uint8_t src_id[MESH_SRC_ID_LEN],
                         uint16_t pkt_id)
{
    uint32_t ts      = now_ms();
    int      oldest  = 0;
    uint32_t min_ts  = s_dedup[0].timestamp_ms;

    for (int i = 0; i < MESH_DEDUP_TABLE_SIZE; i++) {
        /* Slot vazio ou expirado — reutiliza imediatamente */
        if (s_dedup[i].timestamp_ms == 0 ||
            (ts - s_dedup[i].timestamp_ms) > MESH_DEDUP_TTL_MS) {
            memcpy(s_dedup[i].src_id, src_id, MESH_SRC_ID_LEN);
            s_dedup[i].pkt_id       = pkt_id;
            s_dedup[i].timestamp_ms = ts;
            return;
        }
        if (s_dedup[i].timestamp_ms < min_ts) {
            min_ts = s_dedup[i].timestamp_ms;
            oldest = i;
        }
    }

    /* Tabela cheia — substitui a entrada mais antiga */
    memcpy(s_dedup[oldest].src_id, src_id, MESH_SRC_ID_LEN);
    s_dedup[oldest].pkt_id       = pkt_id;
    s_dedup[oldest].timestamp_ms = ts;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_relay_rx_cb — chamado pela task de RX do LoRa (não é ISR direta)
 * Enfileira o pacote bruto para processamento pela relay task.
 * ═══════════════════════════════════════════════════════════════════════════ */
void mesh_relay_rx_cb(const uint8_t *data, size_t len, int8_t rssi, int8_t snr)
{
    if (!s_rx_queue || len == 0 || len > MESH_MAX_PKT_LEN) return;

    rx_item_t item;
    memcpy(item.data, data, len);
    item.len  = len;
    item.rssi = rssi;
    item.snr  = snr;

    if (xQueueSend(s_rx_queue, &item, 0) != pdTRUE) {
        ESP_LOGW(TAG, "RX queue cheia — pacote descartado");
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_relay_task
 * ═══════════════════════════════════════════════════════════════════════════ */
void mesh_relay_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "mesh_relay_task iniciada (Core %d)", xPortGetCoreID());

    rx_item_t    item;
    mesh_packet_t pkt;
    static uint8_t tx_buf[MESH_MAX_PKT_LEN];
    uint32_t last_prune = now_ms();

    while (1) {
        /* Aguarda pacote ou timeout de prune */
        if (xQueueReceive(s_rx_queue, &item, MS_TO_TICKS(10000)) == pdTRUE) {

            /* 1. Deserializa */
            if (mesh_proto_deserialize(item.data, item.len, &pkt) != ESP_OK) {
                continue;
            }

            pkt.rssi = item.rssi;
            pkt.snr  = item.snr;

            /* 2. Deduplicação */
            if (mesh_relay_is_duplicate(pkt.hdr.src_id, pkt.hdr.pkt_id)) {
                ESP_LOGD(TAG, "Duplicata descartada PKT_ID=0x%04X", pkt.hdr.pkt_id);
                continue;
            }
            mesh_relay_register(pkt.hdr.src_id, pkt.hdr.pkt_id);

            /* 3. Atualiza node_table se for BEACON */
            if (pkt.hdr.type == MESH_PKT_BEACON &&
                pkt.payload_len >= sizeof(beacon_payload_t)) {
                beacon_payload_t beacon;
                if (mesh_proto_decode_beacon(pkt.payload, pkt.payload_len,
                                             &beacon) == ESP_OK) {
                    node_table_update(pkt.hdr.src_id, &beacon,
                                      pkt.rssi, pkt.snr, pkt.hdr.hop);
                }
            }

            /* 4. Relay se hop < MAX */
            if (pkt.hdr.hop < MESH_MAX_HOPS) {
                pkt.hdr.hop++;

                /* Atraso para desempate de colisões */
                vTaskDelay(MS_TO_TICKS(MESH_RELAY_DELAY_MS));

                size_t tx_len = 0;
                if (mesh_proto_serialize(&pkt, tx_buf, sizeof(tx_buf),
                                         &tx_len) == ESP_OK) {
                    lora_send(tx_buf, tx_len, NULL);
                    ESP_LOGD(TAG, "Relay PKT_ID=0x%04X hop=%d rssi=%d",
                             pkt.hdr.pkt_id, pkt.hdr.hop, pkt.rssi);
                }
            }
        }

        /* Prune periódico da tabela de nós (a cada ~30s) */
        if ((now_ms() - last_prune) >= 30000) {
            node_table_prune();
            last_prune = now_ms();
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_relay_init
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t mesh_relay_init(void)
{
    ESP_LOGI(TAG, "=== mesh_relay_init START ===");

    memset(s_dedup, 0, sizeof(s_dedup));

    s_rx_queue = xQueueCreate(8, sizeof(rx_item_t));
    if (!s_rx_queue) {
        ESP_LOGE(TAG, "Falha ao criar rx_queue");
        return ESP_ERR_NO_MEM;
    }

    /* Registra callback no driver LoRa */
    lora_start_rx(mesh_relay_rx_cb);

    BaseType_t rc = xTaskCreatePinnedToCore(
        mesh_relay_task, "mesh_relay",
        TASK_MESH_RELAY_STACK, NULL,
        TASK_MESH_RELAY_PRIO, &s_task_hdl,
        TASK_MESH_RELAY_CORE);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar mesh_relay_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "=== mesh_relay_init OK ===");
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_relay_deinit
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t mesh_relay_deinit(void)
{
    lora_stop_rx();
    if (s_task_hdl) { vTaskDelete(s_task_hdl); s_task_hdl = NULL; }
    if (s_rx_queue) { vQueueDelete(s_rx_queue); s_rx_queue = NULL; }
    return ESP_OK;
}

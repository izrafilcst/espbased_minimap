/**
 * @file mesh_beacon.c
 * @brief Beacon periódico com posição GPS do nó local.
 *
 * Lê o MAC WiFi STA do chip como SRC_ID (imutável).
 * A cada MESH_BEACON_INTERVAL_MS:
 *   1. Lê gps_get_data() — usa posição se válida, zeros se sem fix.
 *   2. Monta beacon_payload_t com posição + nome curto do nó.
 *   3. Serializa em mesh_packet_t e transmite via lora_send().
 */

#include "mesh_beacon.h"
#include "mesh_protocol.h"
#include "meshtracker_config.h"
#include "drivers/gps_neo6m.h"
#include "drivers/lora_sx1278.h"
#include "utils/mac_utils.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "mesh_beacon";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static uint8_t       s_src_id[MESH_SRC_ID_LEN] = {0};
static volatile uint16_t s_pkt_id              = 0;
static TaskHandle_t  s_task_hdl                = NULL;
static volatile bool s_send_now                = false;

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_beacon_task
 * ═══════════════════════════════════════════════════════════════════════════ */
void mesh_beacon_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "mesh_beacon_task iniciada (Core %d)", xPortGetCoreID());

    static uint8_t      tx_buf[MESH_MAX_PKT_LEN];
    static mesh_packet_t pkt;

    while (1) {
        /* Aguarda intervalo ou envio forçado */
        TickType_t wait = MS_TO_TICKS(MESH_BEACON_INTERVAL_MS);
        xTaskNotifyWait(0, 0, NULL, wait);

        /* Monta beacon payload */
        beacon_payload_t beacon;
        memset(&beacon, 0, sizeof(beacon));

        gps_data_t gps;
        if (gps_get_data(&gps) == ESP_OK) {
            beacon.latitude    = gps.latitude;
            beacon.longitude   = gps.longitude;
            beacon.altitude_m  = gps.altitude_m;
            beacon.speed_kmh   = gps.speed_kmh;
            beacon.satellites  = gps.satellites;
            beacon.fix_quality = gps.fix_quality;
        }

        /* Nome curto: últimos 3 bytes do MAC em hex ("DDEEFF") */
        mac_to_short_name(s_src_id, beacon.name, sizeof(beacon.name));

        /* Monta pacote mesh */
        memset(&pkt, 0, sizeof(pkt));
        pkt.hdr.magic  = MESH_MAGIC;
        memcpy(pkt.hdr.src_id, s_src_id, MESH_SRC_ID_LEN);
        pkt.hdr.hop    = 0;
        pkt.hdr.pkt_id = ++s_pkt_id;
        pkt.hdr.type   = MESH_PKT_BEACON;

        size_t payload_len = 0;
        if (mesh_proto_encode_beacon(&beacon, pkt.payload,
                                     sizeof(pkt.payload),
                                     &payload_len) != ESP_OK) {
            ESP_LOGW(TAG, "encode_beacon falhou");
            continue;
        }
        pkt.payload_len = (uint16_t)payload_len;

        /* Serializa */
        size_t tx_len = 0;
        if (mesh_proto_serialize(&pkt, tx_buf, sizeof(tx_buf), &tx_len) != ESP_OK) {
            ESP_LOGW(TAG, "serialize falhou");
            continue;
        }

        /* Transmite */
        esp_err_t ret = lora_send(tx_buf, tx_len, NULL);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Beacon TX PKT_ID=0x%04X nome=%s lat=%.6f lon=%.6f fix=%d",
                     pkt.hdr.pkt_id, beacon.name,
                     beacon.latitude, beacon.longitude,
                     beacon.fix_quality);
        } else {
            ESP_LOGW(TAG, "lora_send falhou: %s", esp_err_to_name(ret));
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_beacon_init
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t mesh_beacon_init(void)
{
    ESP_LOGI(TAG, "=== mesh_beacon_init START ===");

    /* Lê MAC WiFi STA como identificador permanente do nó */
    esp_read_mac(s_src_id, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "SRC_ID (MAC): %02X:%02X:%02X:%02X:%02X:%02X",
             s_src_id[0], s_src_id[1], s_src_id[2],
             s_src_id[3], s_src_id[4], s_src_id[5]);

    BaseType_t rc = xTaskCreatePinnedToCore(
        mesh_beacon_task, "mesh_beacon",
        TASK_MESH_BEACON_STACK, NULL,
        TASK_MESH_BEACON_PRIO, &s_task_hdl,
        TASK_MESH_BEACON_CORE);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar mesh_beacon_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "=== mesh_beacon_init OK (intervalo %dms) ===",
             MESH_BEACON_INTERVAL_MS);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_beacon_deinit
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t mesh_beacon_deinit(void)
{
    if (s_task_hdl) { vTaskDelete(s_task_hdl); s_task_hdl = NULL; }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_beacon_send_now — força envio imediato via notificação de task
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t mesh_beacon_send_now(void)
{
    if (!s_task_hdl) return ESP_ERR_INVALID_STATE;
    xTaskNotify(s_task_hdl, 0, eNoAction);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Getters
 * ═══════════════════════════════════════════════════════════════════════════ */
void mesh_beacon_get_src_id(uint8_t out[MESH_SRC_ID_LEN])
{
    memcpy(out, s_src_id, MESH_SRC_ID_LEN);
}

uint16_t mesh_beacon_next_pkt_id(void)
{
    return ++s_pkt_id;
}

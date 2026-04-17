/**
 * @file mesh_protocol.c
 * @brief Serialização, deserialização e CRC16-CCITT do protocolo mesh.
 *
 * Layout binário (little-endian):
 *   [MAGIC 2B][SRC_ID 6B][HOP 1B][PKT_ID 2B][TYPE 1B][PAYLOAD 0-200B][CRC16 2B]
 *
 * CRC16-CCITT (poly 0x1021, init 0xFFFF) sobre todos os bytes
 * desde MAGIC até o último byte do PAYLOAD (exclusive CRC).
 */

#include "mesh_protocol.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "mesh_protocol";

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_proto_crc16 — CRC16-CCITT (poly=0x1021, init=0xFFFF)
 * ═══════════════════════════════════════════════════════════════════════════ */
uint16_t mesh_proto_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (uint16_t)((crc << 1) ^ 0x1021);
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_proto_check_crc
 * ═══════════════════════════════════════════════════════════════════════════ */
bool mesh_proto_check_crc(const uint8_t *buf, size_t buf_len)
{
    if (buf_len < MESH_HEADER_LEN + MESH_CRC_LEN) return false;

    size_t   data_len  = buf_len - MESH_CRC_LEN;
    uint16_t calc      = mesh_proto_crc16(buf, data_len);
    uint16_t declared  = (uint16_t)buf[data_len]
                       | ((uint16_t)buf[data_len + 1] << 8);
    return calc == declared;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_proto_serialize
 *
 * Monta: [MAGIC(2)][SRC_ID(6)][HOP(1)][PKT_ID(2)][TYPE(1)][PAYLOAD][CRC(2)]
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t mesh_proto_serialize(const mesh_packet_t *pkt,
                               uint8_t *buf, size_t buf_len,
                               size_t *out_len)
{
    if (!pkt || !buf || !out_len) return ESP_ERR_INVALID_ARG;

    size_t total = MESH_HEADER_LEN + pkt->payload_len + MESH_CRC_LEN;
    if (total > buf_len) {
        ESP_LOGW(TAG, "serialize: buffer insuficiente (%zu < %zu)", buf_len, total);
        return ESP_ERR_INVALID_SIZE;
    }

    size_t pos = 0;

    /* MAGIC — little-endian */
    buf[pos++] = (uint8_t)(MESH_MAGIC & 0xFF);
    buf[pos++] = (uint8_t)(MESH_MAGIC >> 8);

    /* SRC_ID */
    memcpy(&buf[pos], pkt->hdr.src_id, MESH_SRC_ID_LEN);
    pos += MESH_SRC_ID_LEN;

    /* HOP */
    buf[pos++] = pkt->hdr.hop;

    /* PKT_ID — little-endian */
    buf[pos++] = (uint8_t)(pkt->hdr.pkt_id & 0xFF);
    buf[pos++] = (uint8_t)(pkt->hdr.pkt_id >> 8);

    /* TYPE */
    buf[pos++] = pkt->hdr.type;

    /* PAYLOAD */
    if (pkt->payload_len > 0) {
        memcpy(&buf[pos], pkt->payload, pkt->payload_len);
        pos += pkt->payload_len;
    }

    /* CRC16 sobre MAGIC…PAYLOAD */
    uint16_t crc = mesh_proto_crc16(buf, pos);
    buf[pos++] = (uint8_t)(crc & 0xFF);
    buf[pos++] = (uint8_t)(crc >> 8);

    *out_len = pos;
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_proto_deserialize
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t mesh_proto_deserialize(const uint8_t *buf, size_t buf_len,
                                 mesh_packet_t *pkt)
{
    if (!buf || !pkt) return ESP_ERR_INVALID_ARG;

    if (buf_len < MESH_HEADER_LEN + MESH_CRC_LEN) {
        ESP_LOGW(TAG, "deserialize: pacote muito curto (%zu)", buf_len);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Valida MAGIC */
    uint16_t magic = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    if (magic != MESH_MAGIC) {
        ESP_LOGW(TAG, "deserialize: magic inválido 0x%04X", magic);
        return ESP_ERR_INVALID_RESPONSE;
    }

    /* Valida CRC */
    if (!mesh_proto_check_crc(buf, buf_len)) {
        ESP_LOGW(TAG, "deserialize: CRC inválido");
        return ESP_ERR_INVALID_RESPONSE;
    }

    memset(pkt, 0, sizeof(*pkt));

    size_t pos = 2; /* após MAGIC */

    pkt->hdr.magic = magic;
    memcpy(pkt->hdr.src_id, &buf[pos], MESH_SRC_ID_LEN);
    pos += MESH_SRC_ID_LEN;

    pkt->hdr.hop    = buf[pos++];
    pkt->hdr.pkt_id = (uint16_t)buf[pos] | ((uint16_t)buf[pos + 1] << 8);
    pos += 2;
    pkt->hdr.type   = buf[pos++];

    /* Payload = tudo entre cabeçalho e CRC */
    size_t payload_len = buf_len - MESH_HEADER_LEN - MESH_CRC_LEN;
    if (payload_len > MESH_MAX_PAYLOAD_BYTES) {
        ESP_LOGW(TAG, "deserialize: payload muito grande (%zu)", payload_len);
        return ESP_ERR_INVALID_RESPONSE;
    }

    pkt->payload_len = (uint16_t)payload_len;
    if (payload_len > 0) {
        memcpy(pkt->payload, &buf[pos], payload_len);
    }

    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_proto_encode_beacon
 *
 * Serializa beacon_payload_t em bytes para o campo payload do pacote.
 * Layout: lat(8) lon(8) alt(4) spd(4) sat(1) fix(1) bat(1) name(12) = 39 B
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t mesh_proto_encode_beacon(const beacon_payload_t *beacon,
                                   uint8_t *buf, size_t buf_len,
                                   size_t *out_len)
{
    if (!beacon || !buf || !out_len) return ESP_ERR_INVALID_ARG;

    const size_t BEACON_SIZE = sizeof(beacon_payload_t);
    if (buf_len < BEACON_SIZE) return ESP_ERR_INVALID_SIZE;

    memcpy(buf, beacon, BEACON_SIZE);
    *out_len = BEACON_SIZE;
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * mesh_proto_decode_beacon
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t mesh_proto_decode_beacon(const uint8_t *buf, size_t buf_len,
                                   beacon_payload_t *beacon)
{
    if (!buf || !beacon) return ESP_ERR_INVALID_ARG;
    if (buf_len < sizeof(beacon_payload_t)) return ESP_ERR_INVALID_SIZE;

    memcpy(beacon, buf, sizeof(beacon_payload_t));

    /* Garante null-termination do nome */
    beacon->name[sizeof(beacon->name) - 1] = '\0';
    return ESP_OK;
}

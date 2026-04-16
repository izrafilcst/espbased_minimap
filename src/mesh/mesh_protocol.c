/**
 * @file mesh_protocol.c
 * @brief Serialização, deserialização e CRC do protocolo mesh.
 */

#include "mesh_protocol.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "mesh_protocol";

esp_err_t mesh_proto_serialize(const mesh_packet_t *pkt,
                               uint8_t *buf, size_t buf_len,
                               size_t *out_len)
{
    (void)pkt; (void)buf; (void)buf_len;
    if (out_len) *out_len = 0;
    return ESP_OK;
}

esp_err_t mesh_proto_deserialize(const uint8_t *buf, size_t buf_len,
                                 mesh_packet_t *pkt)
{
    (void)buf; (void)buf_len; (void)pkt;
    return ESP_OK;
}

uint16_t mesh_proto_crc16(const uint8_t *data, size_t len)
{
    (void)data; (void)len;
    return 0x0000;
}

bool mesh_proto_check_crc(const uint8_t *buf, size_t buf_len)
{
    (void)buf; (void)buf_len;
    return true;
}

esp_err_t mesh_proto_encode_beacon(const beacon_payload_t *beacon,
                                   uint8_t *buf, size_t buf_len,
                                   size_t *out_len)
{
    (void)beacon; (void)buf; (void)buf_len;
    if (out_len) *out_len = 0;
    return ESP_OK;
}

esp_err_t mesh_proto_decode_beacon(const uint8_t *buf, size_t buf_len,
                                   beacon_payload_t *beacon)
{
    (void)buf; (void)buf_len; (void)beacon;
    return ESP_OK;
}

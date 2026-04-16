/**
 * @file mac_utils.c
 * @brief Utilitários de MAC address para o nó mesh.
 */

#include "mac_utils.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "esp_mac.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "mac_utils";

esp_err_t mac_get_local(uint8_t out[MESH_SRC_ID_LEN])
{
    return esp_read_mac(out, ESP_MAC_WIFI_STA);
}

void mac_to_string(const uint8_t mac[MESH_SRC_ID_LEN],
                   char *buf, size_t buf_size)
{
    if (!buf || buf_size < 18) return;
    snprintf(buf, buf_size, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void mac_to_short_name(const uint8_t mac[MESH_SRC_ID_LEN],
                       char *buf, size_t buf_size)
{
    if (!buf || buf_size < 7) return;
    snprintf(buf, buf_size, "%02X%02X%02X",
             mac[3], mac[4], mac[5]);
}

bool mac_equal(const uint8_t a[MESH_SRC_ID_LEN],
               const uint8_t b[MESH_SRC_ID_LEN])
{
    return (memcmp(a, b, MESH_SRC_ID_LEN) == 0);
}

void mac_copy(uint8_t dst[MESH_SRC_ID_LEN],
              const uint8_t src[MESH_SRC_ID_LEN])
{
    memcpy(dst, src, MESH_SRC_ID_LEN);
}

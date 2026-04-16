#ifndef MAC_UTILS_H
#define MAC_UTILS_H

/**
 * @file mac_utils.h
 * @brief Utilitários para manipulação de endereços MAC.
 *        Formatação, comparação e leitura do MAC do ESP32-S3.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "meshtracker_config.h"

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Lê o MAC address base do ESP32-S3 (WiFi STA interface).
 *        Usado como SRC_ID do nó mesh.
 * @param out Buffer de 6 bytes para receber o MAC.
 * @return ESP_OK em sucesso.
 */
esp_err_t mac_get_local(uint8_t out[MESH_SRC_ID_LEN]);

/**
 * @brief Formata um MAC de 6 bytes em string "XX:XX:XX:XX:XX:XX".
 * @param mac      MAC de 6 bytes.
 * @param buf      Buffer de saída (mínimo 18 bytes).
 * @param buf_size Tamanho do buffer.
 */
void mac_to_string(const uint8_t mac[MESH_SRC_ID_LEN],
                   char *buf, size_t buf_size);

/**
 * @brief Formata os 3 últimos bytes do MAC como "XXXXXX" (nome curto do nó).
 *        Exemplo: MAC AA:BB:CC:DD:EE:FF → "DDEEFF".
 * @param mac      MAC de 6 bytes.
 * @param buf      Buffer de saída (mínimo 7 bytes).
 * @param buf_size Tamanho do buffer.
 */
void mac_to_short_name(const uint8_t mac[MESH_SRC_ID_LEN],
                       char *buf, size_t buf_size);

/**
 * @brief Compara dois endereços MAC.
 * @return true se iguais.
 */
bool mac_equal(const uint8_t a[MESH_SRC_ID_LEN],
               const uint8_t b[MESH_SRC_ID_LEN]);

/**
 * @brief Copia um MAC de origem para destino.
 * @param dst Destino.
 * @param src Origem.
 */
void mac_copy(uint8_t dst[MESH_SRC_ID_LEN],
              const uint8_t src[MESH_SRC_ID_LEN]);

#endif /* MAC_UTILS_H */

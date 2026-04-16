#ifndef MESH_BEACON_H
#define MESH_BEACON_H

/**
 * @file mesh_beacon.h
 * @brief Módulo de beacon periódico do nó local.
 *        Publica posição GPS + status do nó via LoRa a cada
 *        MESH_BEACON_INTERVAL_MS milissegundos.
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "mesh_protocol.h"
#include "meshtracker_config.h"

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Inicializa o módulo de beacon, obtém o MAC do ESP32-S3 como
 *        SRC_ID do nó e cria a task de beacon no Core 0.
 * @return ESP_OK em sucesso.
 */
esp_err_t mesh_beacon_init(void);

/**
 * @brief Para a task de beacon e libera recursos.
 * @return ESP_OK em sucesso.
 */
esp_err_t mesh_beacon_deinit(void);

/**
 * @brief Força o envio imediato de um beacon, independente do intervalo.
 *        Útil após mudança de posição significativa.
 * @return ESP_OK se beacon enfileirado para transmissão.
 */
esp_err_t mesh_beacon_send_now(void);

/**
 * @brief Retorna o SRC_ID (MAC) deste nó (6 bytes).
 * @param out Buffer de 6 bytes para receber o MAC.
 */
void mesh_beacon_get_src_id(uint8_t out[MESH_SRC_ID_LEN]);

/**
 * @brief Retorna o próximo PKT_ID único (incrementa atomicamente).
 */
uint16_t mesh_beacon_next_pkt_id(void);

/**
 * @brief Task interna de beacon periódico — NÃO chamar diretamente.
 */
void mesh_beacon_task(void *arg);

#endif /* MESH_BEACON_H */

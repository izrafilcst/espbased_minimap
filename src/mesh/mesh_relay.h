#ifndef MESH_RELAY_H
#define MESH_RELAY_H

/**
 * @file mesh_relay.h
 * @brief Módulo de relay de pacotes mesh.
 *        Recebe pacotes via LoRa, filtra duplicatas (dedup table),
 *        incrementa hop count e retransmite se hop < MESH_MAX_HOPS.
 *        Atualiza a node_table quando recebe beacons.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "mesh_protocol.h"
#include "meshtracker_config.h"

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Inicializa o módulo relay: aloca tabela de deduplicação,
 *        registra callback de RX no driver LoRa e cria task de relay
 *        pinada no Core 0.
 * @return ESP_OK em sucesso.
 */
esp_err_t mesh_relay_init(void);

/**
 * @brief Para a task de relay e libera recursos.
 * @return ESP_OK em sucesso.
 */
esp_err_t mesh_relay_deinit(void);

/**
 * @brief Callback chamado pelo driver LoRa quando um pacote é recebido.
 *        Enfileira o pacote para processamento pela task de relay.
 *        Executado no contexto da ISR — deve ser rápido.
 * @param data  Buffer de bytes recebidos.
 * @param len   Tamanho do buffer.
 * @param rssi  RSSI do pacote (dBm).
 * @param snr   SNR do pacote (dB).
 */
void mesh_relay_rx_cb(const uint8_t *data, size_t len, int8_t rssi, int8_t snr);

/**
 * @brief Verifica se um pacote já foi visto recentemente (deduplicação).
 *        Usa SRC_ID + PKT_ID como chave.
 * @param src_id MAC do nó origem.
 * @param pkt_id Identificador do pacote.
 * @return true se é duplicata (não relayt), false se é novo.
 */
bool mesh_relay_is_duplicate(const uint8_t src_id[MESH_SRC_ID_LEN],
                             uint16_t pkt_id);

/**
 * @brief Registra um pacote na tabela de deduplicação.
 * @param src_id MAC do nó origem.
 * @param pkt_id Identificador do pacote.
 */
void mesh_relay_register(const uint8_t src_id[MESH_SRC_ID_LEN],
                         uint16_t pkt_id);

/**
 * @brief Task interna de processamento de relay — NÃO chamar diretamente.
 */
void mesh_relay_task(void *arg);

#endif /* MESH_RELAY_H */

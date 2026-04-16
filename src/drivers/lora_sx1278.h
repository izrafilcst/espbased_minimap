#ifndef LORA_SX1278_H
#define LORA_SX1278_H

/**
 * @file lora_sx1278.h
 * @brief Driver para módulo LoRa RA-02 (SX1278) via SPI3.
 *        Frequência 433 MHz, SF9, BW125, CR4/5, SYNC 0x34.
 *        DIO0 = IRQ TxDone/RxDone via GPIO interrupt.
 *
 * ATENÇÃO HARDWARE:
 *   - Alimentar com 3.3 V SOMENTE. 5 V danifica o chip.
 *   - NUNCA transmitir sem antena conectada.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "meshtracker_config.h"

/* ─── Callback types ─────────────────────────────────────────────────────── */

/** Callback chamado pela ISR quando um pacote é recebido. */
typedef void (*lora_rx_cb_t)(const uint8_t *data, size_t len, int8_t rssi, int8_t snr);

/** Callback chamado quando transmissão é concluída. */
typedef void (*lora_tx_done_cb_t)(void);

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Inicializa SPI3, reseta o SX1278 e configura todos os parâmetros
 *        RF (frequência, SF, BW, CR, sync word, potência, preâmbulo, CRC).
 *        Instala a ISR no DIO0 para recepção e transmissão.
 * @return ESP_OK em sucesso, erro caso o SX1278 não responda.
 */
esp_err_t lora_init(void);

/**
 * @brief Libera recursos SPI e ISR. Coloca SX1278 em modo sleep.
 * @return ESP_OK em sucesso.
 */
esp_err_t lora_deinit(void);

/**
 * @brief Envia um pacote LoRa de forma assíncrona.
 *        Bloqueia até o módulo estar pronto (saindo de recepção).
 *        Chama tx_done_cb quando a transmissão terminar.
 * @param data      Buffer de dados a transmitir.
 * @param len       Tamanho em bytes (max MESH_MAX_PKT_LEN).
 * @param tx_done_cb Callback de conclusão (pode ser NULL).
 * @return ESP_OK se enfileirado, ESP_ERR_INVALID_SIZE se len > máximo.
 */
esp_err_t lora_send(const uint8_t *data, size_t len, lora_tx_done_cb_t tx_done_cb);

/**
 * @brief Coloca o SX1278 em modo de recepção contínua.
 *        Pacotes recebidos disparam rx_cb via ISR (DIO0).
 * @param rx_cb Callback chamado com o pacote recebido.
 * @return ESP_OK em sucesso.
 */
esp_err_t lora_start_rx(lora_rx_cb_t rx_cb);

/**
 * @brief Para a recepção contínua (coloca em standby).
 * @return ESP_OK em sucesso.
 */
esp_err_t lora_stop_rx(void);

/**
 * @brief Lê o RSSI atual do canal (em dBm).
 * @param rssi_out Ponteiro para armazenar o RSSI.
 * @return ESP_OK em sucesso.
 */
esp_err_t lora_read_rssi(int8_t *rssi_out);

/**
 * @brief Retorna true se o módulo está atualmente transmitindo.
 */
bool lora_is_transmitting(void);

/**
 * @brief Reseta o SX1278 via GPIO RST (ciclo de 10ms).
 * @return ESP_OK em sucesso.
 */
esp_err_t lora_reset(void);

#endif /* LORA_SX1278_H */

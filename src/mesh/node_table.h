#ifndef NODE_TABLE_H
#define NODE_TABLE_H

/**
 * @file node_table.h
 * @brief Tabela de nós mesh visíveis (até NODE_TABLE_MAX_NODES).
 *        Cada entrada é atualizada ao receber um beacon.
 *        Entradas expiradas (> NODE_TABLE_TIMEOUT_MS) são removidas.
 *        Thread-safe via mutex interno.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "mesh_protocol.h"
#include "meshtracker_config.h"

/* ─── Tipos públicos ─────────────────────────────────────────────────────── */

/**
 * @brief Entrada na tabela de nós conhecidos.
 */
typedef struct {
    uint8_t         node_id[MESH_SRC_ID_LEN]; /**< MAC do nó              */
    beacon_payload_t beacon;                   /**< Último beacon recebido */
    int8_t          rssi;                      /**< RSSI do último pacote  */
    int8_t          snr;                       /**< SNR do último pacote   */
    uint8_t         last_hop;                  /**< Hops do último pacote  */
    uint32_t        last_seen_ms;              /**< Timestamp último beacon */
    bool            active;                    /**< Entrada em uso         */
} mesh_node_t;

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Inicializa a tabela de nós, alocando memória na PSRAM.
 *        Cria o mutex interno de proteção.
 * @return ESP_OK em sucesso, ESP_ERR_NO_MEM se PSRAM indisponível.
 */
esp_err_t node_table_init(void);

/**
 * @brief Libera recursos da tabela.
 * @return ESP_OK em sucesso.
 */
esp_err_t node_table_deinit(void);

/**
 * @brief Atualiza ou insere um nó na tabela com base no beacon recebido.
 *        Se o nó já existe, atualiza beacon, RSSI, SNR e timestamp.
 *        Se é novo e há espaço, cria nova entrada.
 * @param src_id   MAC do nó origem.
 * @param beacon   Payload do beacon decodificado.
 * @param rssi     RSSI medido.
 * @param snr      SNR medido.
 * @param hop      Número de hops do pacote.
 * @return ESP_OK em sucesso, ESP_ERR_NO_MEM se tabela cheia.
 */
esp_err_t node_table_update(const uint8_t src_id[MESH_SRC_ID_LEN],
                            const beacon_payload_t *beacon,
                            int8_t rssi, int8_t snr, uint8_t hop);

/**
 * @brief Remove entradas expiradas (last_seen_ms > NODE_TABLE_TIMEOUT_MS).
 *        Chamar periodicamente da task de rede.
 */
void node_table_prune(void);

/**
 * @brief Obtém snapshot da tabela inteira de forma thread-safe.
 * @param out      Buffer para copiar as entradas (NODE_TABLE_MAX_NODES).
 * @param out_count Número de entradas ativas copiadas.
 * @return ESP_OK em sucesso.
 */
esp_err_t node_table_get_all(mesh_node_t *out, size_t *out_count);

/**
 * @brief Obtém um único nó pelo MAC.
 * @param node_id  MAC do nó desejado.
 * @param out      Ponteiro para receber a cópia da entrada.
 * @return ESP_OK se encontrado, ESP_ERR_NOT_FOUND se ausente.
 */
esp_err_t node_table_get_by_id(const uint8_t node_id[MESH_SRC_ID_LEN],
                                mesh_node_t *out);

/**
 * @brief Retorna o número de nós ativos na tabela.
 *        Thread-safe.
 */
size_t node_table_count(void);

#endif /* NODE_TABLE_H */

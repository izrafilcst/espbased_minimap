#ifndef MESH_PROTOCOL_H
#define MESH_PROTOCOL_H

/**
 * @file mesh_protocol.h
 * @brief Definição do protocolo de pacotes da rede mesh LoRa.
 *        Pacote: [MAGIC 2B][SRC_ID 6B][HOP 1B][PKT_ID 2B][TYPE 1B]
 *                [PAYLOAD 0-200B][CRC16 2B]
 *
 *        Funções de serialização, deserialização e validação de CRC.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "meshtracker_config.h"

/* ─── Tipos públicos ─────────────────────────────────────────────────────── */

/**
 * @brief Cabeçalho deserializado de um pacote mesh.
 */
typedef struct {
    uint16_t magic;                          /**< Deve ser MESH_MAGIC       */
    uint8_t  src_id[MESH_SRC_ID_LEN];        /**< MAC do nó origem          */
    uint8_t  hop;                            /**< Contador de hops (0..5)   */
    uint16_t pkt_id;                         /**< Identificador único       */
    uint8_t  type;                           /**< Tipo: MESH_PKT_*          */
} mesh_header_t;

/**
 * @brief Pacote completo com cabeçalho e payload.
 *        Alocado na DRAM/PSRAM conforme tamanho.
 */
typedef struct {
    mesh_header_t hdr;
    uint8_t       payload[MESH_MAX_PAYLOAD_BYTES];
    uint16_t      payload_len;
    int8_t        rssi;  /**< RSSI medido na recepção (dBm)                 */
    int8_t        snr;   /**< SNR medido na recepção (dB)                   */
} mesh_packet_t;

/**
 * @brief Payload de um pacote BEACON (tipo MESH_PKT_BEACON).
 */
typedef struct {
    double   latitude;   /**< Graus decimais                                */
    double   longitude;  /**< Graus decimais                                */
    float    altitude_m; /**< Altitude em metros                            */
    float    speed_kmh;  /**< Velocidade em km/h                            */
    uint8_t  satellites; /**< Nº de satélites                               */
    uint8_t  fix_quality;/**< Qualidade do fix GPS                          */
    uint8_t  battery_pct;/**< Nível de bateria 0–100%                       */
    char     name[12];   /**< Nome do nó (null-terminated)                  */
} beacon_payload_t;

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Serializa um mesh_packet_t em buffer de bytes pronto para
 *        transmissão via LoRa. Inclui cálculo e append do CRC16.
 * @param pkt     Pacote a serializar.
 * @param buf     Buffer de saída (tamanho mínimo MESH_MAX_PKT_LEN).
 * @param buf_len Tamanho do buffer de saída.
 * @param out_len Número de bytes escritos.
 * @return ESP_OK em sucesso, ESP_ERR_INVALID_SIZE se buffer insuficiente.
 */
esp_err_t mesh_proto_serialize(const mesh_packet_t *pkt,
                               uint8_t *buf, size_t buf_len,
                               size_t *out_len);

/**
 * @brief Deserializa buffer de bytes recebido em mesh_packet_t.
 *        Valida magic e CRC16 antes de preencher a struct.
 * @param buf     Buffer de bytes recebido.
 * @param buf_len Tamanho do buffer.
 * @param pkt     Struct de saída.
 * @return ESP_OK se pacote válido,
 *         ESP_ERR_INVALID_RESPONSE se CRC ou magic inválidos.
 */
esp_err_t mesh_proto_deserialize(const uint8_t *buf, size_t buf_len,
                                 mesh_packet_t *pkt);

/**
 * @brief Calcula o CRC16-CCITT de um buffer.
 * @param data    Ponteiro para os dados.
 * @param len     Tamanho dos dados.
 * @return CRC16 calculado.
 */
uint16_t mesh_proto_crc16(const uint8_t *data, size_t len);

/**
 * @brief Verifica se o CRC16 ao final do buffer está correto.
 * @param buf     Buffer completo (header + payload + CRC).
 * @param buf_len Tamanho total.
 * @return true se CRC correto.
 */
bool mesh_proto_check_crc(const uint8_t *buf, size_t buf_len);

/**
 * @brief Serializa um beacon_payload_t em bytes para inserir no campo
 *        payload de um mesh_packet_t.
 * @param beacon  Payload de beacon.
 * @param buf     Buffer de saída.
 * @param buf_len Tamanho máximo do buffer.
 * @param out_len Bytes escritos.
 * @return ESP_OK em sucesso.
 */
esp_err_t mesh_proto_encode_beacon(const beacon_payload_t *beacon,
                                   uint8_t *buf, size_t buf_len,
                                   size_t *out_len);

/**
 * @brief Deserializa bytes de payload em beacon_payload_t.
 * @param buf     Buffer com payload codificado.
 * @param buf_len Tamanho do buffer.
 * @param beacon  Struct de saída.
 * @return ESP_OK em sucesso.
 */
esp_err_t mesh_proto_decode_beacon(const uint8_t *buf, size_t buf_len,
                                   beacon_payload_t *beacon);

#endif /* MESH_PROTOCOL_H */

#ifndef NMEA_PARSER_H
#define NMEA_PARSER_H

/**
 * @file nmea_parser.h
 * @brief Parser de sentenças NMEA 0183.
 *        Suporta GGA (posição, altitude, satélites) e
 *        RMC (posição, velocidade, rumo, data/hora).
 *        Sem alocação dinâmica — opera sobre buffers estáticos.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* ─── Tipos públicos ─────────────────────────────────────────────────────── */

/** Dados extraídos de uma sentença GGA */
typedef struct {
    double   latitude;    /**< Graus decimais (+ = Norte, - = Sul)         */
    double   longitude;   /**< Graus decimais (+ = Leste, - = Oeste)       */
    float    altitude_m;  /**< Altitude MSL em metros                      */
    uint8_t  fix_quality; /**< 0=inválido, 1=GPS, 2=DGPS                   */
    uint8_t  satellites;  /**< Número de satélites em uso                  */
    float    hdop;        /**< Horizontal Dilution of Precision             */
    uint8_t  hour;        /**< Hora UTC                                    */
    uint8_t  minute;      /**< Minuto UTC                                  */
    uint8_t  second;      /**< Segundo UTC                                 */
} nmea_gga_t;

/** Dados extraídos de uma sentença RMC */
typedef struct {
    double   latitude;    /**< Graus decimais                              */
    double   longitude;   /**< Graus decimais                              */
    float    speed_kmh;   /**< Velocidade sobre o solo em km/h             */
    float    course_deg;  /**< Rumo verdadeiro em graus                    */
    bool     valid;       /**< true = status 'A' (válido)                  */
    uint8_t  hour;        /**< Hora UTC                                    */
    uint8_t  minute;      /**< Minuto UTC                                  */
    uint8_t  second;      /**< Segundo UTC                                 */
    uint8_t  day;         /**< Dia UTC                                     */
    uint8_t  month;       /**< Mês UTC                                     */
    uint16_t year;        /**< Ano UTC                                     */
} nmea_rmc_t;

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Valida o checksum NMEA (*HH ao final da sentença).
 * @param sentence String da sentença NMEA (com '$' e '*HH').
 * @return true se checksum correto.
 */
bool nmea_validate_checksum(const char *sentence);

/**
 * @brief Parseia uma sentença $GPGGA ou $GNGGA em nmea_gga_t.
 * @param sentence Sentença NMEA (null-terminated).
 * @param out      Struct de saída.
 * @return ESP_OK se parseada com sucesso,
 *         ESP_ERR_INVALID_ARG se não é GGA,
 *         ESP_ERR_INVALID_RESPONSE se checksum inválido.
 */
esp_err_t nmea_parse_gga(const char *sentence, nmea_gga_t *out);

/**
 * @brief Parseia uma sentença $GPRMC ou $GNRMC em nmea_rmc_t.
 * @param sentence Sentença NMEA (null-terminated).
 * @param out      Struct de saída.
 * @return ESP_OK se parseada com sucesso,
 *         ESP_ERR_INVALID_ARG se não é RMC,
 *         ESP_ERR_INVALID_RESPONSE se checksum inválido.
 */
esp_err_t nmea_parse_rmc(const char *sentence, nmea_rmc_t *out);

/**
 * @brief Converte coordenada no formato NMEA (DDDMM.MMMM) para graus decimais.
 * @param nmea_coord Valor NMEA (ex: 2257.9148 para 22°57.9148').
 * @param direction  Caractere de direção: 'N', 'S', 'E', 'W'.
 * @return Graus decimais (negativo para S/W).
 */
double nmea_coord_to_decimal(double nmea_coord, char direction);

/**
 * @brief Detecta o tipo de sentença a partir do prefixo.
 * @param sentence Sentença NMEA.
 * @return Ponteiro para string com tipo ("GGA", "RMC", etc.) ou NULL.
 */
const char *nmea_sentence_type(const char *sentence);

#endif /* NMEA_PARSER_H */

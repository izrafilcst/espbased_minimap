#ifndef GPS_NEO6M_H
#define GPS_NEO6M_H

/**
 * @file gps_neo6m.h
 * @brief Driver para receptor GPS NEO-6M via UART1.
 *        Parseia sentenças NMEA (GGA, RMC) e disponibiliza
 *        posição, velocidade, altitude e número de satélites.
 *        Dados protegidos por mutex — use getter com mutex.
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "meshtracker_config.h"

/* ─── Tipos públicos ─────────────────────────────────────────────────────── */

/**
 * @brief Dados GPS processados. Protegidos internamente por mutex.
 */
typedef struct {
    double    latitude;     /**< Graus decimais, positivo = Norte          */
    double    longitude;    /**< Graus decimais, positivo = Leste          */
    float     altitude_m;   /**< Altitude em metros (MSL)                  */
    float     speed_kmh;    /**< Velocidade em km/h                        */
    float     course_deg;   /**< Rumo em graus (0–360)                     */
    uint8_t   satellites;   /**< Número de satélites usados no fix         */
    uint8_t   fix_quality;  /**< 0=sem fix, 1=GPS, 2=DGPS                 */
    bool      valid;        /**< true = dados válidos                      */
    uint32_t  timestamp_ms; /**< esp_timer_get_time() / 1000 no momento do fix */
} gps_data_t;

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Inicializa UART1 (9600 baud, 8N1), instala driver e cria
 *        task GPS no Core 0. Configura GPIO de PPS como entrada.
 * @return ESP_OK em sucesso.
 */
esp_err_t gps_init(void);

/**
 * @brief Para a task GPS e libera o driver UART1.
 * @return ESP_OK em sucesso.
 */
esp_err_t gps_deinit(void);

/**
 * @brief Copia os dados GPS mais recentes para o buffer fornecido.
 *        Thread-safe (usa mutex interno).
 * @param out  Ponteiro para gps_data_t que receberá os dados.
 * @return ESP_OK se dados válidos foram copiados,
 *         ESP_ERR_NOT_FOUND se ainda não há fix.
 */
esp_err_t gps_get_data(gps_data_t *out);

/**
 * @brief Retorna true se o GPS tem fix válido (fix_quality > 0 && valid).
 *        Thread-safe.
 */
bool gps_has_fix(void);

/**
 * @brief Tenta reconfigurar o NEO-6M para 115200 baud via mensagem UBX.
 *        Após envio, reinicia o UART1 na nova velocidade.
 * @return ESP_OK em sucesso.
 */
esp_err_t gps_set_baud_115200(void);

/**
 * @brief Task interna de recepção NMEA — NÃO chamar diretamente.
 *        Criada por gps_init() e pinada no Core 0.
 */
void gps_rx_task(void *arg);

#endif /* GPS_NEO6M_H */

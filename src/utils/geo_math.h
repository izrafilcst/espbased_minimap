#ifndef GEO_MATH_H
#define GEO_MATH_H

/**
 * @file geo_math.h
 * @brief Funções de matemática geográfica (Haversine, bearing, projeção).
 *        Usadas para calcular distâncias entre nós e posições relativas
 *        no mapa do display.
 */

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

/* ─── Constantes ─────────────────────────────────────────────────────────── */
#define GEO_EARTH_RADIUS_M  6371000.0  /**< Raio médio da Terra em metros   */

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Calcula a distância em metros entre dois pontos geográficos
 *        usando a fórmula de Haversine.
 * @param lat1  Latitude do ponto 1 (graus decimais).
 * @param lon1  Longitude do ponto 1 (graus decimais).
 * @param lat2  Latitude do ponto 2 (graus decimais).
 * @param lon2  Longitude do ponto 2 (graus decimais).
 * @return Distância em metros.
 */
double geo_distance_m(double lat1, double lon1, double lat2, double lon2);

/**
 * @brief Calcula o bearing (rumo inicial) do ponto 1 para o ponto 2.
 * @param lat1  Latitude do ponto 1 (graus decimais).
 * @param lon1  Longitude do ponto 1 (graus decimais).
 * @param lat2  Latitude do ponto 2 (graus decimais).
 * @param lon2  Longitude do ponto 2 (graus decimais).
 * @return Bearing em graus (0–360, 0 = Norte, 90 = Leste).
 */
double geo_bearing_deg(double lat1, double lon1, double lat2, double lon2);

/**
 * @brief Projeta coordenadas geográficas em pixels de tela usando projeção
 *        equiretangular centrada em (center_lat, center_lon).
 * @param lat        Latitude do ponto a projetar.
 * @param lon        Longitude do ponto a projetar.
 * @param center_lat Latitude do centro do mapa.
 * @param center_lon Longitude do centro do mapa.
 * @param scale_mpx  Metros por pixel da escala corrente.
 * @param screen_w   Largura da tela em pixels.
 * @param screen_h   Altura da tela em pixels.
 * @param px_out     Coordenada X em pixels (saída).
 * @param py_out     Coordenada Y em pixels (saída).
 * @return ESP_OK se o ponto está dentro da tela, ESP_ERR_NOT_FOUND se fora.
 */
esp_err_t geo_project_to_screen(double lat, double lon,
                                double center_lat, double center_lon,
                                double scale_mpx,
                                uint16_t screen_w, uint16_t screen_h,
                                int16_t *px_out, int16_t *py_out);

/**
 * @brief Formata distância de forma legível ("123 m", "1.5 km").
 * @param dist_m     Distância em metros.
 * @param buf        Buffer de saída.
 * @param buf_size   Tamanho do buffer.
 */
void geo_format_distance(double dist_m, char *buf, size_t buf_size);

/**
 * @brief Formata bearing como cardinal abreviado ("N", "NE", "E", ...).
 * @param bearing_deg Bearing em graus.
 * @return String estática com o cardinal.
 */
const char *geo_bearing_to_cardinal(double bearing_deg);

#endif /* GEO_MATH_H */

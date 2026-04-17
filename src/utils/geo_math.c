/**
 * @file geo_math.c
 * @brief Matemática geográfica: Haversine, bearing, projeção equiretangular.
 */

#include "geo_math.h"
#include "esp_log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "geo_math";

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double deg2rad(double deg) { return deg * (M_PI / 180.0); }
static inline double rad2deg(double rad) { return rad * (180.0 / M_PI); }

/* ═══════════════════════════════════════════════════════════════════════════
 * geo_distance_m — fórmula de Haversine
 * ═══════════════════════════════════════════════════════════════════════════ */
double geo_distance_m(double lat1, double lon1, double lat2, double lon2)
{
    double dlat = deg2rad(lat2 - lat1);
    double dlon = deg2rad(lon2 - lon1);
    double rlat1 = deg2rad(lat1);
    double rlat2 = deg2rad(lat2);

    double a = sin(dlat / 2.0) * sin(dlat / 2.0)
             + cos(rlat1) * cos(rlat2)
             * sin(dlon / 2.0) * sin(dlon / 2.0);

    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return GEO_EARTH_RADIUS_M * c;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * geo_bearing_deg — bearing inicial (rumo de ponto1 → ponto2)
 * ═══════════════════════════════════════════════════════════════════════════ */
double geo_bearing_deg(double lat1, double lon1, double lat2, double lon2)
{
    double rlat1 = deg2rad(lat1);
    double rlat2 = deg2rad(lat2);
    double dlon  = deg2rad(lon2 - lon1);

    double x = sin(dlon) * cos(rlat2);
    double y = cos(rlat1) * sin(rlat2) - sin(rlat1) * cos(rlat2) * cos(dlon);

    double bearing = rad2deg(atan2(x, y));
    /* Normaliza para 0–360 */
    return fmod(bearing + 360.0, 360.0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * geo_project_to_screen — projeção equiretangular centrada
 *
 * Converte (lat, lon) em pixels de tela dado:
 *   - centro do mapa (center_lat, center_lon)
 *   - escala em metros por pixel (scale_mpx)
 *   - dimensões da tela (screen_w, screen_h)
 *
 * Eixo X: leste positivo. Eixo Y: sul positivo (coordenadas de tela).
 * Retorna ESP_ERR_NOT_FOUND se o ponto cai fora dos limites da tela.
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t geo_project_to_screen(double lat, double lon,
                                double center_lat, double center_lon,
                                double scale_mpx,
                                uint16_t screen_w, uint16_t screen_h,
                                int16_t *px_out, int16_t *py_out)
{
    if (!px_out || !py_out || scale_mpx <= 0.0) return ESP_ERR_INVALID_ARG;

    /* Deslocamento em metros usando aproximação equiretangular */
    double dlat_m = deg2rad(lat - center_lat) * GEO_EARTH_RADIUS_M;
    double dlon_m = deg2rad(lon - center_lon) * GEO_EARTH_RADIUS_M
                  * cos(deg2rad(center_lat));

    /* Converte metros → pixels; Y invertido (norte = cima) */
    double px = (double)(screen_w  / 2) + (dlon_m  / scale_mpx);
    double py = (double)(screen_h / 2) - (dlat_m / scale_mpx);

    *px_out = (int16_t)px;
    *py_out = (int16_t)py;

    if (px < 0 || px >= screen_w || py < 0 || py >= screen_h) {
        return ESP_ERR_NOT_FOUND;
    }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * geo_format_distance  (já implementado, mantido)
 * ═══════════════════════════════════════════════════════════════════════════ */
void geo_format_distance(double dist_m, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return;
    if (dist_m < 1000.0) {
        snprintf(buf, buf_size, "%.0f m", dist_m);
    } else {
        snprintf(buf, buf_size, "%.1f km", dist_m / 1000.0);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * geo_bearing_to_cardinal  (já implementado, mantido)
 * ═══════════════════════════════════════════════════════════════════════════ */
const char *geo_bearing_to_cardinal(double bearing_deg)
{
    static const char *cardinals[] = {
        "N", "NE", "E", "SE", "S", "SW", "W", "NW"
    };
    int idx = (int)((bearing_deg + 22.5) / 45.0) % 8;
    return cardinals[idx];
}

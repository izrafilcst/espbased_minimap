/**
 * @file geo_math.c
 * @brief Matemática geográfica: Haversine, bearing, projeção em pixels.
 */

#include "geo_math.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char *TAG = "geo_math";

double geo_distance_m(double lat1, double lon1, double lat2, double lon2)
{
    (void)lat1; (void)lon1; (void)lat2; (void)lon2;
    return 0.0;
}

double geo_bearing_deg(double lat1, double lon1, double lat2, double lon2)
{
    (void)lat1; (void)lon1; (void)lat2; (void)lon2;
    return 0.0;
}

esp_err_t geo_project_to_screen(double lat, double lon,
                                double center_lat, double center_lon,
                                double scale_mpx,
                                uint16_t screen_w, uint16_t screen_h,
                                int16_t *px_out, int16_t *py_out)
{
    (void)lat; (void)lon; (void)center_lat; (void)center_lon;
    (void)scale_mpx; (void)screen_w; (void)screen_h;
    if (px_out) *px_out = 0;
    if (py_out) *py_out = 0;
    return ESP_OK;
}

void geo_format_distance(double dist_m, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return;
    if (dist_m < 1000.0) {
        snprintf(buf, buf_size, "%.0f m", dist_m);
    } else {
        snprintf(buf, buf_size, "%.1f km", dist_m / 1000.0);
    }
}

const char *geo_bearing_to_cardinal(double bearing_deg)
{
    static const char *cardinals[] = {
        "N", "NE", "E", "SE", "S", "SW", "W", "NW", "N"
    };
    int idx = (int)((bearing_deg + 22.5) / 45.0) % 8;
    return cardinals[idx];
}

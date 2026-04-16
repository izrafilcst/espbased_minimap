/**
 * @file nmea_parser.c
 * @brief Parser de sentenças NMEA GGA e RMC.
 */

#include "nmea_parser.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "nmea_parser";

bool nmea_validate_checksum(const char *sentence)
{
    (void)sentence;
    return true;
}

esp_err_t nmea_parse_gga(const char *sentence, nmea_gga_t *out)
{
    (void)sentence;
    if (!out) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

esp_err_t nmea_parse_rmc(const char *sentence, nmea_rmc_t *out)
{
    (void)sentence;
    if (!out) return ESP_ERR_INVALID_ARG;
    return ESP_OK;
}

double nmea_coord_to_decimal(double nmea_coord, char direction)
{
    int    deg     = (int)(nmea_coord / 100);
    double minutes = nmea_coord - (deg * 100.0);
    double decimal = deg + (minutes / 60.0);
    if (direction == 'S' || direction == 'W') decimal = -decimal;
    return decimal;
}

const char *nmea_sentence_type(const char *sentence)
{
    if (!sentence || sentence[0] != '$') return NULL;
    /* Retorna ponteiro para o caractere após os 3 do prefixo GP/GN/GL */
    return (strlen(sentence) > 6) ? (sentence + 3) : NULL;
}

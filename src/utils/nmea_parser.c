/**
 * @file nmea_parser.c
 * @brief Parser de sentenças NMEA 0183 — GGA e RMC.
 *        Sem alocação dinâmica; opera sobre buffers estáticos.
 */

#include "nmea_parser.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

static const char *TAG = "nmea_parser";

/* ═══════════════════════════════════════════════════════════════════════════
 * Helpers internos
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Extrai o i-ésimo campo (0-based) de uma sentença NMEA separada por vírgulas.
 * Escreve em buf (máx buf_size bytes, null-terminated).
 * Retorna o ponteiro buf ou NULL se o campo não existir.
 */
static char *nmea_field(const char *sentence, int index, char *buf, size_t buf_size)
{
    if (!sentence || !buf || buf_size == 0) return NULL;
    buf[0] = '\0';

    int  field = 0;
    const char *p = sentence;

    while (*p && field < index) {
        if (*p == ',') field++;
        p++;
    }
    if (field < index) return NULL;   /* campo inexistente */

    size_t i = 0;
    while (*p && *p != ',' && *p != '*' && *p != '\r' && *p != '\n') {
        if (i < buf_size - 1) buf[i++] = *p;
        p++;
    }
    buf[i] = '\0';
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * nmea_validate_checksum
 * ═══════════════════════════════════════════════════════════════════════════ */
bool nmea_validate_checksum(const char *sentence)
{
    if (!sentence || sentence[0] != '$') return false;

    /* Localiza o '*' que precede os dois hex do checksum */
    const char *star = strchr(sentence, '*');
    if (!star || strlen(star) < 3) return false;

    /* Calcula XOR de tudo entre '$' (exclusive) e '*' (exclusive) */
    uint8_t calc = 0;
    for (const char *p = sentence + 1; p < star; p++) {
        calc ^= (uint8_t)*p;
    }

    /* Lê o checksum declarado */
    unsigned int declared = 0;
    if (sscanf(star + 1, "%2X", &declared) != 1) return false;

    return calc == (uint8_t)declared;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * nmea_coord_to_decimal  (já implementado, mantido idêntico)
 * ═══════════════════════════════════════════════════════════════════════════ */
double nmea_coord_to_decimal(double nmea_coord, char direction)
{
    int    deg     = (int)(nmea_coord / 100);
    double minutes = nmea_coord - (deg * 100.0);
    double decimal = deg + (minutes / 60.0);
    if (direction == 'S' || direction == 'W') decimal = -decimal;
    return decimal;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * nmea_sentence_type
 * ═══════════════════════════════════════════════════════════════════════════ */
const char *nmea_sentence_type(const char *sentence)
{
    if (!sentence || sentence[0] != '$') return NULL;
    /* Pula os dois caracteres de talker (GP, GN, GL…) */
    return (strlen(sentence) > 6) ? (sentence + 3) : NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * nmea_parse_gga
 *
 * Formato: $--GGA,hhmmss.ss,llll.ll,a,yyyyy.yy,a,x,xx,x.x,x.x,M,x.x,M,x.x,xxxx*hh
 * Campo:   0      1          2       3  4        5  6  7   8   9  10  11 12   13  14
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t nmea_parse_gga(const char *sentence, nmea_gga_t *out)
{
    if (!sentence || !out) return ESP_ERR_INVALID_ARG;

    /* Verifica prefixo GGA (aceita GP, GN, GL…) */
    const char *type = nmea_sentence_type(sentence);
    if (!type || strncmp(type, "GGA", 3) != 0) return ESP_ERR_INVALID_ARG;

    if (!nmea_validate_checksum(sentence)) {
        ESP_LOGW(TAG, "GGA checksum inválido: %.80s", sentence);
        return ESP_ERR_INVALID_RESPONSE;
    }

    char buf[32];
    memset(out, 0, sizeof(*out));

    /* Campo 1 — hora UTC: hhmmss.ss */
    if (nmea_field(sentence, 1, buf, sizeof(buf)) && buf[0]) {
        unsigned h = 0, m = 0, s = 0;
        sscanf(buf, "%2u%2u%2u", &h, &m, &s);
        out->hour   = (uint8_t)h;
        out->minute = (uint8_t)m;
        out->second = (uint8_t)s;
    }

    /* Campo 2 — latitude DDDMM.MMMM, Campo 3 — N/S */
    char lat_dir[4] = "N";
    if (nmea_field(sentence, 2, buf, sizeof(buf)) && buf[0]) {
        double raw = atof(buf);
        nmea_field(sentence, 3, lat_dir, sizeof(lat_dir));
        out->latitude = nmea_coord_to_decimal(raw, lat_dir[0]);
    }

    /* Campo 4 — longitude DDDMM.MMMM, Campo 5 — E/W */
    char lon_dir[4] = "E";
    if (nmea_field(sentence, 4, buf, sizeof(buf)) && buf[0]) {
        double raw = atof(buf);
        nmea_field(sentence, 5, lon_dir, sizeof(lon_dir));
        out->longitude = nmea_coord_to_decimal(raw, lon_dir[0]);
    }

    /* Campo 6 — fix quality */
    if (nmea_field(sentence, 6, buf, sizeof(buf)) && buf[0])
        out->fix_quality = (uint8_t)atoi(buf);

    /* Campo 7 — nº satélites */
    if (nmea_field(sentence, 7, buf, sizeof(buf)) && buf[0])
        out->satellites = (uint8_t)atoi(buf);

    /* Campo 8 — HDOP */
    if (nmea_field(sentence, 8, buf, sizeof(buf)) && buf[0])
        out->hdop = (float)atof(buf);

    /* Campo 9 — altitude MSL (metros) */
    if (nmea_field(sentence, 9, buf, sizeof(buf)) && buf[0])
        out->altitude_m = (float)atof(buf);

    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * nmea_parse_rmc
 *
 * Formato: $--RMC,hhmmss.ss,A,llll.ll,a,yyyyy.yy,a,x.x,x.x,ddmmyy,x.x,a*hh
 * Campo:   0      1          2  3      4  5        6  7   8   9      10  11
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t nmea_parse_rmc(const char *sentence, nmea_rmc_t *out)
{
    if (!sentence || !out) return ESP_ERR_INVALID_ARG;

    const char *type = nmea_sentence_type(sentence);
    if (!type || strncmp(type, "RMC", 3) != 0) return ESP_ERR_INVALID_ARG;

    if (!nmea_validate_checksum(sentence)) {
        ESP_LOGW(TAG, "RMC checksum inválido: %.80s", sentence);
        return ESP_ERR_INVALID_RESPONSE;
    }

    char buf[32];
    memset(out, 0, sizeof(*out));

    /* Campo 1 — hora UTC */
    if (nmea_field(sentence, 1, buf, sizeof(buf)) && buf[0]) {
        unsigned h = 0, m = 0, s = 0;
        sscanf(buf, "%2u%2u%2u", &h, &m, &s);
        out->hour   = (uint8_t)h;
        out->minute = (uint8_t)m;
        out->second = (uint8_t)s;
    }

    /* Campo 2 — status: A=válido, V=inválido */
    if (nmea_field(sentence, 2, buf, sizeof(buf)) && buf[0])
        out->valid = (buf[0] == 'A');

    /* Campo 3 — latitude, Campo 4 — N/S */
    char lat_dir[4] = "N";
    if (nmea_field(sentence, 3, buf, sizeof(buf)) && buf[0]) {
        double raw = atof(buf);
        nmea_field(sentence, 4, lat_dir, sizeof(lat_dir));
        out->latitude = nmea_coord_to_decimal(raw, lat_dir[0]);
    }

    /* Campo 5 — longitude, Campo 6 — E/W */
    char lon_dir[4] = "E";
    if (nmea_field(sentence, 5, buf, sizeof(buf)) && buf[0]) {
        double raw = atof(buf);
        nmea_field(sentence, 6, lon_dir, sizeof(lon_dir));
        out->longitude = nmea_coord_to_decimal(raw, lon_dir[0]);
    }

    /* Campo 7 — velocidade em nós → km/h (1 nó = 1.852 km/h) */
    if (nmea_field(sentence, 7, buf, sizeof(buf)) && buf[0])
        out->speed_kmh = (float)(atof(buf) * 1.852);

    /* Campo 8 — rumo verdadeiro */
    if (nmea_field(sentence, 8, buf, sizeof(buf)) && buf[0])
        out->course_deg = (float)atof(buf);

    /* Campo 9 — data: ddmmyy */
    if (nmea_field(sentence, 9, buf, sizeof(buf)) && buf[0]) {
        unsigned d = 0, mo = 0, y = 0;
        sscanf(buf, "%2u%2u%2u", &d, &mo, &y);
        out->day   = (uint8_t)d;
        out->month = (uint8_t)mo;
        out->year  = (uint16_t)(y + 2000);
    }

    return ESP_OK;
}

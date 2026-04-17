/**
 * @file gps_neo6m.c
 * @brief Driver GPS NEO-6M — UART1 + parser NMEA + task Core 0.
 *
 * Fluxo:
 *   gps_init()
 *     → instala driver UART1 (9600 baud, 8N1)
 *     → cria mutex de proteção de s_gps_data
 *     → cria gps_rx_task (Core 0, prio TASK_GPS_PRIO)
 *
 *   gps_rx_task  (loop infinito)
 *     → lê bytes do UART até '\n'
 *     → valida checksum NMEA
 *     → nmea_parse_gga / nmea_parse_rmc
 *     → atualiza s_gps_data sob mutex
 */

#include "gps_neo6m.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "utils/nmea_parser.h"
#include <string.h>

static const char *TAG = "gps_neo6m";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static gps_data_t        s_gps_data  = {0};
static SemaphoreHandle_t s_mutex     = NULL;
static TaskHandle_t      s_task_hdl  = NULL;
static bool              s_running   = false;

/* ═══════════════════════════════════════════════════════════════════════════
 * gps_init
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t gps_init(void)
{
    ESP_LOGI(TAG, "=== gps_init START ===");

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) {
        ESP_LOGE(TAG, "Falha ao criar mutex");
        return ESP_ERR_NO_MEM;
    }

    /* UART1 — 9600 8N1 */
    const uart_config_t uart_cfg = {
        .baud_rate  = GPS_UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_PORT,
                                 GPS_PIN_TX, GPS_PIN_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_PORT,
                                        GPS_RX_BUF_SIZE, 0,
                                        0, NULL, 0));

    /* PPS como entrada simples (sem interrupção por ora) */
    gpio_reset_pin(GPS_PIN_PPS);
    gpio_set_direction(GPS_PIN_PPS, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPS_PIN_PPS, GPIO_PULLDOWN_ONLY);

    s_running = true;
    BaseType_t rc = xTaskCreatePinnedToCore(
        gps_rx_task, "gps_rx",
        TASK_GPS_STACK, NULL,
        TASK_GPS_PRIO, &s_task_hdl,
        TASK_GPS_CORE);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar gps_rx_task");
        uart_driver_delete(GPS_UART_PORT);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "=== gps_init OK (UART%d @ %d baud) ===",
             GPS_UART_PORT, GPS_UART_BAUD);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * gps_deinit
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t gps_deinit(void)
{
    s_running = false;
    if (s_task_hdl) {
        vTaskDelete(s_task_hdl);
        s_task_hdl = NULL;
    }
    uart_driver_delete(GPS_UART_PORT);
    if (s_mutex) {
        vSemaphoreDelete(s_mutex);
        s_mutex = NULL;
    }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * gps_get_data — cópia thread-safe
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t gps_get_data(gps_data_t *out)
{
    if (!out)     return ESP_ERR_INVALID_ARG;
    if (!s_mutex) return ESP_ERR_INVALID_STATE;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memcpy(out, &s_gps_data, sizeof(gps_data_t));
    xSemaphoreGive(s_mutex);

    return s_gps_data.valid ? ESP_OK : ESP_ERR_NOT_FOUND;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * gps_has_fix
 * ═══════════════════════════════════════════════════════════════════════════ */
bool gps_has_fix(void)
{
    /* Leitura sem mutex: bool é atômica em Xtensa LX7 */
    return s_gps_data.valid && (s_gps_data.fix_quality > 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * gps_set_baud_115200
 *
 * Envia mensagem UBX CFG-PRT para reconfigurar a UART do NEO-6M para 115200.
 * Após envio, reinicia o driver UART na nova velocidade.
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t gps_set_baud_115200(void)
{
    ESP_LOGI(TAG, "Reconfigurando NEO-6M para 115200 baud");

    /* UBX CFG-PRT: porta 1 (UART1), baud 115200, NMEA I/O, sem flow control */
    static const uint8_t ubx_cfg_prt[] = {
        0xB5, 0x62,        /* sync chars */
        0x06, 0x00,        /* class: CFG, id: PRT */
        0x14, 0x00,        /* length: 20 bytes */
        0x01,              /* portID: 1 (UART) */
        0x00,              /* reserved */
        0x00, 0x00,        /* txReady */
        0xD0, 0x08, 0x00, 0x00,  /* mode: 8N1 */
        0x00, 0xC2, 0x01, 0x00,  /* baudRate: 115200 */
        0x07, 0x00,        /* inProtoMask: UBX+NMEA+RTCM */
        0x03, 0x00,        /* outProtoMask: UBX+NMEA */
        0x00, 0x00,        /* flags */
        0x00, 0x00,        /* reserved */
        0xBC, 0x5E         /* checksum CK_A, CK_B */
    };

    uart_write_bytes(GPS_UART_PORT, ubx_cfg_prt, sizeof(ubx_cfg_prt));
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Reinicia driver UART na nova velocidade */
    uart_set_baudrate(GPS_UART_PORT, GPS_UART_BAUD_HIGH);
    ESP_LOGI(TAG, "UART1 reconfigurado para %d baud", GPS_UART_BAUD_HIGH);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * gps_rx_task
 *
 * Lê sentenças NMEA linha por linha do buffer UART.
 * Cada sentença termina em '\n' (precedida de '\r').
 * Despacha GGA e RMC para os parsers correspondentes.
 * ═══════════════════════════════════════════════════════════════════════════ */
void gps_rx_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "gps_rx_task iniciada (Core %d)", xPortGetCoreID());

    static char line[GPS_NMEA_MAX_LEN + 1];
    int line_pos = 0;
    uint8_t byte;

    while (s_running) {
        int n = uart_read_bytes(GPS_UART_PORT, &byte, 1, pdMS_TO_TICKS(20));
        if (n <= 0) continue;

        if (byte == '$') {
            /* Início de nova sentença — reseta buffer */
            line_pos = 0;
            line[0] = '$';
            line_pos = 1;
        } else if (byte == '\n' && line_pos > 0) {
            /* Fim de sentença */
            line[line_pos] = '\0';

            const char *type = nmea_sentence_type(line);
            if (!type) { line_pos = 0; continue; }

            if (strncmp(type, "GGA", 3) == 0) {
                nmea_gga_t gga;
                if (nmea_parse_gga(line, &gga) == ESP_OK) {
                    xSemaphoreTake(s_mutex, portMAX_DELAY);
                    s_gps_data.latitude    = gga.latitude;
                    s_gps_data.longitude   = gga.longitude;
                    s_gps_data.altitude_m  = gga.altitude_m;
                    s_gps_data.satellites  = gga.satellites;
                    s_gps_data.fix_quality = gga.fix_quality;
                    s_gps_data.valid       = (gga.fix_quality > 0);
                    s_gps_data.timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000);
                    xSemaphoreGive(s_mutex);
                }
            } else if (strncmp(type, "RMC", 3) == 0) {
                nmea_rmc_t rmc;
                if (nmea_parse_rmc(line, &rmc) == ESP_OK && rmc.valid) {
                    xSemaphoreTake(s_mutex, portMAX_DELAY);
                    s_gps_data.speed_kmh  = rmc.speed_kmh;
                    s_gps_data.course_deg = rmc.course_deg;
                    /* valid já gerenciado pelo GGA */
                    xSemaphoreGive(s_mutex);
                }
            }

            line_pos = 0;
        } else if (byte != '\r' && line_pos > 0) {
            if (line_pos < GPS_NMEA_MAX_LEN) {
                line[line_pos++] = (char)byte;
            } else {
                /* Sentença muito longa — descarta */
                ESP_LOGW(TAG, "Sentença NMEA muito longa, descartando");
                line_pos = 0;
            }
        }
    }

    ESP_LOGI(TAG, "gps_rx_task encerrando");
    vTaskDelete(NULL);
}

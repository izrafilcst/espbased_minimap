/**
 * @file lora_sx1278.cpp
 * @brief Implementação do driver SX1278 usando RadioLib (C++ wrapper).
 *
 * Arquitetura de tasks:
 *   lora_rx_task  (Core 0) — aguarda semáforo sinalizado pela ISR do DIO0,
 *                             lê pacote via radio.readData() e chama s_rx_cb.
 *   lora_tx_task  (Core 0) — aguarda fila s_tx_queue, chama radio.transmit(),
 *                             chama tx_done_cb ao terminar.
 *
 * Todos os objetos RadioLib são estáticos (alocados em DRAM interna).
 */

extern "C" {
#include "lora_sx1278.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>
}

#include <RadioLib.h>
#include "EspS3Hal.h"

static const char *TAG = "lora_sx1278";

/* ═══════════════════════════════════════════════════════════════════════════
 * Objetos RadioLib (static — vivem pelo tempo de execução do firmware)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* EspS3Hal: wraps ESP-IDF spi_master para RadioLib no ESP32-S3.
 * CS é gerenciado manualmente pelo HAL — Module recebe RADIOLIB_NC. */
static EspS3Hal s_hal(LORA_PIN_SCK, LORA_PIN_MISO, LORA_PIN_MOSI,
                      LORA_PIN_CS, SPI3_HOST, LORA_SPI_FREQ_HZ);

static Module s_module(
    &s_hal,
    RADIOLIB_NC,    /* NSS — gerenciado pelo EspS3Hal */
    LORA_PIN_DIO0,  /* IRQ  (TxDone / RxDone) */
    LORA_PIN_RST,   /* RST  */
    LORA_PIN_DIO1   /* busy / RxTimeout */
);

static SX1278 s_radio(&s_module);

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static lora_rx_cb_t          s_rx_cb        = NULL;
static volatile bool         s_tx_busy      = false;
static SemaphoreHandle_t     s_rx_sem       = NULL;   /* ISR → rx_task */
static QueueHandle_t         s_tx_queue     = NULL;   /* lora_send → tx_task */
static TaskHandle_t          s_rx_task_hdl  = NULL;
static TaskHandle_t          s_tx_task_hdl  = NULL;
static bool                  s_initialized  = false;

/* Estrutura enviada pela fila de TX */
typedef struct {
    uint8_t           data[MESH_MAX_PKT_LEN];
    size_t            len;
    lora_tx_done_cb_t done_cb;
} tx_item_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * ISR do DIO0 — executada em borda de subida (TxDone / RxDone)
 * Apenas sinaliza o semáforo; não chama RadioLib.
 * ═══════════════════════════════════════════════════════════════════════════ */
static void IRAM_ATTR lora_dio0_isr(void)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_rx_sem, &woken);
    if (woken) portYIELD_FROM_ISR();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * lora_rx_task — aguarda DIO0 e processa pacote recebido
 * ═══════════════════════════════════════════════════════════════════════════ */
static void lora_rx_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "lora_rx_task iniciada (Core %d)", xPortGetCoreID());

    static uint8_t buf[MESH_MAX_PKT_LEN];

    while (1) {
        /* Aguarda sinalização do DIO0 ISR */
        if (xSemaphoreTake(s_rx_sem, portMAX_DELAY) != pdTRUE) continue;

        if (s_tx_busy) {
            /* DIO0 foi TxDone — retorna ao RX contínuo */
            s_tx_busy = false;
            s_radio.startReceive();
            continue;
        }

        /* DIO0 foi RxDone — lê pacote */
        int pkt_len = s_radio.getPacketLength();
        if (pkt_len <= 0 || pkt_len > (int)MESH_MAX_PKT_LEN) {
            ESP_LOGW(TAG, "Pacote com tamanho inválido: %d", pkt_len);
            s_radio.startReceive();
            continue;
        }

        int state = s_radio.readData(buf, (size_t)pkt_len);
        if (state != RADIOLIB_ERR_NONE) {
            ESP_LOGW(TAG, "readData erro: %d", state);
            s_radio.startReceive();
            continue;
        }

        int8_t rssi = (int8_t)s_radio.getRSSI();
        int8_t snr  = (int8_t)s_radio.getSNR();

        if (s_rx_cb) {
            s_rx_cb(buf, (size_t)pkt_len, rssi, snr);
        }

        /* Retorna a RX contínuo */
        s_radio.startReceive();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * lora_tx_task — despacha itens da fila de transmissão
 * ═══════════════════════════════════════════════════════════════════════════ */
static void lora_tx_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "lora_tx_task iniciada (Core %d)", xPortGetCoreID());

    tx_item_t item;

    while (1) {
        if (xQueueReceive(s_tx_queue, &item, portMAX_DELAY) != pdTRUE) continue;

        s_tx_busy = true;

        /* Para RX antes de transmitir */
        s_radio.standby();

        int state = s_radio.transmit(item.data, item.len);
        if (state != RADIOLIB_ERR_NONE) {
            ESP_LOGW(TAG, "transmit erro: %d", state);
        } else {
            ESP_LOGD(TAG, "TX OK (%zu bytes)", item.len);
        }

        s_tx_busy = false;

        if (item.done_cb) item.done_cb();

        /* Retorna a RX */
        s_radio.startReceive();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * lora_init
 * ═══════════════════════════════════════════════════════════════════════════ */
extern "C" esp_err_t lora_init(void)
{
    ESP_LOGI(TAG, "=== lora_init START ===");

    /* GPIO ISR service — pode já ter sido instalado por outro módulo */
    esp_err_t isr_ret = gpio_install_isr_service(0);
    if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service: %s", esp_err_to_name(isr_ret));
        return isr_ret;
    }

    /* Semáforo binário: ISR sinaliza, rx_task consome */
    s_rx_sem = xSemaphoreCreateBinary();
    if (!s_rx_sem) return ESP_ERR_NO_MEM;

    /* Fila de TX: até 4 pacotes pendentes */
    s_tx_queue = xQueueCreate(4, sizeof(tx_item_t));
    if (!s_tx_queue) return ESP_ERR_NO_MEM;

    /* Inicializa RadioLib */
    ESP_LOGI(TAG, "SX1278: CS=%d DIO0=%d RST=%d SCK=%d MOSI=%d MISO=%d",
             LORA_PIN_CS, LORA_PIN_DIO0, LORA_PIN_RST,
             LORA_PIN_SCK, LORA_PIN_MOSI, LORA_PIN_MISO);

    int state = s_radio.begin(
        (float)LORA_FREQUENCY_HZ / 1e6f,   /* freq MHz */
        (float)LORA_BANDWIDTH_HZ  / 1e3f,  /* BW kHz  */
        LORA_SPREADING_FACTOR,
        LORA_CODING_RATE,
        LORA_SYNC_WORD,
        LORA_TX_POWER_DBM,
        LORA_PREAMBLE_LEN
    );

    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "radio.begin() falhou: %d", state);
        return ESP_FAIL;
    }

    /* CRC de hardware habilitado */
    s_radio.setCRC(LORA_CRC_ENABLE ? 1 : 0);

    /* DIO0: modo interrupção — RadioLib usa gpio_isr_handler_add internamente */
    s_radio.setDio0Action(lora_dio0_isr, RISING);

    /* Coloca em RX contínuo imediatamente */
    s_radio.startReceive();

    /* Tasks */
    BaseType_t rc;
    rc = xTaskCreatePinnedToCore(lora_rx_task, "lora_rx",
                                 TASK_LORA_RX_STACK, NULL,
                                 TASK_LORA_RX_PRIO, &s_rx_task_hdl,
                                 TASK_LORA_RX_CORE);
    if (rc != pdPASS) { ESP_LOGE(TAG, "Falha lora_rx_task"); return ESP_FAIL; }

    rc = xTaskCreatePinnedToCore(lora_tx_task, "lora_tx",
                                 TASK_LORA_TX_STACK, NULL,
                                 TASK_LORA_TX_PRIO, &s_tx_task_hdl,
                                 TASK_LORA_TX_CORE);
    if (rc != pdPASS) { ESP_LOGE(TAG, "Falha lora_tx_task"); return ESP_FAIL; }

    s_initialized = true;
    ESP_LOGI(TAG, "=== lora_init OK (%.3f MHz SF%d BW%.0fkHz) ===",
             (float)LORA_FREQUENCY_HZ / 1e6f,
             LORA_SPREADING_FACTOR,
             (float)LORA_BANDWIDTH_HZ / 1e3f);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * lora_deinit
 * ═══════════════════════════════════════════════════════════════════════════ */
extern "C" esp_err_t lora_deinit(void)
{
    s_radio.sleep();
    s_initialized = false;

    if (s_rx_task_hdl) { vTaskDelete(s_rx_task_hdl); s_rx_task_hdl = NULL; }
    if (s_tx_task_hdl) { vTaskDelete(s_tx_task_hdl); s_tx_task_hdl = NULL; }
    if (s_rx_sem)   { vSemaphoreDelete(s_rx_sem);  s_rx_sem   = NULL; }
    if (s_tx_queue) { vQueueDelete(s_tx_queue);    s_tx_queue = NULL; }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * lora_send
 * ═══════════════════════════════════════════════════════════════════════════ */
extern "C" esp_err_t lora_send(const uint8_t *data, size_t len,
                               lora_tx_done_cb_t tx_done_cb)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    if (len == 0 || len > MESH_MAX_PKT_LEN) return ESP_ERR_INVALID_SIZE;

    tx_item_t item;
    memcpy(item.data, data, len);
    item.len     = len;
    item.done_cb = tx_done_cb;

    if (xQueueSend(s_tx_queue, &item, MS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGW(TAG, "TX queue cheia");
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * lora_start_rx / lora_stop_rx
 * ═══════════════════════════════════════════════════════════════════════════ */
extern "C" esp_err_t lora_start_rx(lora_rx_cb_t rx_cb)
{
    s_rx_cb = rx_cb;
    if (s_initialized) s_radio.startReceive();
    return ESP_OK;
}

extern "C" esp_err_t lora_stop_rx(void)
{
    s_rx_cb = NULL;
    if (s_initialized) s_radio.standby();
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * lora_read_rssi
 * ═══════════════════════════════════════════════════════════════════════════ */
extern "C" esp_err_t lora_read_rssi(int8_t *rssi_out)
{
    if (!rssi_out) return ESP_ERR_INVALID_ARG;
    if (!s_initialized) { *rssi_out = -127; return ESP_ERR_INVALID_STATE; }
    *rssi_out = (int8_t)s_radio.getRSSI();
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * lora_is_transmitting / lora_reset
 * ═══════════════════════════════════════════════════════════════════════════ */
extern "C" bool lora_is_transmitting(void)
{
    return s_tx_busy;
}

extern "C" esp_err_t lora_reset(void)
{
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    s_radio.reset();
    return ESP_OK;
}

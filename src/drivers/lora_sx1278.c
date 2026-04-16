/**
 * @file lora_sx1278.c
 * @brief Implementação do driver SX1278 RA-02 via SPI3.
 */

#include "lora_sx1278.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "lora_sx1278";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static spi_device_handle_t s_spi_dev   = NULL;
static lora_rx_cb_t        s_rx_cb     = NULL;
static volatile bool       s_tx_busy   = false;

/* ─── Implementações (stubs) ────────────────────────────────────────────────*/

esp_err_t lora_init(void)
{
    ESP_LOGI(TAG, "lora_init: stub");
    return ESP_OK;
}

esp_err_t lora_deinit(void)
{
    ESP_LOGI(TAG, "lora_deinit: stub");
    return ESP_OK;
}

esp_err_t lora_send(const uint8_t *data, size_t len, lora_tx_done_cb_t tx_done_cb)
{
    (void)data; (void)len; (void)tx_done_cb;
    return ESP_OK;
}

esp_err_t lora_start_rx(lora_rx_cb_t rx_cb)
{
    s_rx_cb = rx_cb;
    return ESP_OK;
}

esp_err_t lora_stop_rx(void)
{
    s_rx_cb = NULL;
    return ESP_OK;
}

esp_err_t lora_read_rssi(int8_t *rssi_out)
{
    if (rssi_out) *rssi_out = -100;
    return ESP_OK;
}

bool lora_is_transmitting(void)
{
    return s_tx_busy;
}

esp_err_t lora_reset(void)
{
    ESP_LOGI(TAG, "lora_reset: stub");
    return ESP_OK;
}

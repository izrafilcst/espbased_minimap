/**
 * @file display_ili9341.c
 * @brief Implementação do driver ILI9341 via SPI2 com DMA.
 */

#include "display_ili9341.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "display_ili9341";

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static spi_device_handle_t s_spi_dev = NULL;

/* ─── Implementações (stubs) ────────────────────────────────────────────────*/

esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "display_init: stub");
    return ESP_OK;
}

esp_err_t display_deinit(void)
{
    ESP_LOGI(TAG, "display_deinit: stub");
    return ESP_OK;
}

esp_err_t display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    (void)x0; (void)y0; (void)x1; (void)y1;
    return ESP_OK;
}

esp_err_t display_send_pixels(const uint16_t *data, size_t len)
{
    (void)data; (void)len;
    return ESP_OK;
}

esp_err_t display_fill_rect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color)
{
    (void)x0; (void)y0; (void)w; (void)h; (void)color;
    return ESP_OK;
}

void display_set_backlight(uint8_t brightness)
{
    (void)brightness;
}

void display_lvgl_flush_cb(void *display, const void *area, uint8_t *px_map)
{
    (void)display; (void)area; (void)px_map;
}

#ifndef DISPLAY_ILI9341_H
#define DISPLAY_ILI9341_H

/**
 * @file display_ili9341.h
 * @brief Driver para display ILI9341 320×240 via SPI2 (FSPI).
 *        Usa DMA automático via esp_lcd panel API.
 *        Backlight controlado via LEDC PWM no GPIO8.
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "meshtracker_config.h"

/* ─── Constantes públicas ────────────────────────────────────────────────── */
#define ILI9341_CMD_SWRESET     0x01  /**< Software Reset                   */
#define ILI9341_CMD_SLPOUT      0x11  /**< Sleep Out (aguardar 120ms)        */
#define ILI9341_CMD_DISPON      0x29  /**< Display On                        */
#define ILI9341_CMD_CASET       0x2A  /**< Column Address Set                */
#define ILI9341_CMD_PASET       0x2B  /**< Page (row) Address Set            */
#define ILI9341_CMD_RAMWR       0x2C  /**< Memory Write                      */
#define ILI9341_CMD_MADCTL      0x36  /**< Memory Access Control             */
#define ILI9341_CMD_COLMOD      0x3A  /**< Pixel Format Set                  */

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Inicializa SPI2 bus, panel IO e painel ILI9341.
 *        Configura landscape (swap_xy), RGB565, 40MHz.
 *        Backlight LEDC canal 0, 5kHz, fade 0→100% em 500ms.
 * @return ESP_OK em sucesso, código de erro em falha.
 */
esp_err_t display_init(void);

/**
 * @brief Desinicializa o driver, libera SPI bus e desativa backlight.
 * @return ESP_OK em sucesso.
 */
esp_err_t display_deinit(void);

/**
 * @brief Define a janela de escrita (column/row address) no ILI9341.
 *        Obsoleta com a panel API — mantida por compatibilidade.
 */
esp_err_t display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Envia bloco de pixels RGB565 via DMA para a janela corrente.
 * @param data  Buffer de pixels RGB565.
 * @param len   Número de bytes a enviar.
 */
esp_err_t display_send_pixels(const uint16_t *data, size_t len);

/**
 * @brief Preenche um retângulo inteiro com uma cor sólida RGB565.
 * @param x0    Coluna inicial.
 * @param y0    Linha inicial.
 * @param w     Largura em pixels.
 * @param h     Altura em pixels.
 * @param color Cor RGB565.
 */
esp_err_t display_fill_rect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief Define a intensidade do backlight via LEDC PWM.
 * @param percent Nível de 0 (apagado) a 100 (máximo).
 */
void display_set_backlight(uint8_t percent);

/**
 * @brief Callback flush para o LVGL 9.x (lv_display_set_flush_cb).
 *        Envia a área suja para o display via esp_lcd_panel_draw_bitmap
 *        e chama lv_display_flush_ready() ao terminar.
 * @param display  lv_display_t* (passado como void* para evitar dep. em lvgl.h).
 * @param area     lv_area_t* com coordenadas da região suja.
 * @param px_map   Buffer de pixels RGB565.
 */
void display_lvgl_flush_cb(void *display, const void *area, uint8_t *px_map);

/**
 * @brief Retorna o handle do painel esp_lcd para uso externo (ex.: LVGL direto).
 * @return esp_lcd_panel_handle_t ou NULL se não inicializado.
 */
esp_lcd_panel_handle_t display_get_panel_handle(void);

#endif /* DISPLAY_ILI9341_H */

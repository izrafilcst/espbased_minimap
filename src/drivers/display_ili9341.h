#ifndef DISPLAY_ILI9341_H
#define DISPLAY_ILI9341_H

/**
 * @file display_ili9341.h
 * @brief Driver para display ILI9341 320×240 via SPI2 (FSPI).
 *        Usa DMA para transferências de framebuffer.
 *        Backlight controlado via LEDC PWM no GPIO8.
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/spi_master.h"
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

/* ─── Typedef handle ─────────────────────────────────────────────────────── */
typedef struct display_ili9341_s *display_handle_t;

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Inicializa o barramento SPI2, registra o device ILI9341 e
 *        executa a sequência de inicialização do display (reset, sleep-out,
 *        configuração de cor RGB565, display on, backlight).
 * @return ESP_OK em sucesso, código de erro em falha.
 */
esp_err_t display_init(void);

/**
 * @brief Desinicializa o driver, libera o device SPI e desativa backlight.
 * @return ESP_OK em sucesso.
 */
esp_err_t display_deinit(void);

/**
 * @brief Define a janela de escrita (column/row address) no ILI9341.
 * @param x0 Coluna inicial (0..319).
 * @param y0 Linha inicial (0..239).
 * @param x1 Coluna final (inclusiva).
 * @param y1 Linha final (inclusiva).
 * @return ESP_OK em sucesso.
 */
esp_err_t display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Envia bloco de pixels RGB565 via DMA para a janela corrente.
 *        Os dados devem estar em formato big-endian (byte-swap do host).
 * @param data   Ponteiro para buffer de pixels RGB565.
 * @param len    Número de bytes a enviar.
 * @return ESP_OK em sucesso.
 */
esp_err_t display_send_pixels(const uint16_t *data, size_t len);

/**
 * @brief Preenche um retângulo inteiro com uma cor sólida RGB565.
 * @param x0    Coluna inicial.
 * @param y0    Linha inicial.
 * @param w     Largura em pixels.
 * @param h     Altura em pixels.
 * @param color Cor RGB565.
 * @return ESP_OK em sucesso.
 */
esp_err_t display_fill_rect(uint16_t x0, uint16_t y0, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief Define a intensidade do backlight via PWM (LEDC).
 * @param brightness Nível de 0 (apagado) a 255 (máximo).
 */
void display_set_backlight(uint8_t brightness);

/**
 * @brief Callback flush para o LVGL (lv_display_set_flush_cb).
 *        Envia a área suja para o display e chama lv_display_flush_ready.
 * @param display Ponteiro para o display LVGL.
 * @param area    Área a ser atualizada.
 * @param px_map  Buffer de pixels.
 */
void display_lvgl_flush_cb(void *display, const void *area, uint8_t *px_map);

#endif /* DISPLAY_ILI9341_H */

/**
 * @file display_ili9341.c
 * @brief Driver ILI9341 320×240 via SPI2 (FSPI) com DMA automático.
 *
 * Sequência de inicialização:
 *   1. spi_bus_initialize(SPI2_HOST, DMA_AUTO)
 *   2. esp_lcd_new_panel_io_spi()          — panel IO sobre o bus SPI
 *   3. Hardware reset manual (RST LOW 20µs → HIGH → 5ms)
 *   4. esp_lcd_new_panel_ili9341()         — instancia driver do painel
 *   5. esp_lcd_panel_reset() + init()      — SWRESET + sequência de comandos
 *   6. swap_xy(true) + mirror(true,false)  — landscape 320×240
 *   7. disp_on_off(true)                   — Display ON
 *   8. LEDC PWM backlight, fade 0→100% em 500ms
 */

#include "display_ili9341.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_dev.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "display";

/* ─── Estado interno ─────────────────────────────────────────────────────── */
static esp_lcd_panel_io_handle_t s_io_handle  = NULL;
static esp_lcd_panel_handle_t    s_panel      = NULL;
static bool                      s_initialized = false;

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: LEDC backlight
 * ═══════════════════════════════════════════════════════════════════════════ */
static esp_err_t backlight_init(void)
{
    ESP_LOGI(TAG, "LEDC: GPIO%d, canal %d, %uHz, %u-bit",
             DISPLAY_PIN_BL, DISPLAY_BL_LEDC_CH,
             DISPLAY_BL_LEDC_FREQ, DISPLAY_BL_LEDC_RES);

    ledc_timer_config_t tcfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = DISPLAY_BL_LEDC_TIMER,
        .duty_resolution = DISPLAY_BL_LEDC_RES,
        .freq_hz         = DISPLAY_BL_LEDC_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&tcfg), TAG, "timer LEDC falhou");

    ledc_channel_config_t ccfg = {
        .gpio_num   = DISPLAY_PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = DISPLAY_BL_LEDC_CH,
        .timer_sel  = DISPLAY_BL_LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .duty       = 0,
        .hpoint     = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags      = { .output_invert = 0 },
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ccfg), TAG, "canal LEDC falhou");

    /* Instala serviço de fade; ESP_ERR_INVALID_STATE = já instalado, OK */
    esp_err_t ret = ledc_fade_func_install(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_RETURN_ON_ERROR(ret, TAG, "ledc_fade_func_install falhou");
    }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * display_init
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t display_init(void)
{
    ESP_LOGI(TAG, "=== display_init START ===");

    /* 1 ── SPI2 bus, IO MUX direto (FSPI), DMA automático */
    ESP_LOGI(TAG, "SPI2: SCK=%d MOSI=%d MISO=%d @ %dMHz",
             DISPLAY_PIN_SCK, DISPLAY_PIN_MOSI, DISPLAY_PIN_MISO,
             DISPLAY_SPI_FREQ_HZ / 1000000);

    spi_bus_config_t buscfg = {
        .mosi_io_num     = DISPLAY_PIN_MOSI,
        .miso_io_num     = DISPLAY_PIN_MISO,
        .sclk_io_num     = DISPLAY_PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_LOGI(TAG, "SPI2 bus OK");

    /* 2 ── Panel IO SPI */
    ESP_LOGI(TAG, "Panel IO: CS=%d DC=%d", DISPLAY_PIN_CS, DISPLAY_PIN_DC);
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num       = DISPLAY_PIN_CS,
        .dc_gpio_num       = DISPLAY_PIN_DC,
        .pclk_hz           = DISPLAY_SPI_FREQ_HZ,
        .spi_mode          = 0,   /* ILI9341: CPOL=0 CPHA=0, captura borda subida */
        .trans_queue_depth = 10,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)DISPLAY_SPI_HOST, &io_cfg, &s_io_handle));
    ESP_LOGI(TAG, "Panel IO OK");

    /* 3 ── Hardware reset: RST LOW 20µs → HIGH → esperar 5ms */
    ESP_LOGI(TAG, "HW reset: RST=GPIO%d", DISPLAY_PIN_RST);
    gpio_reset_pin(DISPLAY_PIN_RST);
    gpio_set_direction(DISPLAY_PIN_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(DISPLAY_PIN_RST, 0);
    esp_rom_delay_us(20);
    gpio_set_level(DISPLAY_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_LOGI(TAG, "HW reset OK");

    /* 4 ── Instanciar painel ILI9341 (managed component) */
    ESP_LOGI(TAG, "Criando painel ILI9341");
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,                        /* RST gerenciado manualmente */
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .data_endian    = LCD_RGB_DATA_ENDIAN_BIG,
        .bits_per_pixel = DISPLAY_COLOR_BITS,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(s_io_handle, &panel_cfg, &s_panel));
    ESP_LOGI(TAG, "ILI9341 handle OK");

    /* 5 ── Reset SW (SWRESET 0x01, esperar 5ms) + sequência de init */
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    vTaskDelay(pdMS_TO_TICKS(5));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_LOGI(TAG, "Init sequence OK");

    /* 6 ── Orientação landscape 320×240: swap XY + mirror X */
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true),
                        TAG, "swap_xy falhou");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, false),
                        TAG, "mirror falhou");
    ESP_LOGI(TAG, "Landscape OK (320x240)");

    /* 7 ── Display ON */
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
    ESP_LOGI(TAG, "Display ON");

    /* 8 ── Backlight: LEDC init + fade 0→100% em 500ms */
    ESP_ERROR_CHECK(backlight_init());
    const uint32_t max_duty = (1U << DISPLAY_BL_LEDC_RES) - 1;
    ESP_ERROR_CHECK(ledc_set_fade_with_time(
        LEDC_LOW_SPEED_MODE, DISPLAY_BL_LEDC_CH, max_duty, 500));
    ESP_ERROR_CHECK(ledc_fade_start(
        LEDC_LOW_SPEED_MODE, DISPLAY_BL_LEDC_CH, LEDC_FADE_NO_WAIT));
    ESP_LOGI(TAG, "Backlight fade 0→100%% iniciado (500ms)");

    s_initialized = true;
    ESP_LOGI(TAG, "=== display_init OK (%dx%d RGB565) ===",
             DISPLAY_WIDTH, DISPLAY_HEIGHT);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * display_deinit
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t display_deinit(void)
{
    if (!s_initialized) return ESP_OK;
    ESP_LOGI(TAG, "display_deinit");

    display_set_backlight(0);
    ledc_fade_func_uninstall();

    if (s_panel)     { esp_lcd_panel_del(s_panel);        s_panel = NULL; }
    if (s_io_handle) { esp_lcd_panel_io_del(s_io_handle); s_io_handle = NULL; }
    spi_bus_free(DISPLAY_SPI_HOST);

    s_initialized = false;
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * display_lvgl_flush_cb
 * ═══════════════════════════════════════════════════════════════════════════ */
void display_lvgl_flush_cb(void *display, const void *area, uint8_t *px_map)
{
    lv_display_t    *disp = (lv_display_t *)display;
    const lv_area_t *a    = (const lv_area_t *)area;

    if (!s_initialized || !s_panel) {
        lv_display_flush_ready(disp);
        return;
    }

    /* x_end e y_end são exclusivos na API esp_lcd */
    esp_err_t ret = esp_lcd_panel_draw_bitmap(
        s_panel,
        (int)a->x1, (int)a->y1,
        (int)a->x2 + 1, (int)a->y2 + 1,
        px_map);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "draw_bitmap: %s", esp_err_to_name(ret));
    }
    lv_display_flush_ready(disp);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * display_set_backlight
 * ═══════════════════════════════════════════════════════════════════════════ */
void display_set_backlight(uint8_t percent)
{
    if (percent > 100) percent = 100;
    const uint32_t max_duty = (1U << DISPLAY_BL_LEDC_RES) - 1;
    uint32_t duty = ((uint32_t)percent * max_duty + 50) / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, DISPLAY_BL_LEDC_CH, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, DISPLAY_BL_LEDC_CH);
    ESP_LOGI(TAG, "Backlight %u%% (duty=%lu/%lu)",
             percent, (unsigned long)duty, (unsigned long)max_duty);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * display_get_panel_handle
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_lcd_panel_handle_t display_get_panel_handle(void)
{
    return s_panel;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * display_set_window  (window definida implicitamente por draw_bitmap)
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    (void)x0; (void)y0; (void)x1; (void)y1;
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * display_send_pixels  (legado)
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t display_send_pixels(const uint16_t *data, size_t len)
{
    (void)data; (void)len;
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * display_fill_rect  — preenche retângulo linha por linha
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t display_fill_rect(uint16_t x0, uint16_t y0,
                            uint16_t w,  uint16_t h, uint16_t color)
{
    if (!s_initialized || !s_panel || w == 0 || h == 0) return ESP_OK;

    uint16_t *line = MT_MALLOC_DRAM((size_t)w * sizeof(uint16_t));
    if (!line) {
        ESP_LOGW(TAG, "fill_rect: sem memoria (%u px por linha)", w);
        return ESP_ERR_NO_MEM;
    }
    for (uint16_t i = 0; i < w; i++) line[i] = color;

    esp_err_t ret = ESP_OK;
    for (uint16_t row = 0; row < h && ret == ESP_OK; row++) {
        ret = esp_lcd_panel_draw_bitmap(
            s_panel, x0, y0 + row, x0 + w, y0 + row + 1, line);
    }
    MT_FREE(line);
    return ret;
}

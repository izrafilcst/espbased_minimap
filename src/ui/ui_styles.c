/**
 * @file ui_styles.c
 * @brief Estilos LVGL 9.x para o MeshTracker.
 *        Paleta escura com acentos azul/verde.
 */

#include "ui_styles.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "lvgl.h"

static const char *TAG = "ui_styles";

/* ─── Estilos globais reutilizáveis ─────────────────────────────────────── */
static lv_style_t s_style_panel;
static lv_style_t s_style_label_primary;
static lv_style_t s_style_label_secondary;
static lv_style_t s_style_btn_nav;

static bool s_initialized = false;

/* Helper: converte hex RGB 24-bit para lv_color_t */
static inline lv_color_t hex_to_lv(uint32_t hex)
{
    return lv_color_make((hex >> 16) & 0xFF,
                         (hex >>  8) & 0xFF,
                         (hex      ) & 0xFF);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ui_styles_init
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t ui_styles_init(void)
{
    if (s_initialized) return ESP_OK;
    ESP_LOGI(TAG, "ui_styles_init");

    /* ── Painel escuro ─────────────────────────────────────────────────── */
    lv_style_init(&s_style_panel);
    lv_style_set_bg_color(&s_style_panel,   hex_to_lv(UI_COLOR_PANEL));
    lv_style_set_bg_opa(&s_style_panel,     LV_OPA_COVER);
    lv_style_set_border_color(&s_style_panel, hex_to_lv(UI_COLOR_BORDER));
    lv_style_set_border_width(&s_style_panel, 1);
    lv_style_set_radius(&s_style_panel,     4);
    lv_style_set_pad_all(&s_style_panel,    6);

    /* ── Label primário ────────────────────────────────────────────────── */
    lv_style_init(&s_style_label_primary);
    lv_style_set_text_color(&s_style_label_primary, hex_to_lv(UI_COLOR_TEXT));
    lv_style_set_text_font(&s_style_label_primary,  &lv_font_montserrat_14);

    /* ── Label secundário ──────────────────────────────────────────────── */
    lv_style_init(&s_style_label_secondary);
    lv_style_set_text_color(&s_style_label_secondary, hex_to_lv(UI_COLOR_TEXT_DIM));
    lv_style_set_text_font(&s_style_label_secondary,  &lv_font_montserrat_12);

    /* ── Botão de navegação ────────────────────────────────────────────── */
    lv_style_init(&s_style_btn_nav);
    lv_style_set_bg_color(&s_style_btn_nav,   hex_to_lv(UI_COLOR_PANEL));
    lv_style_set_bg_opa(&s_style_btn_nav,     LV_OPA_COVER);
    lv_style_set_border_color(&s_style_btn_nav, hex_to_lv(UI_COLOR_PRIMARY));
    lv_style_set_border_width(&s_style_btn_nav, 1);
    lv_style_set_text_color(&s_style_btn_nav,   hex_to_lv(UI_COLOR_PRIMARY));
    lv_style_set_radius(&s_style_btn_nav,       4);
    lv_style_set_pad_all(&s_style_btn_nav,      4);

    s_initialized = true;
    ESP_LOGI(TAG, "ui_styles_init OK");
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Aplicadores de estilo
 * ═══════════════════════════════════════════════════════════════════════════ */
void ui_styles_apply_panel(void *obj)
{
    if (!obj) return;
    lv_obj_add_style((lv_obj_t *)obj, &s_style_panel, LV_PART_MAIN);
}

void ui_styles_apply_label_primary(void *obj)
{
    if (!obj) return;
    lv_obj_add_style((lv_obj_t *)obj, &s_style_label_primary, LV_PART_MAIN);
}

void ui_styles_apply_label_secondary(void *obj)
{
    if (!obj) return;
    lv_obj_add_style((lv_obj_t *)obj, &s_style_label_secondary, LV_PART_MAIN);
}

void ui_styles_apply_btn_nav(void *obj)
{
    if (!obj) return;
    lv_obj_add_style((lv_obj_t *)obj, &s_style_btn_nav, LV_PART_MAIN);
}

void ui_styles_apply_rssi_indicator(void *obj, int8_t rssi)
{
    if (!obj) return;
    lv_color_t color;
    if (rssi > -80) {
        color = hex_to_lv(UI_COLOR_SECONDARY);  /* verde — sinal bom */
    } else if (rssi > -100) {
        color = hex_to_lv(UI_COLOR_WARNING);    /* laranja — sinal médio */
    } else {
        color = hex_to_lv(UI_COLOR_ERROR);      /* vermelho — sinal ruim */
    }
    lv_obj_set_style_bg_color((lv_obj_t *)obj, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, LV_OPA_COVER, LV_PART_MAIN);
}

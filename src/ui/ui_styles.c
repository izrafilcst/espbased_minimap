/**
 * @file ui_styles.c
 * @brief Estilos LVGL globais do MeshTracker.
 */

#include "ui_styles.h"
#include "meshtracker_config.h"
#include "esp_log.h"

static const char *TAG = "ui_styles";

/* ─── Implementações (stubs) ────────────────────────────────────────────────*/

esp_err_t ui_styles_init(void)
{
    ESP_LOGI(TAG, "ui_styles_init: stub");
    return ESP_OK;
}

void ui_styles_apply_panel(void *obj)
{
    (void)obj;
}

void ui_styles_apply_label_primary(void *obj)
{
    (void)obj;
}

void ui_styles_apply_label_secondary(void *obj)
{
    (void)obj;
}

void ui_styles_apply_btn_nav(void *obj)
{
    (void)obj;
}

void ui_styles_apply_rssi_indicator(void *obj, int8_t rssi)
{
    (void)obj; (void)rssi;
}

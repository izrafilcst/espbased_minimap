/**
 * @file main.c
 * @brief Ponto de entrada do MeshTracker.
 *        Inicializa todos os subsistemas em ordem e cria as tasks FreeRTOS.
 *
 * Ordem de inicialização:
 *   1. NVS flash
 *   2. Drivers: display → LoRa → GPS → botões
 *   3. Utils: (sem init necessário — funções puras)
 *   4. Mesh: node_table → relay → beacon
 *   5. UI: engine (cria task LVGL no Core 1)
 */

#include "meshtracker_config.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Drivers */
#include "drivers/display_ili9341.h"
#include "drivers/lora_sx1278.h"
#include "drivers/gps_neo6m.h"
#include "drivers/buttons.h"

/* Mesh */
#include "mesh/node_table.h"
#include "mesh/mesh_protocol.h"
#include "mesh/mesh_relay.h"
#include "mesh/mesh_beacon.h"

/* UI */
#include "ui/ui_engine.h"
#include "ui/ui_styles.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "MeshTracker v%d.%d.%d iniciando...",
             MESHTRACKER_VERSION_MAJOR,
             MESHTRACKER_VERSION_MINOR,
             MESHTRACKER_VERSION_PATCH);

    /* 1. NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 2. Drivers — Core 0 tasks criadas internamente por cada init */
    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(lora_init());
    ESP_ERROR_CHECK(gps_init());
    ESP_ERROR_CHECK(buttons_init());

    /* 3. Mesh */
    ESP_ERROR_CHECK(node_table_init());
    ESP_ERROR_CHECK(mesh_relay_init());
    ESP_ERROR_CHECK(mesh_beacon_init());

    /* 4. UI — cria task LVGL no Core 1 */
    ESP_ERROR_CHECK(ui_styles_init());
    ESP_ERROR_CHECK(ui_engine_init());

    ESP_LOGI(TAG, "Todos os subsistemas inicializados. Sistema operacional.");

    /* app_main retorna — scheduler FreeRTOS assume controle */
}

#ifndef UI_ENGINE_H
#define UI_ENGINE_H

/**
 * @file ui_engine.h
 * @brief Motor de UI baseado em LVGL 9.x.
 *        Gerencia o timer tick do LVGL, o display flush e a task
 *        de rendering no Core 1.
 *        Expõe funções para navegar entre telas.
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "meshtracker_config.h"

/* ─── Tipos públicos ─────────────────────────────────────────────────────── */

/** Identificadores de tela */
typedef enum {
    UI_SCREEN_SPLASH  = 0,  /**< Tela de boot/splash                       */
    UI_SCREEN_MAP     = 1,  /**< Tela principal com mapa de nós             */
    UI_SCREEN_NODE    = 2,  /**< Detalhes de um nó selecionado              */
    UI_SCREEN_STATUS  = 3,  /**< Status do sistema (GPS, bateria, etc.)     */
    UI_SCREEN_SETTINGS= 4,  /**< Configurações                              */
    UI_SCREEN_COUNT,
} ui_screen_id_t;

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Inicializa o LVGL (lv_init), registra o display driver
 *        (flush callback), aloca framebuffer na PSRAM, cria
 *        a task LVGL no Core 1 e mostra a tela splash.
 * @return ESP_OK em sucesso.
 */
esp_err_t ui_engine_init(void);

/**
 * @brief Para a task LVGL e libera recursos.
 * @return ESP_OK em sucesso.
 */
esp_err_t ui_engine_deinit(void);

/**
 * @brief Navega para a tela especificada com animação padrão.
 * @param screen_id Tela de destino (ui_screen_id_t).
 * @return ESP_OK se tela existe, ESP_ERR_INVALID_ARG caso contrário.
 */
esp_err_t ui_engine_switch_screen(ui_screen_id_t screen_id);

/**
 * @brief Retorna o identificador da tela atualmente ativa.
 */
ui_screen_id_t ui_engine_current_screen(void);

/**
 * @brief Processa um evento de botão recebido da fila de botões.
 *        Redireciona para a tela ativa.
 *        Chamar a partir da task de UI ou de qualquer contexto LVGL.
 * @param btn_id   Identificador do botão.
 * @param evt_type Tipo do evento (press, long-press, release).
 */
void ui_engine_handle_button(uint8_t btn_id, uint8_t evt_type);

/**
 * @brief Task interna de rendering LVGL — NÃO chamar diretamente.
 *        Chama lv_timer_handler() a cada 5ms.
 */
void ui_engine_lvgl_task(void *arg);

#endif /* UI_ENGINE_H */

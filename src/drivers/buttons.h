#ifndef BUTTONS_H
#define BUTTONS_H

/**
 * @file buttons.h
 * @brief Driver para os três botões de navegação (UP, OK, DOWN).
 *        Debounce por software, detecção de press curto e long-press.
 *        Eventos publicados via fila FreeRTOS.
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "meshtracker_config.h"

/* ─── Tipos públicos ─────────────────────────────────────────────────────── */

/** Identificador de botão */
typedef enum {
    BTN_ID_UP   = 0,
    BTN_ID_OK   = 1,
    BTN_ID_DOWN = 2,
} btn_id_t;

/** Tipo de evento de botão */
typedef enum {
    BTN_EVENT_PRESS      = 0,  /**< Pressão curta (< BTN_LONG_PRESS_MS)    */
    BTN_EVENT_LONG_PRESS = 1,  /**< Pressão longa (>= BTN_LONG_PRESS_MS)   */
    BTN_EVENT_RELEASE    = 2,  /**< Botão solto                             */
} btn_event_type_t;

/** Estrutura de evento publicada na fila */
typedef struct {
    btn_id_t         id;       /**< Qual botão gerou o evento              */
    btn_event_type_t type;     /**< Tipo de evento                         */
    uint32_t         time_ms;  /**< Tempo de pressão em ms (para PRESS)    */
} btn_event_t;

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Configura GPIO dos três botões como entrada com pull-up interno.
 *        Instala ISR de borda e cria fila de eventos.
 *        Cria task de debounce pinada no Core 1.
 * @return ESP_OK em sucesso.
 */
esp_err_t buttons_init(void);

/**
 * @brief Libera recursos do driver de botões.
 * @return ESP_OK em sucesso.
 */
esp_err_t buttons_deinit(void);

/**
 * @brief Retorna o handle da fila de eventos de botões.
 *        Outros módulos podem aguardar eventos nesta fila.
 * @return Handle da QueueHandle_t, ou NULL se não inicializado.
 */
QueueHandle_t buttons_get_queue(void);

/**
 * @brief Retorna true se o botão especificado está atualmente pressionado.
 *        Thread-safe (lê GPIO diretamente).
 * @param id Identificador do botão.
 */
bool buttons_is_pressed(btn_id_t id);

#endif /* BUTTONS_H */

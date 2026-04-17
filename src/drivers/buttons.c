/**
 * @file buttons.c
 * @brief Driver de botões UP/OK/DOWN com debounce e long-press.
 *
 * Fluxo:
 *   buttons_init()
 *     → configura GPIOs como entrada com pull-up (ativo-LOW)
 *     → instala ISR de borda (rising + falling) em cada pino
 *     → cria fila s_event_queue (16 × btn_event_t)
 *     → cria buttons_task (Core 1)
 *
 *   ISR (qualquer borda)
 *     → enfileira {id, timestamp} na fila interna s_isr_queue
 *
 *   buttons_task (loop)
 *     → aguarda s_isr_queue por até BTN_DEBOUNCE_MS
 *     → ao receber notificação, aguarda BTN_DEBOUNCE_MS e relê o nível
 *     → se LOW: marca t_press; aguarda subida ou BTN_LONG_PRESS_MS
 *       → se subiu antes: publica BTN_EVENT_PRESS + BTN_EVENT_RELEASE
 *       → se expirou: publica BTN_EVENT_LONG_PRESS; aguarda subida; publica RELEASE
 *     → publica na fila pública s_event_queue (consumida pela UI)
 */

#include "buttons.h"
#include "meshtracker_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "buttons";

/* ─── Mapeamento de IDs → GPIOs ─────────────────────────────────────────── */
static const gpio_num_t k_btn_gpio[3] = {
    [BTN_ID_UP]   = BTN_PIN_UP,
    [BTN_ID_OK]   = BTN_PIN_OK,
    [BTN_ID_DOWN] = BTN_PIN_DOWN,
};

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static QueueHandle_t s_event_queue = NULL;   /* público — consumido pela UI */
static QueueHandle_t s_isr_queue   = NULL;   /* interno — ISR → task        */
static TaskHandle_t  s_task_hdl    = NULL;

/* Estrutura enfileirada pela ISR */
typedef struct {
    btn_id_t id;
    uint32_t timestamp_ms;
} isr_event_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * ISR — executada em borda de qualquer botão
 * ═══════════════════════════════════════════════════════════════════════════ */
static void IRAM_ATTR btn_isr_handler(void *arg)
{
    btn_id_t id = (btn_id_t)(uintptr_t)arg;
    isr_event_t ev = {
        .id           = id,
        .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
    };
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(s_isr_queue, &ev, &woken);
    if (woken) portYIELD_FROM_ISR();
}

/* ═══════════════════════════════════════════════════════════════════════════
 * buttons_task — debounce e detecção de long-press
 * ═══════════════════════════════════════════════════════════════════════════ */
static void buttons_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "buttons_task iniciada (Core %d)", xPortGetCoreID());

    isr_event_t isr_ev;

    while (1) {
        /* Aguarda qualquer borda do ISR */
        if (xQueueReceive(s_isr_queue, &isr_ev, portMAX_DELAY) != pdTRUE)
            continue;

        btn_id_t     id  = isr_ev.id;
        gpio_num_t   pin = k_btn_gpio[id];

        /* Debounce: aguarda e relê */
        vTaskDelay(MS_TO_TICKS(BTN_DEBOUNCE_MS));

        /* Drena eventos repetidos gerados durante o debounce */
        isr_event_t discard;
        while (xQueueReceive(s_isr_queue, &discard, 0) == pdTRUE) {}

        if (gpio_get_level(pin) != 0) continue;   /* solto → ignora */

        /* Botão está pressionado — registra instante */
        uint32_t t_press = (uint32_t)(esp_timer_get_time() / 1000);
        bool     long_fired = false;

        /* Aguarda soltura ou expiração do limiar de long-press */
        while (1) {
            uint32_t elapsed = (uint32_t)(esp_timer_get_time() / 1000) - t_press;

            if (gpio_get_level(pin) != 0) {
                /* Solto antes do long-press */
                btn_event_t ev_press = {
                    .id      = id,
                    .type    = BTN_EVENT_PRESS,
                    .time_ms = elapsed,
                };
                btn_event_t ev_release = {
                    .id      = id,
                    .type    = BTN_EVENT_RELEASE,
                    .time_ms = elapsed,
                };
                xQueueSend(s_event_queue, &ev_press,   0);
                xQueueSend(s_event_queue, &ev_release, 0);
                break;
            }

            if (!long_fired && elapsed >= BTN_LONG_PRESS_MS) {
                /* Long-press detectado — publica imediatamente */
                btn_event_t ev_long = {
                    .id      = id,
                    .type    = BTN_EVENT_LONG_PRESS,
                    .time_ms = elapsed,
                };
                xQueueSend(s_event_queue, &ev_long, 0);
                long_fired = true;
            }

            if (long_fired && gpio_get_level(pin) != 0) {
                btn_event_t ev_rel = {
                    .id      = id,
                    .type    = BTN_EVENT_RELEASE,
                    .time_ms = (uint32_t)(esp_timer_get_time() / 1000) - t_press,
                };
                xQueueSend(s_event_queue, &ev_rel, 0);
                break;
            }

            vTaskDelay(MS_TO_TICKS(10));
        }

        /* Drena qualquer borda residual gerada durante a leitura */
        while (xQueueReceive(s_isr_queue, &discard, 0) == pdTRUE) {}
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * buttons_init
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t buttons_init(void)
{
    ESP_LOGI(TAG, "=== buttons_init START ===");

    s_event_queue = xQueueCreate(16, sizeof(btn_event_t));
    s_isr_queue   = xQueueCreate(8,  sizeof(isr_event_t));
    if (!s_event_queue || !s_isr_queue) {
        ESP_LOGE(TAG, "Falha ao criar filas");
        return ESP_ERR_NO_MEM;
    }

    /* Configura GPIO + ISR para cada botão.
     * gpio_install_isr_service pode já ter sido chamado pelo driver LoRa. */
    esp_err_t isr_ret = gpio_install_isr_service(0);
    if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "gpio_install_isr_service: %s", esp_err_to_name(isr_ret));
        return isr_ret;
    }

    for (int i = 0; i < 3; i++) {
        gpio_num_t pin = k_btn_gpio[i];
        gpio_reset_pin(pin);
        gpio_set_direction(pin, GPIO_MODE_INPUT);
        gpio_set_pull_mode(pin, GPIO_PULLUP_ONLY);
        gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE);
        ESP_ERROR_CHECK(gpio_isr_handler_add(pin, btn_isr_handler,
                                             (void *)(uintptr_t)i));
    }

    BaseType_t rc = xTaskCreatePinnedToCore(
        buttons_task, "buttons",
        TASK_BUTTONS_STACK, NULL,
        TASK_BUTTONS_PRIO, &s_task_hdl,
        TASK_BUTTONS_CORE);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar buttons_task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "=== buttons_init OK (UP=%d OK=%d DOWN=%d) ===",
             BTN_PIN_UP, BTN_PIN_OK, BTN_PIN_DOWN);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * buttons_deinit
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t buttons_deinit(void)
{
    for (int i = 0; i < 3; i++) {
        gpio_isr_handler_remove(k_btn_gpio[i]);
    }
    if (s_task_hdl) { vTaskDelete(s_task_hdl); s_task_hdl = NULL; }
    if (s_event_queue) { vQueueDelete(s_event_queue); s_event_queue = NULL; }
    if (s_isr_queue)   { vQueueDelete(s_isr_queue);   s_isr_queue   = NULL; }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * API pública
 * ═══════════════════════════════════════════════════════════════════════════ */
QueueHandle_t buttons_get_queue(void)
{
    return s_event_queue;
}

bool buttons_is_pressed(btn_id_t id)
{
    if (id >= 3) return false;
    return gpio_get_level(k_btn_gpio[id]) == 0;
}

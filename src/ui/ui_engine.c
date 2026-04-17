/**
 * @file ui_engine.c
 * @brief Motor LVGL 9.x — inicialização, task de rendering e navegação.
 *
 * Inicialização:
 *   1. lv_init()
 *   2. Aloca 2× draw buffers parciais na PSRAM
 *      (DISPLAY_WIDTH × DISPLAY_DMA_BUF_LINES × 2 bytes cada)
 *   3. Registra display driver + flush callback
 *   4. Cria ui_engine_lvgl_task (Core 1) que chama lv_timer_handler() a 5ms
 *   5. Monta tela splash → após 1,5 s → tela principal (mapa)
 *
 * Telas:
 *   SPLASH  — mensagem de boot com versão
 *   MAP     — tela principal: nó próprio no centro + nós visíveis ao redor
 *   NODE    — detalhes de um nó selecionado
 *   STATUS  — GPS, heap, bateria
 *   SETTINGS— brilho, nome (placeholder)
 */

#include "ui_engine.h"
#include "ui_styles.h"
#include "meshtracker_config.h"
#include "drivers/display_ili9341.h"
#include "drivers/buttons.h"
#include "mesh/node_table.h"
#include "utils/geo_math.h"
#include "utils/mac_utils.h"
#include "drivers/gps_neo6m.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ui_engine";

/* ─── Tick LVGL em ms ───────────────────────────────────────────────────── */
#define LVGL_TICK_MS   5

/* ─── Estado interno ────────────────────────────────────────────────────────*/
static lv_display_t  *s_display       = NULL;
static ui_screen_id_t s_current_screen = UI_SCREEN_SPLASH;
static TaskHandle_t   s_task_hdl      = NULL;

/* Handles de tela — criados uma vez */
static lv_obj_t *s_screens[UI_SCREEN_COUNT] = {NULL};

/* Mutex para LVGL (não é thread-safe por padrão) */
static SemaphoreHandle_t s_lvgl_mutex = NULL;

/* ─── Helper: tomar/dar mutex ────────────────────────────────────────────── */
static inline void lvgl_lock(void)   { xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY); }
static inline void lvgl_unlock(void) { xSemaphoreGive(s_lvgl_mutex); }

/* ═══════════════════════════════════════════════════════════════════════════
 * Construção de telas
 * ═══════════════════════════════════════════════════════════════════════════ */

static void build_splash_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "MeshTracker");
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, -16);

    char ver[32];
    snprintf(ver, sizeof(ver), "v%d.%d.%d",
             MESHTRACKER_VERSION_MAJOR,
             MESHTRACKER_VERSION_MINOR,
             MESHTRACKER_VERSION_PATCH);
    lv_obj_t *lbl_ver = lv_label_create(scr);
    lv_label_set_text(lbl_ver, ver);
    lv_obj_set_style_text_color(lbl_ver, lv_color_make(0x88, 0x88, 0x88), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl_ver, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl_ver, LV_ALIGN_CENTER, 0, 8);
}

static void build_map_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* Título */
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "MAP");
    lv_obj_set_style_text_color(lbl, lv_color_make(0x00, 0xBF, 0xFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 4, 4);

    /* Placeholder — o rendering de nós é feito em ui_engine_lvgl_task */
}

static void build_node_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "NODE DETAIL\n—");
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 6, 6);
}

static void build_status_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "STATUS\n—");
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 6, 6);
}

static void build_settings_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "SETTINGS\n—");
    lv_obj_set_style_text_color(lbl, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, LV_PART_MAIN);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 6, 6);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ui_engine_lvgl_task — rendering loop + input de botões
 * ═══════════════════════════════════════════════════════════════════════════ */
void ui_engine_lvgl_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "ui_engine_lvgl_task iniciada (Core %d)", xPortGetCoreID());

    QueueHandle_t btn_q = buttons_get_queue();
    btn_event_t   btn_ev;

    while (1) {
        lvgl_lock();
        lv_timer_handler();
        lv_tick_inc(LVGL_TICK_MS);
        lvgl_unlock();

        /* Processa botões sem bloquear */
        if (btn_q && xQueueReceive(btn_q, &btn_ev, 0) == pdTRUE) {
            ui_engine_handle_button(btn_ev.id, btn_ev.type);
        }

        vTaskDelay(MS_TO_TICKS(LVGL_TICK_MS));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ui_engine_init
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t ui_engine_init(void)
{
    ESP_LOGI(TAG, "=== ui_engine_init START ===");

    s_lvgl_mutex = xSemaphoreCreateMutex();
    if (!s_lvgl_mutex) return ESP_ERR_NO_MEM;

    /* 1. LVGL */
    lv_init();

    /* 2. Display LVGL — regista driver */
    s_display = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    if (!s_display) {
        ESP_LOGE(TAG, "lv_display_create falhou");
        return ESP_FAIL;
    }

    /* 3. Aloca 2× draw buffers parciais na PSRAM */
    size_t buf_sz = DISPLAY_WIDTH * DISPLAY_DMA_BUF_LINES * sizeof(lv_color16_t);
    void *buf1 = MT_MALLOC_PSRAM(buf_sz);
    void *buf2 = MT_MALLOC_PSRAM(buf_sz);
    if (!buf1 || !buf2) {
        ESP_LOGE(TAG, "Falha ao alocar draw buffers na PSRAM");
        if (buf1) MT_FREE(buf1);
        if (buf2) MT_FREE(buf2);
        return ESP_ERR_NO_MEM;
    }

    lv_display_set_buffers(s_display, buf1, buf2, buf_sz,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* 4. Flush callback — chama driver do display */
    lv_display_set_flush_cb(s_display,
        (lv_display_flush_cb_t)display_lvgl_flush_cb);

    /* 5. Cria telas */
    for (int i = 0; i < UI_SCREEN_COUNT; i++) {
        s_screens[i] = lv_obj_create(NULL);
    }
    build_splash_screen(s_screens[UI_SCREEN_SPLASH]);
    build_map_screen(s_screens[UI_SCREEN_MAP]);
    build_node_screen(s_screens[UI_SCREEN_NODE]);
    build_status_screen(s_screens[UI_SCREEN_STATUS]);
    build_settings_screen(s_screens[UI_SCREEN_SETTINGS]);

    /* 6. Mostra splash */
    lv_screen_load(s_screens[UI_SCREEN_SPLASH]);
    s_current_screen = UI_SCREEN_SPLASH;

    /* 7. Task LVGL no Core 1 */
    BaseType_t rc = xTaskCreatePinnedToCore(
        ui_engine_lvgl_task, "lvgl",
        TASK_LVGL_STACK, NULL,
        TASK_LVGL_PRIO, &s_task_hdl,
        TASK_LVGL_CORE);

    if (rc != pdPASS) {
        ESP_LOGE(TAG, "Falha ao criar lvgl_task");
        return ESP_FAIL;
    }

    /* 8. Transição splash → mapa após 1,5 s */
    vTaskDelay(MS_TO_TICKS(1500));
    ui_engine_switch_screen(UI_SCREEN_MAP);

    ESP_LOGI(TAG, "=== ui_engine_init OK ===");
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ui_engine_deinit
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t ui_engine_deinit(void)
{
    if (s_task_hdl) { vTaskDelete(s_task_hdl); s_task_hdl = NULL; }
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ui_engine_switch_screen
 * ═══════════════════════════════════════════════════════════════════════════ */
esp_err_t ui_engine_switch_screen(ui_screen_id_t screen_id)
{
    if (screen_id >= UI_SCREEN_COUNT) return ESP_ERR_INVALID_ARG;
    if (!s_screens[screen_id])        return ESP_ERR_INVALID_STATE;

    lvgl_lock();
    lv_screen_load_anim(s_screens[screen_id],
                        LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    s_current_screen = screen_id;
    lvgl_unlock();

    ESP_LOGI(TAG, "Tela → %d", screen_id);
    return ESP_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ui_engine_current_screen
 * ═══════════════════════════════════════════════════════════════════════════ */
ui_screen_id_t ui_engine_current_screen(void)
{
    return s_current_screen;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ui_engine_handle_button
 * Navegação simples: UP/DOWN percorre telas; OK seleciona.
 * ═══════════════════════════════════════════════════════════════════════════ */
void ui_engine_handle_button(uint8_t btn_id, uint8_t evt_type)
{
    if (evt_type != BTN_EVENT_PRESS) return;

    switch (btn_id) {
        case BTN_ID_UP: {
            int next = (int)s_current_screen - 1;
            if (next < 0) next = UI_SCREEN_COUNT - 1;
            ui_engine_switch_screen((ui_screen_id_t)next);
            break;
        }
        case BTN_ID_DOWN: {
            int next = ((int)s_current_screen + 1) % UI_SCREEN_COUNT;
            ui_engine_switch_screen((ui_screen_id_t)next);
            break;
        }
        case BTN_ID_OK:
            /* Placeholder: OK na tela de mapa → NODE detail */
            if (s_current_screen == UI_SCREEN_MAP) {
                ui_engine_switch_screen(UI_SCREEN_NODE);
            }
            break;
        default:
            break;
    }
}

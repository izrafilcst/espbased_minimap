#ifndef UI_STYLES_H
#define UI_STYLES_H

/**
 * @file ui_styles.h
 * @brief Estilos LVGL reutilizáveis para o MeshTracker.
 *        Paleta de cores, fontes e estilos de widgets compartilhados
 *        entre todas as telas.
 */

#include "esp_err.h"
#include "meshtracker_config.h"

/* ─── Paleta de cores (RGB565 para LVGL) ─────────────────────────────────── */
#define UI_COLOR_BG          0x0A0A0A  /**< Fundo escuro                    */
#define UI_COLOR_PRIMARY     0x00BFFF  /**< Azul primário (Deep Sky Blue)   */
#define UI_COLOR_SECONDARY   0x32CD32  /**< Verde (Lime Green)              */
#define UI_COLOR_WARNING     0xFFA500  /**< Laranja                         */
#define UI_COLOR_ERROR       0xFF3333  /**< Vermelho                        */
#define UI_COLOR_TEXT        0xFFFFFF  /**< Texto branco                    */
#define UI_COLOR_TEXT_DIM    0x888888  /**< Texto cinza                     */
#define UI_COLOR_PANEL       0x1A1A2E  /**< Fundo de painéis                */
#define UI_COLOR_BORDER      0x333355  /**< Bordas                          */
#define UI_COLOR_NODE_SELF   0x00FF7F  /**< Cor do nó próprio no mapa       */
#define UI_COLOR_NODE_NEAR   0x00BFFF  /**< Nós próximos (< 2 hops)         */
#define UI_COLOR_NODE_FAR    0xFF8C00  /**< Nós distantes (>= 3 hops)       */

/* ─── Funções públicas ───────────────────────────────────────────────────── */

/**
 * @brief Inicializa todos os estilos LVGL globais.
 *        Deve ser chamado após lv_init() e antes de criar qualquer widget.
 * @return ESP_OK em sucesso.
 */
esp_err_t ui_styles_init(void);

/**
 * @brief Aplica o estilo de painel escuro a um objeto LVGL.
 * @param obj Objeto LVGL alvo.
 */
void ui_styles_apply_panel(void *obj);

/**
 * @brief Aplica o estilo de label primário (texto branco, font 14).
 * @param obj Objeto LVGL alvo.
 */
void ui_styles_apply_label_primary(void *obj);

/**
 * @brief Aplica o estilo de label secundário (texto cinza, font 12).
 * @param obj Objeto LVGL alvo.
 */
void ui_styles_apply_label_secondary(void *obj);

/**
 * @brief Aplica o estilo de botão de navegação.
 * @param obj Objeto LVGL alvo.
 */
void ui_styles_apply_btn_nav(void *obj);

/**
 * @brief Aplica o estilo de indicador de sinal RSSI.
 *        Cor varia conforme nível: verde > -80, laranja > -100, vermelho resto.
 * @param obj  Objeto LVGL alvo.
 * @param rssi Valor RSSI em dBm.
 */
void ui_styles_apply_rssi_indicator(void *obj, int8_t rssi);

#endif /* UI_STYLES_H */

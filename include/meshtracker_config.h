#ifndef MESHTRACKER_CONFIG_H
#define MESHTRACKER_CONFIG_H

/**
 * @file meshtracker_config.h
 * @brief Central configuration header for MeshTracker project.
 *        All pin definitions, peripheral constants, mesh protocol
 *        parameters and FreeRTOS task configuration live here.
 *
 *        Every source file must include this header.
 */

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * DISPLAY ILI9341 — SPI2 (FSPI, IO MUX direto, 40 MHz)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define DISPLAY_SPI_HOST        SPI2_HOST
#define DISPLAY_SPI_FREQ_HZ     (40 * 1000 * 1000)   /**< 40 MHz prático    */
#define DISPLAY_SPI_FREQ_READ_HZ (6 * 1000 * 1000)   /**< 6 MHz para leitura */

#define DISPLAY_PIN_SCK         12
#define DISPLAY_PIN_MOSI        11
#define DISPLAY_PIN_MISO        13
#define DISPLAY_PIN_CS          10
#define DISPLAY_PIN_DC           9
#define DISPLAY_PIN_RST         14
#define DISPLAY_PIN_BL           8   /**< Backlight via LEDC PWM            */

#define DISPLAY_WIDTH           320
#define DISPLAY_HEIGHT          240
#define DISPLAY_COLOR_BITS       16   /**< RGB565                            */
#define DISPLAY_BL_LEDC_CH        0   /**< Canal LEDC para backlight         */
#define DISPLAY_BL_LEDC_FREQ   5000   /**< Frequência PWM backlight (Hz)     */
#define DISPLAY_BL_LEDC_RES      10   /**< Resolução PWM (bits)              */
#define DISPLAY_BL_LEDC_TIMER  LEDC_TIMER_0

/** Tamanho do DMA transfer buffer (2 linhas × largura × 2 bytes/pixel) */
#define DISPLAY_DMA_BUF_LINES     2
#define DISPLAY_DMA_BUF_SIZE    (DISPLAY_DMA_BUF_LINES * DISPLAY_WIDTH * 2)

/* ═══════════════════════════════════════════════════════════════════════════
 * LoRa RA-02 SX1278 — SPI3 (GPIO Matrix, 10 MHz)
 * ═══════════════════════════════════════════════════════════════════════════ */
#define LORA_SPI_HOST           SPI3_HOST
#define LORA_SPI_FREQ_HZ        (10 * 1000 * 1000)   /**< 10 MHz máximo SX1278 */

#define LORA_PIN_SCK            36
#define LORA_PIN_MOSI           35
#define LORA_PIN_MISO           37
#define LORA_PIN_CS             38
#define LORA_PIN_RST            39
#define LORA_PIN_DIO0           40   /**< IRQ — TxDone / RxDone             */
#define LORA_PIN_DIO1           41   /**< RxTimeout / FhssChangeChannel     */

/* Parâmetros RF */
#define LORA_FREQUENCY_HZ       (433000000UL)  /**< 433 MHz                 */
#define LORA_SYNC_WORD          0x34           /**< Diferente de LoRaWAN 0x12 */
#define LORA_SPREADING_FACTOR    9             /**< SF9                     */
#define LORA_BANDWIDTH_HZ        125000        /**< 125 kHz                 */
#define LORA_CODING_RATE         5             /**< CR 4/5                  */
#define LORA_TX_POWER_DBM        17            /**< 17 dBm                  */
#define LORA_PREAMBLE_LEN        8             /**< Símbolos de preâmbulo   */
#define LORA_CRC_ENABLE          true
#define LORA_IMPLICIT_HEADER     false

/* ═══════════════════════════════════════════════════════════════════════════
 * GPS NEO-6M — UART1
 * ═══════════════════════════════════════════════════════════════════════════ */
#define GPS_UART_PORT           UART_NUM_1
#define GPS_UART_BAUD           9600
#define GPS_UART_BAUD_HIGH      115200         /**< Velocidade após reconf. */
#define GPS_PIN_TX              17             /**< ESP→GPS                 */
#define GPS_PIN_RX              18             /**< GPS→ESP                 */
#define GPS_PIN_PPS             16             /**< Pulse-per-second        */
#define GPS_RX_BUF_SIZE         (1024 * 2)    /**< Buffer UART RX          */
#define GPS_NMEA_MAX_LEN        100            /**< Tamanho max sentença    */

/* ═══════════════════════════════════════════════════════════════════════════
 * Botões
 * ═══════════════════════════════════════════════════════════════════════════ */
#define BTN_PIN_UP               4
#define BTN_PIN_OK               5
#define BTN_PIN_DOWN             6
#define BTN_DEBOUNCE_MS         30             /**< Debounce em ms          */
#define BTN_LONG_PRESS_MS      800             /**< Limiar de long-press    */

/* ═══════════════════════════════════════════════════════════════════════════
 * Protocolo Mesh LoRa
 * ═══════════════════════════════════════════════════════════════════════════ */
#define MESH_MAGIC              0x4D54U        /**< 'MT' — identificador    */
#define MESH_SRC_ID_LEN          6             /**< MAC address (bytes)     */
#define MESH_MAX_HOPS            5             /**< TTL máximo              */
#define MESH_PKT_ID_BITS        16             /**< 16-bit sequence number  */
#define MESH_MAX_PAYLOAD_BYTES  200            /**< Payload máximo          */
#define MESH_CRC_LEN             2             /**< CRC16 (bytes)           */

/** Overhead fixo: MAGIC(2) + SRC_ID(6) + HOP(1) + PKT_ID(2) + TYPE(1) + CRC(2) */
#define MESH_HEADER_LEN         12
#define MESH_MAX_PKT_LEN        (MESH_HEADER_LEN + MESH_MAX_PAYLOAD_BYTES + MESH_CRC_LEN)

/** Tipos de pacote */
#define MESH_PKT_BEACON         0x01   /**< Beacon com posição GPS          */
#define MESH_PKT_ACK            0x02   /**< Acknowledge                     */
#define MESH_PKT_TEXT           0x03   /**< Mensagem de texto               */
#define MESH_PKT_STATUS         0x04   /**< Status do nó                    */

/** Temporização */
#define MESH_BEACON_INTERVAL_MS  30000  /**< Beacon a cada 30s              */
#define MESH_RELAY_DELAY_MS        100  /**< Delay antes de relayt          */
#define MESH_DEDUP_TABLE_SIZE       32  /**< Entradas de deduplicação       */
#define MESH_DEDUP_TTL_MS         5000  /**< TTL da entrada dedup           */

/* ═══════════════════════════════════════════════════════════════════════════
 * Tabela de nós visíveis
 * ═══════════════════════════════════════════════════════════════════════════ */
#define NODE_TABLE_MAX_NODES     20    /**< Nós simultâneos máximos         */
#define NODE_TABLE_TIMEOUT_MS  120000  /**< Nó removido após 2 min sem beacon */

/* ═══════════════════════════════════════════════════════════════════════════
 * FreeRTOS Tasks
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Core 0: rede, LoRa, GPS */
#define TASK_LORA_RX_STACK      4096
#define TASK_LORA_RX_PRIO          6
#define TASK_LORA_RX_CORE          0

#define TASK_LORA_TX_STACK      4096
#define TASK_LORA_TX_PRIO          5
#define TASK_LORA_TX_CORE          0

#define TASK_GPS_STACK          4096
#define TASK_GPS_PRIO              4
#define TASK_GPS_CORE              0

#define TASK_MESH_RELAY_STACK   4096
#define TASK_MESH_RELAY_PRIO       5
#define TASK_MESH_RELAY_CORE       0

#define TASK_MESH_BEACON_STACK  3072
#define TASK_MESH_BEACON_PRIO      3
#define TASK_MESH_BEACON_CORE      0

/* Core 1: UI, display, LVGL */
#define TASK_LVGL_STACK         8192
#define TASK_LVGL_PRIO             5
#define TASK_LVGL_CORE             1

#define TASK_DISPLAY_STACK      4096
#define TASK_DISPLAY_PRIO          4
#define TASK_DISPLAY_CORE          1

#define TASK_BUTTONS_STACK      2048
#define TASK_BUTTONS_PRIO          3
#define TASK_BUTTONS_CORE          1

/* ═══════════════════════════════════════════════════════════════════════════
 * Memória / PSRAM
 * ═══════════════════════════════════════════════════════════════════════════ */
#define MT_MALLOC_PSRAM(size)   heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define MT_MALLOC_DRAM(size)    heap_caps_malloc((size), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define MT_FREE(ptr)            heap_caps_free((ptr))

/* ═══════════════════════════════════════════════════════════════════════════
 * Misc
 * ═══════════════════════════════════════════════════════════════════════════ */
#define MESHTRACKER_VERSION_MAJOR  0
#define MESHTRACKER_VERSION_MINOR  1
#define MESHTRACKER_VERSION_PATCH  0

/** Converte milissegundos para ticks FreeRTOS */
#define MS_TO_TICKS(ms)         pdMS_TO_TICKS(ms)

#endif /* MESHTRACKER_CONFIG_H */

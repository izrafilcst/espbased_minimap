# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## O que é este projeto
Dispositivo embarcado de rastreamento GPS com rede mesh LoRa. Cada nó transmite
sua posição via LoRa 433MHz e repete pacotes de outros nós, formando uma rede mesh
sem infraestrutura. Display ILI9341 mostra posição própria e de todos os nós visíveis.

## Stack Técnico
- **MCU:** ESP32-S3 N16R8 (dual-core LX7 240MHz, 16MB flash, 8MB PSRAM Octal)
- **Framework:** ESP-IDF via PlatformIO (NÃO Arduino)
- **Linguagem:** C99
- **RTOS:** FreeRTOS (incluído no ESP-IDF)
- **UI:** LVGL 9.x (via IDF Component Manager)
- **Display:** ILI9341 320×240 via SPI2 (FSPI, IO MUX direto, 40MHz) + `espressif/esp_lcd_ili9341`
- **LoRa:** RA-02 SX1278 433MHz via SPI3 (GPIO Matrix, 10MHz) — biblioteca `jgromes/RadioLib@^7.1.0`
- **GPS:** NEO-6M via UART1 (9600 baud)

## Comandos de Build / Flash / Monitor

```bash
# Build (ambiente padrão)
pio run -e esp32-s3-devkitc-1

# Build debug (CORE_DEBUG_LEVEL=4)
pio run -e esp32-s3-devkitc-1-debug

# Flash
pio run -e esp32-s3-devkitc-1 -t upload

# Monitor serial (115200 baud, com decodificador de exceções)
pio device monitor -e esp32-s3-devkitc-1

# Build + flash + monitor em sequência
pio run -e esp32-s3-devkitc-1 -t upload && pio device monitor

# Limpar build
pio run -t clean

# Rodar testes unitários (pasta test/)
pio test -e esp32-s3-devkitc-1
```

## Mapeamento de Pinos (OBRIGATÓRIO seguir)
### Display ILI9341 — SPI2 (FSPI)
- SCK=GPIO12, MOSI=GPIO11, MISO=GPIO13, CS=GPIO10
- DC=GPIO9, RST=GPIO14, BL=GPIO8 (LEDC PWM)

### LoRa RA-02 — SPI3
- SCK=GPIO36, MOSI=GPIO35, MISO=GPIO37, CS=GPIO38
- RST=GPIO39, DIO0=GPIO40 (IRQ), DIO1=GPIO41

### GPS NEO-6M — UART1
- TX(ESP→GPS)=GPIO17, RX(GPS→ESP)=GPIO18, PPS=GPIO16

### Botões
- UP=GPIO4, OK=GPIO5, DOWN=GPIO6

## Arquitetura e Ordem de Inicialização

`app_main` inicializa os subsistemas nesta ordem (ver `src/main.c`):
1. NVS flash
2. Drivers: `display_init()` → `lora_init()` → `gps_init()` → `buttons_init()`
3. Mesh: `node_table_init()` → `mesh_relay_init()` → `mesh_beacon_init()`
4. UI: `ui_styles_init()` → `ui_engine_init()` (cria task LVGL no Core 1)

Cada `_init()` cria suas próprias FreeRTOS tasks internamente. `app_main` retorna após isso — o scheduler assume controle.

## Afinidade de Cores FreeRTOS
- **Core 0:** LoRa RX/TX, GPS, Mesh relay, Mesh beacon
- **Core 1:** Display, LVGL, Botões

Tasks são criadas com `xTaskCreatePinnedToCore`. Prioridades e tamanhos de stack estão definidos em `meshtracker_config.h` (ex.: `TASK_LORA_RX_PRIO`, `TASK_LVGL_STACK`).

## Macros de Alocação de Memória
Sempre usar as macros de `meshtracker_config.h` para alocações grandes:
- `MT_MALLOC_PSRAM(size)` — aloca na PSRAM (framebuffers, tabela de nós)
- `MT_MALLOC_DRAM(size)` — aloca na DRAM interna
- `MT_FREE(ptr)` — libera qualquer heap_caps allocation
- `MS_TO_TICKS(ms)` — converte ms para ticks FreeRTOS

## Regras de Código
1. Sempre usar APIs do ESP-IDF (spi_master.h, uart_driver.h, gpio.h). NUNCA Arduino.
2. Usar DMA para transferências SPI do display (`spi_device_queue_trans`).
3. Dados compartilhados entre tasks DEVEM usar mutex (`SemaphoreHandle_t`).
4. Incluir `meshtracker_config.h` em todo arquivo fonte — centraliza todos os defines.
5. Naming: `snake_case` funções/variáveis, `UPPER_CASE` defines/constantes.
6. Cada módulo `.c` tem seu `.h` com include guard.
7. Log: `ESP_LOGI/LOGW/LOGE` com tag do módulo (ex.: `static const char *TAG = "lora"`).
8. Sem variáveis globais expostas — getters/setters protegidos por mutex.

## Restrições de Hardware Críticas
- RA-02: alimentação 3.3V SOMENTE. NUNCA 5V. Danifica o chip.
- RA-02: NUNCA transmitir sem antena conectada (danifica o PA do SX1278).
- ILI9341: Clock SPI oficial max 10MHz (escrita). 40MHz funciona na prática.
- ILI9341: SPI Mode 0 (dados capturados na borda de subida do SCK).
- ILI9341: Após Sleep Out (0x11), OBRIGATÓRIO esperar 120ms.
- ILI9341: Após Software Reset (0x01), esperar 5ms (ou 120ms se estava em Sleep Out).
- NEO-6M: Baud rate padrão 9600. Pode reconfigurar para 115200 via UBX.
- ESP32-S3 N16R8: GPIOs 26-32 e 33-37 ocupados por flash/PSRAM. NÃO usar.

## Protocolo Mesh LoRa
Pacote: `[MAGIC 2B][SRC_ID 6B][HOP 1B][PKT_ID 2B][TYPE 1B][PAYLOAD 0-200B][CRC 2B]`
- `MAGIC = 0x4D54` (`'MT'`)
- `SRC_ID` = MAC address do nó origem (não muda nos relays)
- `HOP` incrementa a cada relay (max 5)
- `SYNC_WORD = 0x34` (diferente de LoRaWAN `0x12`)
- SF=9, BW=125kHz, CR=4/5, TX Power=17dBm
- Tipos: `MESH_PKT_BEACON(0x01)`, `MESH_PKT_ACK(0x02)`, `MESH_PKT_TEXT(0x03)`, `MESH_PKT_STATUS(0x04)`

# MeshTracker — Contexto do Projeto

## O que é este projeto
Dispositivo embarcado de rastreamento GPS com rede mesh LoRa. Cada nó transmite
sua posição via LoRa 433MHz e repete pacotes de outros nós, formando uma rede mesh
sem infraestrutura. Display ILI9341 mostra posição própria e de todos os nós visíveis.

## Stack Técnico
- **MCU:** ESP32-S3 N16R8 (dual-core LX7 240MHz, 16MB flash, 8MB PSRAM Octal)
- **Framework:** ESP-IDF via PlatformIO (NÃO Arduino)
- **Linguagem:** C99
- **RTOS:** FreeRTOS (incluído no ESP-IDF)
- **UI:** LVGL 9.x
- **Display:** ILI9341 320×240 via SPI2 (FSPI, IO MUX direto, 40MHz)
- **LoRa:** RA-02 SX1278 433MHz via SPI3 (GPIO Matrix, 10MHz)
- **GPS:** NEO-6M via UART1 (9600 baud)

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

## Regras de Código
1. Sempre usar APIs do ESP-IDF (spi_master.h, uart_driver.h, gpio.h). NUNCA Arduino.
2. Usar DMA para transferências SPI do display (spi_device_queue_trans).
3. Todas as tasks FreeRTOS devem ser pinadas em cores específicos:
   - Core 0: LoRa RX/TX, GPS, rede
   - Core 1: Display, UI, LVGL
4. Dados compartilhados entre tasks DEVEM usar mutex (SemaphoreHandle_t).
5. Buffers grandes (framebuffers, tabela de nós) alocados na PSRAM via heap_caps_malloc.
6. Includes do projeto usam: #include "meshtracker_config.h" para todos os defines.
7. Naming convention: snake_case para funções e variáveis, UPPER_CASE para defines/constantes.
8. Cada módulo (.c) tem seu .h correspondente com include guard.
9. Log via ESP_LOGI/ESP_LOGW/ESP_LOGE com tag do módulo.
10. Sem variáveis globais expostas — usar getters/setters com mutex.

## Restrições de Hardware Críticas
- RA-02: alimentação 3.3V SOMENTE. NUNCA 5V. Danifica o chip.
- RA-02: NUNCA transmitir sem antena conectada (danifica o PA do SX1278).
- ILI9341: Clock SPI oficial max 10MHz (escrita). 40MHz funciona na prática.
- ILI9341: Dados capturados na borda de subida do SCL. SPI Mode 0.
- ILI9341: Após Sleep Out (0x11), OBRIGATÓRIO esperar 120ms.
- ILI9341: Após Software Reset (0x01), esperar 5ms (ou 120ms se estava em Sleep Out).
- NEO-6M: Baud rate padrão 9600. Pode reconfigurar para 115200 via UBX.
- ESP32-S3 N16R8: GPIOs 26-32 e 33-37 ocupados por flash/PSRAM. NÃO usar.
- Capacitores de bypass 100nF obrigatórios próximos a VCC de cada módulo.

## Estrutura do Projeto
meshtracker/
├── platformio.ini
├── sdkconfig.defaults
├── partitions.csv
├── CLAUDE.md
├── include/
│   └── meshtracker_config.h
├── src/
│   ├── main.c
│   ├── drivers/    (display, lora, gps, buttons)
│   ├── mesh/       (protocol, relay, beacon, node_table)
│   ├── ui/         (engine, styles, screens/, components/)
│   └── utils/      (nmea_parser, geo_math, mac_utils)
├── data/           (SPIFFS: fontes, ícones)
└── test/           (testes unitários)

## Protocolo Mesh LoRa
Pacote: [MAGIC 2B][SRC_ID 6B][HOP 1B][PKT_ID 2B][TYPE 1B][PAYLOAD 0-200B][CRC 2B]
- MAGIC = 0x4D54
- SRC_ID = MAC address do nó origem (não muda nos relays)
- HOP incrementa a cada relay (max 5)
- SYNC_WORD = 0x34 (diferente de LoRaWAN 0x12)
- SF=9, BW=125kHz, CR=4/5, TX Power=17dBm
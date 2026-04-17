# MeshTracker

Dispositivo embarcado de rastreamento GPS com rede **mesh LoRa 433 MHz** sem infraestrutura. Cada nó transmite sua posição via LoRa, repete pacotes de outros nós (multi-hop) e exibe em um display ILI9341 320×240 o mapa relativo de todos os nós visíveis na rede.

---

## Índice

1. [Visão Geral](#1-visão-geral)
2. [Stack de Hardware e Software](#2-stack-de-hardware-e-software)
3. [Mapeamento de Pinos](#3-mapeamento-de-pinos)
4. [Estrutura do Projeto](#4-estrutura-do-projeto)
5. [Arquitetura de Software](#5-arquitetura-de-software)
6. [Documentação dos Módulos](#6-documentação-dos-módulos)
7. [Protocolo Mesh LoRa](#7-protocolo-mesh-lora)
8. [Build, Flash e Monitor](#8-build-flash-e-monitor)
9. [Status de Implementação](#9-status-de-implementação)
10. [Restrições Críticas de Hardware](#10-restrições-críticas-de-hardware)

---

## 1. Visão Geral

O **MeshTracker** é um nó de rede mesh LoRa que combina:

- **GPS** (NEO-6M) para obter sua posição geográfica.
- **Rádio LoRa** (RA-02 / SX1278 a 433 MHz) para transmitir e receber beacons da rede mesh.
- **Display colorido** (ILI9341 320×240 RGB565) para mostrar:
  - Posição própria e dos nós vizinhos em um mapa orientado.
  - Estatísticas do rádio, GPS, bateria, satélites e hops.
- **Interface por 3 botões** (UP / OK / DOWN) com debounce e long-press.

Cada nó envia um **beacon** periódico (a cada 30 s por padrão) com latitude, longitude, altitude, velocidade, satélites e nome curto. Todos os outros nós que escutam esse beacon repetem (relay) o pacote se o contador de hops ainda permitir, até `MESH_MAX_HOPS = 5`. Um **dedup table** (`SRC_ID + PKT_ID`) evita tempestades de retransmissão. A **tabela de nós** guarda o último beacon de cada MAC visto nos últimos 2 minutos.

---

## 2. Stack de Hardware e Software

| Componente | Especificação |
|---|---|
| **MCU** | ESP32-S3 N16R8 — dual-core Xtensa LX7 240 MHz, 16 MB Flash QIO, 8 MB PSRAM Octal 80 MHz |
| **Framework** | ESP-IDF 5.5.3 via PlatformIO (**não** Arduino) |
| **Linguagem** | C99 |
| **RTOS** | FreeRTOS (built-in ESP-IDF, tick 1 kHz, dual-core) |
| **UI** | LVGL 9.x (IDF Component Manager) |
| **Display** | ILI9341 320×240 RGB565 — SPI2/FSPI @ 40 MHz + `espressif/esp_lcd_ili9341` |
| **LoRa** | RA-02 SX1278 433 MHz — SPI3 @ 10 MHz + `jgromes/RadioLib@^7.1.0` |
| **GPS** | NEO-6M NMEA — UART1 @ 9600 (rcfig. opc. para 115200) |
| **Particionamento** | 3 MB app factory, 1 MB SPIFFS (fontes/ícones), 64 KB coredump |

**Alocações específicas**:
- `CONFIG_SPI_MASTER_ISR_IN_IRAM=y` — ISRs SPI rápidas mesmo durante flash writes.
- `CONFIG_UART_ISR_IN_IRAM=y` — mesma ideia para UART do GPS.
- `CONFIG_SPIRAM_USE_CAPS_ALLOC=y` — `heap_caps_malloc(MALLOC_CAP_SPIRAM)` habilitado.
- `CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=16384` — allocs < 16 KB ficam na DRAM interna; maiores vão pra PSRAM.

---

## 3. Mapeamento de Pinos

Todos os pinos estão centralizados em [`include/meshtracker_config.h`](include/meshtracker_config.h). **Nunca hard-code um pino fora desse header.**

### Display ILI9341 — SPI2 (FSPI, IO MUX direto, 40 MHz)

| Sinal | GPIO |
|---|---|
| SCK  | 12 |
| MOSI | 11 |
| MISO | 13 |
| CS   | 10 |
| DC   | 9  |
| RST  | 14 |
| BL (Backlight PWM) | 8 |

### LoRa RA-02 SX1278 — SPI3 (GPIO Matrix, 10 MHz)

| Sinal | GPIO |
|---|---|
| SCK  | 36 |
| MOSI | 35 |
| MISO | 37 |
| CS   | 38 |
| RST  | 39 |
| DIO0 (IRQ TxDone/RxDone) | 40 |
| DIO1 (RxTimeout/FhssChangeCh) | 41 |

### GPS NEO-6M — UART1 (9600 baud padrão)

| Sinal | GPIO |
|---|---|
| TX (ESP → GPS) | 17 |
| RX (GPS → ESP) | 18 |
| PPS (pulse-per-second) | 16 |

### Botões (pull-up interno, ativo-LOW)

| Sinal | GPIO |
|---|---|
| UP   | 4 |
| OK   | 5 |
| DOWN | 6 |

> **GPIOs 26–32 e 33–37** estão reservados pelo flash QIO e PSRAM Octal no ESP32-S3 N16R8 — **não usar**. A única exceção são os pinos da SPI3 do LoRa (35/36/37/38/39/40/41), que o N16R8 libera normalmente via GPIO Matrix.

---

## 4. Estrutura do Projeto

```
meshtracker/
├── platformio.ini                  # Config PlatformIO (env release + debug)
├── sdkconfig.defaults              # Defaults do ESP-IDF (PSRAM, FreeRTOS, IRAM ISR)
├── partitions.csv                  # Tabela de partições (app 3MB, SPIFFS 1MB)
├── CMakeLists.txt                  # Top-level (auto)
├── CLAUDE.md                       # Diretrizes para Claude Code
├── README.md                       # Este arquivo
│
├── include/
│   └── meshtracker_config.h        # ÚNICA fonte de defines (pinos, consts, tasks)
│
├── src/
│   ├── CMakeLists.txt              # GLOB src/*.*
│   ├── main.c                      # app_main — init + spawn de tasks
│   ├── idf_component.yml           # Dependências IDF Component Manager
│   │
│   ├── drivers/                    # Camada HAL dos periféricos
│   │   ├── display_ili9341.{c,h}   # Driver ILI9341 via esp_lcd + LEDC BL
│   │   ├── lora_sx1278.{c,h}       # Driver SX1278 (SPI3 + DIO0 IRQ)
│   │   ├── gps_neo6m.{c,h}         # UART1 + parsing NMEA
│   │   └── buttons.{c,h}           # 3 botões com debounce + long-press
│   │
│   ├── mesh/                       # Protocolo mesh LoRa
│   │   ├── mesh_protocol.{c,h}     # Serialização/CRC16 de pacotes
│   │   ├── mesh_relay.{c,h}        # RX + dedup + retransmissão
│   │   ├── mesh_beacon.{c,h}       # TX periódico da posição local
│   │   └── node_table.{c,h}        # Tabela de nós visíveis (PSRAM)
│   │
│   ├── ui/                         # Interface LVGL 9.x
│   │   ├── ui_engine.{c,h}         # Task LVGL, flush, navegação
│   │   └── ui_styles.{c,h}         # Paleta, estilos e tema
│   │
│   └── utils/                      # Funções puras (sem estado)
│       ├── nmea_parser.{c,h}       # Parser GGA/RMC + checksum
│       ├── geo_math.{c,h}          # Haversine, bearing, projeção
│       └── mac_utils.{c,h}         # Formatação/comparação de MAC
│
├── data/                           # Conteúdo SPIFFS (fontes, ícones)
├── test/                           # Testes unitários (pio test)
└── managed_components/             # Baixados pelo IDF Component Manager (git-ignored)
```

---

## 5. Arquitetura de Software

### 5.1. Ordem de inicialização (`src/main.c`)

```
app_main()
  ├─ 1. nvs_flash_init()                    [erase+retry se corrupto]
  │
  ├─ 2. Drivers (Core 0 em geral)
  │     ├─ display_init()                   [Core 1 — LVGL flush]
  │     ├─ lora_init()                      [Core 0 — ISR DIO0]
  │     ├─ gps_init()                       [Core 0 — task NMEA RX]
  │     └─ buttons_init()                   [Core 1 — task debounce]
  │
  ├─ 3. Mesh
  │     ├─ node_table_init()                [aloca PSRAM]
  │     ├─ mesh_relay_init()                [task relay Core 0]
  │     └─ mesh_beacon_init()               [task beacon Core 0]
  │
  └─ 4. UI
        ├─ ui_styles_init()                 [LVGL styles]
        └─ ui_engine_init()                 [task LVGL Core 1]

  app_main retorna — scheduler FreeRTOS assume.
```

### 5.2. Afinidade de Cores

| Core 0 — **Rede** | Core 1 — **UI** |
|---|---|
| `lora_rx_task` (prio 6) | `lvgl_task` (prio 5) |
| `lora_tx_task` (prio 5) | `display_task` (prio 4) |
| `mesh_relay_task` (prio 5) | `buttons_task` (prio 3) |
| `gps_task` (prio 4) | |
| `mesh_beacon_task` (prio 3) | |

Prioridades e stack sizes em `meshtracker_config.h` (`TASK_*_PRIO`, `TASK_*_STACK`, `TASK_*_CORE`).

### 5.3. Sincronização

- **Mutex (`SemaphoreHandle_t`)** para todo dado compartilhado entre tasks:
  - `gps_data_t` interno de `gps_neo6m`.
  - `mesh_node_t[]` de `node_table`.
- **Queues (`QueueHandle_t`)** para eventos assíncronos:
  - `buttons_get_queue()` → fila de `btn_event_t`.
  - `mesh_relay` → fila interna de `mesh_packet_t`.
- **ISRs** (DIO0 do LoRa, borda dos botões) só enfileiram e notificam — jamais fazem I/O bloqueante.

### 5.4. Memória

| Tipo | Macro | Uso típico |
|---|---|---|
| PSRAM Octal (8 MB) | `MT_MALLOC_PSRAM(size)` | framebuffer LVGL, `node_table`, buffers grandes |
| DRAM interna (512 KB) | `MT_MALLOC_DRAM(size)` | linhas curtas, estruturas de controle |
| Release | `MT_FREE(ptr)` | qualquer uma das duas |

`MS_TO_TICKS(ms)` → `pdMS_TO_TICKS(ms)` — converter intervalos.

---

## 6. Documentação dos Módulos

Cada módulo vive em `src/<categoria>/<nome>.{c,h}` e expõe apenas a API do `.h`. Toda a API pública segue três regras:

1. **`init()` retorna `esp_err_t`** — sucesso = `ESP_OK`.
2. **Getters são thread-safe** (bloqueiam no mutex interno se houver estado compartilhado).
3. **ISRs nunca chamam funções do módulo** — só enfileiram.

### 6.1. Drivers

#### `drivers/display_ili9341` — Display ILI9341 320×240

Driver completo em cima do `esp_lcd` panel API + `esp_lcd_ili9341` (managed component). **Único módulo atualmente totalmente implementado.**

**Sequência de `display_init()`**:
1. `spi_bus_initialize(SPI2_HOST, DMA_AUTO)` — habilita DMA.
2. `esp_lcd_new_panel_io_spi()` — cria IO em cima do bus, 40 MHz, SPI mode 0.
3. **Hardware reset manual** — `RST` LOW 20 µs → HIGH → aguarda 5 ms.
4. `esp_lcd_new_panel_ili9341()` — instancia driver do painel (BGR, 16bpp).
5. `esp_lcd_panel_reset()` + `esp_lcd_panel_init()` — SWRESET + seq. de comandos.
6. `swap_xy(true)` + `mirror(true, false)` — landscape 320×240 com origem top-left.
7. `disp_on_off(true)` — Display ON.
8. `backlight_init()` — LEDC LOW_SPEED_MODE, 5 kHz, 10-bit, canal 0 → **fade 0→100% em 500 ms** (evita flash inicial).

**API pública**:
| Função | Descrição |
|---|---|
| `display_init()` / `display_deinit()` | Lifecycle |
| `display_lvgl_flush_cb(disp, area, px_map)` | Callback registrado no `lv_display_set_flush_cb`. Chama `esp_lcd_panel_draw_bitmap` e `lv_display_flush_ready()`. |
| `display_set_backlight(percent)` | 0–100 % via duty PWM |
| `display_fill_rect(x, y, w, h, color)` | Helper que aloca linha temporária (`MT_MALLOC_DRAM`) e desenha linha por linha |
| `display_get_panel_handle()` | Escape hatch para chamadas `esp_lcd_*` diretas |

**Detalhes críticos**:
- O painel é configurado com `LCD_RGB_ELEMENT_ORDER_BGR` — ILI9341 é BGR por padrão.
- `LCD_RGB_DATA_ENDIAN_BIG` — LVGL no S3 precisa de byte-swap.
- `trans_queue_depth = 10` na panel IO permite pipeline DMA de 10 transações.

#### `drivers/lora_sx1278` — Rádio LoRa

Driver para o SX1278 no RA-02. Atualmente **stub**; a implementação final usará a `jgromes/RadioLib` já vendorizada via `platformio.ini`.

**Modelo operacional**:
- Sempre em **RX contínuo** por padrão (`lora_start_rx(cb)`).
- Ao receber pacote: ISR em DIO0 → lê FIFO via SPI → entrega via `lora_rx_cb_t` para a task.
- `lora_send()` é síncrono do ponto de vista de enfileiramento, mas a transmissão é assíncrona: volta pra RX quando DIO0 sobe em TxDone, chama `lora_tx_done_cb_t`.

**Parâmetros RF (em `meshtracker_config.h`)**:
```c
LORA_FREQUENCY_HZ    = 433_000_000   // 433 MHz ISM
LORA_SYNC_WORD       = 0x34          // distinto de LoRaWAN (0x12)
LORA_SPREADING_FACTOR= 9             // SF9 — ~1,7 kbps, alcance ~10 km rural
LORA_BANDWIDTH_HZ    = 125_000       // 125 kHz
LORA_CODING_RATE     = 5             // 4/5
LORA_TX_POWER_DBM    = 17            // 50 mW; máx legal no ISM 433 na maioria dos países
LORA_PREAMBLE_LEN    = 8
LORA_CRC_ENABLE      = true
```

**API pública**: `lora_init/deinit`, `lora_send`, `lora_start_rx/stop_rx`, `lora_read_rssi`, `lora_is_transmitting`, `lora_reset`.

#### `drivers/gps_neo6m` — Receptor GPS

Driver do NEO-6M via UART1. Atualmente **stub** com mutex + struct de saída prontos.

**Plano de implementação**:
1. `uart_driver_install(UART_NUM_1, GPS_RX_BUF_SIZE*2, 0, 0, NULL, 0)`.
2. `uart_set_pin(...GPS_PIN_TX, GPS_PIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE)`.
3. Task `gps_rx_task` (Core 0, prio 4) — lê linhas `\r\n`-terminated, valida checksum NMEA, despacha para `nmea_parse_gga()` / `nmea_parse_rmc()`.
4. Atualiza `s_gps_data` sob mutex.
5. Opcionalmente `gps_set_baud_115200()` envia UBX `CFG-PRT` e reabre UART em 115200 (recomendado para tráfego NMEA denso).

**API pública**: `gps_init/deinit`, `gps_get_data(out)` (cópia thread-safe), `gps_has_fix()`, `gps_set_baud_115200()`.

#### `drivers/buttons` — 3 Botões

3 GPIOs com pull-up interno, ativo-LOW. Stub com `xQueueCreate(16, sizeof(btn_event_t))` pronto.

**Plano de implementação**:
- ISR em borda de descida/subida de cada pino → envia notificação para a task de debounce.
- Task `buttons_task` (Core 1) aguarda 30 ms (`BTN_DEBOUNCE_MS`) e reconfirma o nível.
- Se ficar pressionado por ≥ 800 ms (`BTN_LONG_PRESS_MS`), publica `BTN_EVENT_LONG_PRESS` antes do `RELEASE`.

**Tipos**:
```c
typedef enum { BTN_ID_UP, BTN_ID_OK, BTN_ID_DOWN } btn_id_t;
typedef enum { BTN_EVENT_PRESS, BTN_EVENT_LONG_PRESS, BTN_EVENT_RELEASE } btn_event_type_t;
typedef struct { btn_id_t id; btn_event_type_t type; uint32_t time_ms; } btn_event_t;
```

**API pública**: `buttons_init/deinit`, `buttons_get_queue()`, `buttons_is_pressed(id)`.

### 6.2. Mesh

#### `mesh/mesh_protocol` — Serialização / CRC

Layout binário do pacote:

```
 offset  0  2       8  9 11 12              (12+payload)  (12+payload+2)
        ┌──┬────────┬──┬──┬──┬─────────────┬──────────────┐
        │MG│SRC_ID 6│HP│PK│TY│  PAYLOAD    │    CRC16     │
        │2B│        │1B│2B│1B│   0-200 B   │     2 B      │
        └──┴────────┴──┴──┴──┴─────────────┴──────────────┘
```

- `MG` = `MESH_MAGIC` (0x4D54 = "MT") — LSB first.
- `SRC_ID` = MAC do **nó origem**, nunca muda em relay.
- `HP` = hop count, 0 na origem, incrementa antes de cada retransmissão. Descartar se > `MESH_MAX_HOPS (5)`.
- `PK` = sequence number 16-bit, gerado pelo nó origem (`mesh_beacon_next_pkt_id()`).
- `TY` = tipo do pacote (`MESH_PKT_BEACON/ACK/TEXT/STATUS`).
- `CRC` = CRC16-CCITT sobre `MG..PAYLOAD`.

**Funções**:
- `mesh_proto_serialize(pkt, buf, buf_len, *out_len)` — monta bytes + calcula CRC.
- `mesh_proto_deserialize(buf, len, pkt)` — valida magic + CRC, preenche struct.
- `mesh_proto_crc16(data, len)` — CRC16-CCITT.
- `mesh_proto_check_crc(buf, len)` — comparação contra os 2 bytes finais.
- `mesh_proto_encode_beacon(beacon, buf, len, *out_len)` e `decode_beacon(...)` — payload BEACON.

**Payload de `MESH_PKT_BEACON`** (`beacon_payload_t`):
```c
double  latitude, longitude;   // graus decimais
float   altitude_m, speed_kmh;
uint8_t satellites, fix_quality, battery_pct;
char    name[12];              // null-terminated, gerado de mac_to_short_name()
```

#### `mesh/mesh_relay` — RX + Dedup + Retransmissão

Responsável pela "cola" da mesh.

**Fluxo principal**:
1. ISR DIO0 do LoRa recebe pacote → `lora` chama `mesh_relay_rx_cb(data, len, rssi, snr)`.
2. Callback **apenas enfileira** `{data, rssi, snr}` numa fila interna (8 slots).
3. `mesh_relay_task` (Core 0, prio 5) faz o trabalho pesado:
   1. `mesh_proto_deserialize()` — descarta se magic/CRC errados.
   2. `mesh_relay_is_duplicate(src_id, pkt_id)` — consulta tabela dedup.
   3. Se for **BEACON**, chama `node_table_update(...)` com posição, RSSI, SNR e hop.
   4. Se `hop < MESH_MAX_HOPS`, incrementa hop, re-serializa e chama `lora_send()` após atraso `MESH_RELAY_DELAY_MS = 100 ms` (desempate simples contra tempestades).
   5. `mesh_relay_register(src_id, pkt_id)` — marca como visto.

**Tabela de deduplicação**:
```c
typedef struct {
    uint8_t  src_id[6];
    uint16_t pkt_id;
    uint32_t timestamp_ms;
} dedup_entry_t;
static dedup_entry_t s_dedup[MESH_DEDUP_TABLE_SIZE /* 32 */];
```
Entradas válidas por `MESH_DEDUP_TTL_MS = 5000 ms`; busca linear O(32).

#### `mesh/mesh_beacon` — TX Periódico

**Fluxo principal**:
1. `mesh_beacon_init()` chama `esp_read_mac(..., ESP_MAC_WIFI_STA)` para obter o MAC base do chip como `SRC_ID` do nó.
2. `mesh_beacon_task` (Core 0, prio 3) dorme `MESH_BEACON_INTERVAL_MS = 30000 ms`.
3. Ao acordar:
   1. Consulta `gps_get_data()` — se tem fix, popula `beacon_payload_t`.
   2. Monta `mesh_packet_t` com `SRC_ID = s_src_id`, `PKT_ID = mesh_beacon_next_pkt_id()`, `HOP = 0`, `TYPE = MESH_PKT_BEACON`.
   3. `mesh_proto_encode_beacon()` + `mesh_proto_serialize()`.
   4. `lora_send()` — volta pra dormir.

`mesh_beacon_send_now()` força envio imediato (útil após fix GPS ou mudança brusca de posição).

#### `mesh/node_table` — Nós Visíveis

Array fixo de `NODE_TABLE_MAX_NODES = 20` alocado na **PSRAM** (`MT_MALLOC_PSRAM`), protegido por mutex.

```c
typedef struct {
    uint8_t          node_id[6];
    beacon_payload_t beacon;       // último beacon recebido
    int8_t           rssi, snr;
    uint8_t          last_hop;
    uint32_t         last_seen_ms;
    bool             active;
} mesh_node_t;
```

**Regras**:
- `node_table_update()`: procura por `node_id`. Se existe, atualiza campos; senão, usa primeiro slot `active == false`; senão retorna `ESP_ERR_NO_MEM`.
- `node_table_prune()`: remove entradas com `esp_timer_get_time()/1000 - last_seen_ms > NODE_TABLE_TIMEOUT_MS (120000 ms)`.
- `node_table_get_all(out, *count)`: copia snapshot para o chamador (UI itera sem segurar o mutex por muito tempo).

### 6.3. UI

#### `ui/ui_engine` — Motor LVGL

**Plano**:
1. `ui_engine_init()`:
   - `lv_init()`.
   - Aloca 2× **draw buffers parciais** (`lv_display_set_buffers`) na PSRAM — tamanho `DISPLAY_WIDTH * DISPLAY_DMA_BUF_LINES * 2 B`.
   - `lv_display_set_flush_cb(display_lvgl_flush_cb)`.
   - Cria `ui_engine_lvgl_task` (Core 1, prio 5) que roda `lv_timer_handler()` a cada 5 ms e `lv_tick_inc(5)`.
   - Mostra `UI_SCREEN_SPLASH` por ~1,5 s → transiciona para `UI_SCREEN_MAP`.
2. `ui_engine_switch_screen(id)` — transição com animação LVGL padrão entre telas.
3. `ui_engine_handle_button(btn_id, evt_type)` — chamado pela task de UI ao despachar `btn_event_t`.

**Telas previstas** (`ui_screen_id_t`):
- `UI_SCREEN_SPLASH` — boot.
- `UI_SCREEN_MAP` — principal. Centro = nó próprio. Pontos dos outros nós projetados via `geo_project_to_screen()`.
- `UI_SCREEN_NODE` — detalhes do nó selecionado (MAC, distância, RSSI/SNR, hops, última vez visto).
- `UI_SCREEN_STATUS` — fix GPS, satélites, bateria, chip temp, heap free.
- `UI_SCREEN_SETTINGS` — brilho, nome, intervalo de beacon.

#### `ui/ui_styles` — Paleta e Estilos

Paleta completa em `UI_COLOR_*` (hex RGB, convertidas para RGB565 nos helpers). Cores semânticas:

| Macro | Uso |
|---|---|
| `UI_COLOR_NODE_SELF` | verde-primavera — você no mapa |
| `UI_COLOR_NODE_NEAR` | azul — nós com ≤ 2 hops |
| `UI_COLOR_NODE_FAR` | laranja-escuro — nós distantes |
| `UI_COLOR_PRIMARY` | azul deep-sky — destaques |
| `UI_COLOR_WARNING/ERROR` | laranja/vermelho — alertas |

`ui_styles_apply_rssi_indicator(obj, rssi)` muda a cor conforme faixa:
- verde `> -80 dBm`, laranja `> -100 dBm`, vermelho abaixo disso.

### 6.4. Utils

#### `utils/nmea_parser` — Parser NMEA 0183

Funções **puras** (sem estado). Duas sentenças suportadas:

- `$GPGGA` / `$GNGGA` → `nmea_gga_t` — lat, lon, altitude, fix quality, #sats, HDOP, hora UTC.
- `$GPRMC` / `$GNRMC` → `nmea_rmc_t` — lat, lon, velocidade km/h, rumo, data/hora UTC, `valid`.

Helpers:
- `nmea_validate_checksum(sent)` — valida o `*HH` final (XOR byte-a-byte do conteúdo entre `$` e `*`).
- `nmea_coord_to_decimal(DDDMM.MMMM, 'N'|'S'|'E'|'W')` — converte para graus decimais com sinal (já implementado).
- `nmea_sentence_type(sent)` — retorna `"GGA"`, `"RMC"`, etc. ignorando o prefixo `GP/GN/GL`.

#### `utils/geo_math` — Matemática Geográfica

- `geo_distance_m(lat1, lon1, lat2, lon2)` — fórmula **Haversine** com `GEO_EARTH_RADIUS_M = 6371000`.
- `geo_bearing_deg(...)` — bearing inicial em graus (0 = Norte, 90 = Leste, etc.).
- `geo_project_to_screen(lat, lon, center_lat, center_lon, scale_mpx, w, h, *px, *py)` — projeção equiretangular simples centrada em `(center_*)`, convertendo metros → pixels via `scale_mpx`. Retorna `ESP_ERR_NOT_FOUND` se o ponto cai fora da tela.
- `geo_format_distance(dist_m, buf, size)` — **já implementado**: `"%.0f m"` < 1 km, `"%.1f km"` ≥ 1 km.
- `geo_bearing_to_cardinal(deg)` — **já implementado**: `N/NE/E/SE/S/SW/W/NW` pelo setor de 45°.

#### `utils/mac_utils` — Endereços MAC

**Todas implementadas**:
- `mac_get_local(out[6])` → `esp_read_mac(out, ESP_MAC_WIFI_STA)`.
- `mac_to_string(mac, buf, size)` → `"AA:BB:CC:DD:EE:FF"`.
- `mac_to_short_name(mac, buf, size)` → `"DDEEFF"` (últimos 3 bytes em hex). Usado como `beacon.name[12]`.
- `mac_equal(a, b)` / `mac_copy(dst, src)` — wrappers de `memcmp`/`memcpy`.

---

## 7. Protocolo Mesh LoRa

### 7.1. Camada física

| Parâmetro | Valor |
|---|---|
| Freq. | 433 MHz (ISM na maioria das regiões) |
| SF | 9 |
| BW | 125 kHz |
| CR | 4/5 |
| Sync word | `0x34` (distinto de LoRaWAN `0x12`) |
| TX power | 17 dBm (≈ 50 mW) |
| Preâmbulo | 8 símbolos |
| CRC de hardware | habilitado |
| Cabeçalho | explícito |

Com SF9 / BW125 / CR4/5 o **bitrate efetivo** é ~1,7 kbps e o time-on-air de um pacote de ~30 B ≈ 180 ms. Respeite **duty cycle regional** (ex.: ETSI 1 % em 433,050–434,790 MHz): com `MESH_BEACON_INTERVAL_MS = 30 000 ms` você está em < 1 %.

### 7.2. Camada de enlace / mesh

1. Todo nó ouve continuamente (RX contínuo, baixíssimo consumo no SX1278).
2. Cada nó emite um BEACON a cada 30 s com sua posição.
3. Ao receber qualquer pacote: se não for duplicata (pelo par `(SRC_ID, PKT_ID)` nos últimos 5 s), **retransmite** incrementando `HOP` — mesh é flooding controlado.
4. Pacotes com `HOP >= MESH_MAX_HOPS = 5` são descartados.
5. Retransmissão aguarda `MESH_RELAY_DELAY_MS = 100 ms` para reduzir colisões (outros nós que receberam ao mesmo tempo vão transmitir em momentos diferentes).

### 7.3. Tipos de pacote

| Tipo | Código | Payload |
|---|---|---|
| `MESH_PKT_BEACON` | `0x01` | `beacon_payload_t` — posição + status |
| `MESH_PKT_ACK` | `0x02` | ack de recebimento |
| `MESH_PKT_TEXT` | `0x03` | mensagem de texto curta |
| `MESH_PKT_STATUS` | `0x04` | status detalhado (bateria, temp, heap) |

---

## 8. Build, Flash e Monitor

### 8.1. Pré-requisitos

- PlatformIO Core (via `pip`, VSCode ou CLion).
- Driver USB do ESP32-S3-DevKitC-1 (CP210x ou CDC nativo, dependendo da revisão).
- Em Windows: se o seu usuário tem espaço no nome (ex.: `C:\Users\Nome Sobrenome\`), exporte `PLATFORMIO_HOME_DIR=C:\pio-home` **antes** do build — a toolchain IDF não aceita whitespace no caminho dos pacotes.

### 8.2. Comandos

```bash
# Build (release)
pio run -e esp32-s3-devkitc-1

# Build debug (CORE_DEBUG_LEVEL=4)
pio run -e esp32-s3-devkitc-1-debug

# Flash (detecta COM automaticamente)
pio run -e esp32-s3-devkitc-1 -t upload

# Monitor serial (115200, exception decoder + colorize)
pio device monitor

# Build + flash + monitor (one-liner)
pio run -e esp32-s3-devkitc-1 -t upload && pio device monitor

# Clean
pio run -t clean

# Testes (pasta test/)
pio test -e esp32-s3-devkitc-1
```

### 8.3. Resultado do primeiro build limpo

```
RAM:    6.6% (usados 21 776 B de 327 680 B internos)
Flash:  9.4% (usados 295 429 B de 3 145 728 B na partição factory)
```

(Esperado aumentar conforme stubs forem substituídos pela implementação real.)

---

## 9. Status de Implementação

| Módulo | Status | Observações |
|---|---|---|
| `drivers/display_ili9341` | ✅ **Completo** | esp_lcd panel + LEDC backlight com fade. |
| `drivers/lora_sx1278` | 🚧 stub | Estrutura pronta; falta wiring via RadioLib. |
| `drivers/gps_neo6m` | 🚧 stub | Mutex + struct ok; falta task NMEA RX. |
| `drivers/buttons` | 🚧 stub | Queue criada; falta ISR + debounce. |
| `mesh/mesh_protocol` | 🚧 stub | Assinaturas prontas; falta CRC16 + marshaling. |
| `mesh/mesh_relay` | 🚧 stub | Queue + dedup table prontas; falta task. |
| `mesh/mesh_beacon` | 🚧 stub | `src_id` e `pkt_id` prontos; falta task TX. |
| `mesh/node_table` | 🚧 stub | Aloca PSRAM e mutex; falta update/prune. |
| `ui/ui_engine` | 🚧 stub | Falta `lv_init`, draw buffers e telas. |
| `ui/ui_styles` | 🚧 stub | Falta criar os `lv_style_t` e a aplicação. |
| `utils/nmea_parser` | 🟡 parcial | `nmea_coord_to_decimal` e `nmea_sentence_type` prontos. |
| `utils/geo_math` | 🟡 parcial | `geo_format_distance` e `geo_bearing_to_cardinal` prontos. |
| `utils/mac_utils` | ✅ **Completo** | 5/5 funções implementadas. |

Stubs retornam `ESP_OK` sem efeito colateral. `main.c` usa `ESP_ERROR_CHECK(...)` em todos os `init()` — se algum falhar, o sistema reinicia via panic handler.

---

## 10. Restrições Críticas de Hardware

### RA-02 (SX1278)
- **Alimentar SOMENTE com 3,3 V.** 5 V destrói o PA do SX1278 permanentemente.
- **NUNCA transmitir sem antena** (ou carga de 50 Ω). Transmitir em circuito aberto danifica o PA.
- Capacitor de bypass 100 nF + 10 µF próximo ao VCC do módulo (picos de corrente no TX).

### ILI9341
- Clock SPI máximo oficial de escrita: 10 MHz. Na prática, 40 MHz funciona (validado aqui) porque é o que o IO MUX do ESP32-S3 FSPI permite direto sem GPIO Matrix.
- Leitura do framebuffer: 6 MHz máx. (`DISPLAY_SPI_FREQ_READ_HZ`).
- SPI Mode 0 obrigatório (CPOL=0 CPHA=0, captura na borda de **subida**).
- Após **Sleep Out (0x11)**: aguardar **120 ms** antes do próximo comando (gerido pelo driver esp_lcd).
- Após **Software Reset (0x01)**: aguardar **5 ms** (ou 120 ms se estava em Sleep Out) — implementado explicitamente em `display_init()`.

### NEO-6M
- Baud padrão 9600 — a maioria dos módulos genéricos chineses perde configuração quando o backup cap interno descarrega. Se usar `gps_set_baud_115200()`, grave via UBX `CFG-CFG` se o módulo tiver EEPROM.
- PPS fica em 0 até o primeiro fix; use como **disciplinamento de clock** opcional, não como presença de dados.

### ESP32-S3 N16R8
- **GPIOs 26–37 (exceto 33/34)** estão ocupados por Flash QIO e PSRAM Octal. **Não usar** em nenhuma hipótese. O driver do PSRAM silenciosamente desalinha se um desses pinos for reconfigurado.
- PSRAM Octal @ 80 MHz exige `CONFIG_SPIRAM_MODE_OCT=y` + `CONFIG_SPIRAM_SPEED_80M=y` (já em `sdkconfig.defaults`).
- Capacitores 100 nF em cada VCC (ESP, LoRa, GPS, LCD) são **obrigatórios** — a placa tem picos de > 300 mA durante TX LoRa.

---

## Licença

A definir.

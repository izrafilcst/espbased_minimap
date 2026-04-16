# Projeto MeshTracker
## Dispositivo de Rastreamento com Rede Mesh LoRa + GPS + Display ILI9341

**Versão:** 0.1-draft
**Data:** Abril 2026
**Hardware Base:** ESP32-S3 N16R8 + LoRa RA-02 (SX1278) 433MHz + NEO-6M GPS + ILI9341 320×240

---

## 1. Visão Geral

### 1.1 Objetivo

Criar um dispositivo portátil que:

- Captura posição GPS (latitude, longitude, altitude, velocidade)
- Transmite sua identidade (MAC address) e posição via LoRa 433 MHz
- Participa de uma rede mesh, repetindo pacotes de outros nós
- Exibe em tempo real a posição própria e de todos os nós visíveis na rede
- Permite criação de interfaces visuais no display ILI9341 usando uma abordagem declarativa similar a HTML/CSS

### 1.2 Diagrama de Blocos

```
┌──────────────────────────────────────────────────────┐
│                   MeshTracker Node                    │
│                                                       │
│  ┌─────────────┐    SPI (80MHz)    ┌──────────────┐  │
│  │  ILI9341    │◄──────────────────│              │  │
│  │  320×240    │                   │              │  │
│  │  Display    │    SPI (10MHz)    │   ESP32-S3   │  │
│  └─────────────┘   ┌──────────────│   N16R8      │  │
│                     │              │              │  │
│  ┌─────────────┐   │              │  240MHz      │  │
│  │  LoRa RA-02 │◄──┘    UART      │  16MB Flash  │  │
│  │  SX1278     │   ┌──────────────│  8MB PSRAM   │  │
│  │  433 MHz    │   │              │              │  │
│  └──────┬──────┘   │              └──────────────┘  │
│         │          │                     │           │
│    ┌────┴────┐  ┌──┴──────────┐    ┌────┴────┐     │
│    │ Antena  │  │  NEO-6M GPS │    │  USB-C  │     │
│    │ 433MHz  │  │  + Antena   │    │  Power  │     │
│    └─────────┘  └─────────────┘    └─────────┘     │
│                                                       │
│  ┌─────────┐  ┌─────────┐  ┌─────────┐              │
│  │  Botão  │  │  Botão  │  │  Botão  │  (Navegação) │
│  │   UP    │  │  OK/SEL │  │  DOWN   │              │
│  └─────────┘  └─────────┘  └─────────┘              │
└──────────────────────────────────────────────────────┘
```

### 1.3 Conceito da Rede Mesh

```
   Nó A (campo)          Nó B (repetidor)          Nó C (base)
  ┌───────────┐         ┌───────────┐           ┌───────────┐
  │ GPS + LoRa│─ ─ ─ ─ ─│ GPS + LoRa│─ ─ ─ ─ ─ │ GPS + LoRa│
  │  TX/RX    │  LoRa   │  TX/RX    │   LoRa    │  TX/RX    │
  │  Beacon   │  433MHz │  Repete   │   433MHz  │  Exibe    │
  └───────────┘         │  Pacotes  │           │  Mapa     │
                         └───────────┘           └───────────┘
       │                      │                       │
   Alcance direto         Alcance direto          Alcance direto
   ~2-5 km (campo)        ~2-5 km                 ~2-5 km
       │◄─────────────────────────────────────────────►│
                    Alcance total: ~4-10 km
                    (com 1 repetidor)
```

Cada nó funciona simultaneamente como **transmissor**, **receptor** e **repetidor**. Não existe hierarquia — todos os nós são iguais.

---

## 2. Hardware

### 2.1 Bill of Materials (BOM)

| # | Componente | Especificação | Qty | Interface | Tensão |
|---|-----------|---------------|-----|-----------|--------|
| 1 | ESP32-S3 N16R8 | DevKit-C ou módulo | 1 | — | 3.3V |
| 2 | Display ILI9341 | 2.8" TFT 320×240 SPI | 1 | SPI (HSPI) | 3.3V |
| 3 | LoRa RA-02 | SX1278, 433 MHz | 1 | SPI (VSPI) | 3.3V |
| 4 | GPS NEO-6M | u-blox, com antena cerâmica | 1 | UART | 3.3V |
| 5 | Antena 433MHz | SMA ou wire (17.3 cm para λ/4) | 1 | RF | — |
| 6 | Botões | Tactile switch 6mm | 3 | GPIO | — |
| 7 | Resistores pull-up | 10kΩ | 3 | — | — |
| 8 | Capacitores | 100nF + 10µF (bypass) | 4 | — | — |
| 9 | Regulador | AMS1117-3.3V ou similar | 1 | — | 5V→3.3V |
| 10 | Bateria (opcional) | LiPo 3.7V 2000mAh + TP4056 | 1 | — | 3.7V |

### 2.2 Pinout — Mapeamento Completo

O ESP32-S3 tem 45 GPIOs, mas muitos estão ocupados pela flash/PSRAM (N16R8 usa Octal SPI). Os pinos disponíveis devem ser cuidadosamente alocados entre os três periféricos SPI/UART.

**Barramentos SPI:**

O ILI9341 e o LoRa RA-02 **ambos usam SPI**, mas em frequências e modos diferentes. Existem duas opções:

- **Opção A:** Compartilhar SPI2 (FSPI) com CS separado — mais simples, menos pinos
- **Opção B:** Usar SPI2 para display e SPI3 para LoRa — isolamento total, sem contenção

**Escolha: Opção B** (SPI separados) — o display precisa de DMA a 40MHz contínuo e o LoRa pode interromper transações a qualquer momento.

```
═══════════════════════════════════════════════════════════
 DISPLAY ILI9341 — SPI2 (FSPI) — IO MUX direto
═══════════════════════════════════════════════════════════
 ILI9341 Pin    ESP32-S3 GPIO    Função          Nota
─────────────────────────────────────────────────────────
 SCK            GPIO12           FSPICLK         IO MUX nativo
 MOSI (SDI)     GPIO11           FSPID           IO MUX nativo
 MISO (SDO)     GPIO13           FSPIQ           IO MUX (opcional)
 CS             GPIO10           FSPICS0         IO MUX nativo
 DC             GPIO9            GPIO             Data/Command
 RST            GPIO14           GPIO             Hardware Reset
 BL             GPIO8            LEDC PWM ch0     Backlight

═══════════════════════════════════════════════════════════
 LORA RA-02 (SX1278) — SPI3 — via GPIO Matrix
═══════════════════════════════════════════════════════════
 RA-02 Pin      ESP32-S3 GPIO    Função          Nota
─────────────────────────────────────────────────────────
 SCK            GPIO36           SPI3_CLK        GPIO Matrix
 MOSI           GPIO35           SPI3_D          GPIO Matrix
 MISO           GPIO37           SPI3_Q          GPIO Matrix
 NSS (CS)       GPIO38           SPI3_CS         GPIO Matrix
 RST            GPIO39           GPIO             LoRa Reset
 DIO0 (IRQ)     GPIO40           GPIO + ISR       Interrupt on RX
 DIO1           GPIO41           GPIO (opcional)  Timeout/CAD

═══════════════════════════════════════════════════════════
 GPS NEO-6M — UART1
═══════════════════════════════════════════════════════════
 NEO-6M Pin     ESP32-S3 GPIO    Função          Nota
─────────────────────────────────────────────────────────
 TX             GPIO18           U1RXD (entrada)  GPS → ESP32
 RX             GPIO17           U1TXD (saída)    ESP32 → GPS
 PPS            GPIO16           GPIO + ISR       Pulse-per-second

═══════════════════════════════════════════════════════════
 BOTÕES — GPIOs com pull-up interno
═══════════════════════════════════════════════════════════
 Botão          ESP32-S3 GPIO    Função
─────────────────────────────────────────────────────────
 UP             GPIO4            Navegar cima
 OK/SELECT      GPIO5            Confirmar/entrar
 DOWN           GPIO6            Navegar baixo

═══════════════════════════════════════════════════════════
 DEBUG — UART0 (USB nativo)
═══════════════════════════════════════════════════════════
 U0TXD          GPIO43           Console serial
 U0RXD          GPIO44           Console serial
```

### 2.3 Esquema de Conexão RA-02

O módulo RA-02 da Ai-Thinker encapsula o SX1278 e precisa de atenção especial:

```
                  RA-02 (SX1278)
                 ┌──────────────┐
           GND ──┤1  GND    VCC├── 3.3V (NÃO usar 5V!)
           ──────┤2  MISO  RST ├── GPIO39
           ──────┤3  MOSI  DIO5├── NC
           ──────┤4  SCK   DIO3├── NC
           ──────┤5  NSS   DIO4├── NC
           ──────┤6  DIO0  DIO2├── NC
           ──────┤7  DIO1  DIO1├── GPIO41 (opcional)
           GND ──┤8  GND   ANT ├── Antena 433MHz
                 └──────────────┘

CRÍTICO: 
- Alimentar com 3.3V (max 3.7V). NUNCA 5V!
- A antena DEVE estar conectada antes de transmitir
  (transmitir sem antena danifica o PA do SX1278)
- Capacitor de 100nF entre VCC e GND, o mais perto possível do módulo
- DIO0 é essencial — gera interrupção quando pacote é recebido
```

### 2.4 Esquema de Conexão NEO-6M

```
                  NEO-6M GPS
                 ┌──────────────┐
           VCC ──┤ VCC (3.3V)   │
           GND ──┤ GND          │
    GPIO18 ◄─────┤ TX           │  (GPS envia NMEA para ESP32)
    GPIO17 ─────►┤ RX           │  (ESP32 envia comandos UBX)
    GPIO16 ◄─────┤ PPS          │  (Pulso 1Hz preciso)
                 └──────────────┘

Nota: O NEO-6M fala NMEA 0183 a 9600 baud por padrão.
Pode ser reconfigurado para 115200 baud via protocolo UBX.
```

---

## 3. Arquitetura de Software

### 3.1 Visão Geral das Tasks

```
┌─────────────────────────────────────────────────────┐
│                    FreeRTOS                          │
│                                                      │
│  Core 0 (Protocolo)          Core 1 (UI/Display)    │
│  ┌──────────────────┐       ┌──────────────────┐    │
│  │ task_lora_rx      │       │ task_display      │    │
│  │ Prioridade: 6     │       │ Prioridade: 5     │    │
│  │ Recebe pacotes    │       │ LVGL tick + flush  │    │
│  │ Parse + relay     │       │ Atualiza telas     │    │
│  └──────────────────┘       └──────────────────┘    │
│  ┌──────────────────┐       ┌──────────────────┐    │
│  │ task_lora_tx      │       │ task_ui_logic     │    │
│  │ Prioridade: 5     │       │ Prioridade: 4     │    │
│  │ Beacon periódico  │       │ Processa botões    │    │
│  │ Relay queue       │       │ Navega telas       │    │
│  └──────────────────┘       └──────────────────┘    │
│  ┌──────────────────┐                                │
│  │ task_gps          │                                │
│  │ Prioridade: 4     │                                │
│  │ Parse NMEA        │                                │
│  │ Atualiza posição  │                                │
│  └──────────────────┘                                │
│                                                      │
│  ┌──────────────────────────────────────────────┐    │
│  │          Shared State (mutex-protected)        │    │
│  │  - own_position (lat, lon, alt, speed, sats)  │    │
│  │  - node_table[MAX_NODES] (posições da rede)   │    │
│  │  - relay_queue (pacotes para retransmitir)     │    │
│  └──────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────┘
```

### 3.2 Estrutura do Projeto (PlatformIO + ESP-IDF)

```
meshtracker/
├── platformio.ini                ← Configuração central do projeto
├── sdkconfig.defaults            ← Overrides do menuconfig ESP-IDF
├── partitions.csv                ← Tabela de partições customizada
├── .gitignore
├── README.md
│
├── src/                          ← Código-fonte principal
│   ├── main.c                    ← Inicialização e criação de tasks
│   │
│   ├── drivers/
│   │   ├── display_ili9341.c/h   ← Driver SPI2 + DMA para display
│   │   ├── lora_sx1278.c/h       ← Driver SPI3 para LoRa RA-02
│   │   ├── gps_neo6m.c/h         ← Parser NMEA via UART1
│   │   └── buttons.c/h           ← Debounce + eventos de botão
│   │
│   ├── mesh/
│   │   ├── mesh_protocol.c/h     ← Protocolo mesh (pacotes, routing)
│   │   ├── mesh_relay.c/h        ← Lógica de retransmissão
│   │   ├── mesh_beacon.c/h       ← Beacon periódico (MAC + GPS)
│   │   └── node_table.c/h        ← Tabela de nós conhecidos
│   │
│   ├── ui/
│   │   ├── ui_engine.c/h         ← Motor de UI (baseado em LVGL)
│   │   ├── ui_styles.c/h         ← Estilos globais (tema CSS-like)
│   │   ├── screens/
│   │   │   ├── scr_splash.c/h    ← Tela de boot
│   │   │   ├── scr_map.c/h       ← Mapa com nós
│   │   │   ├── scr_nodes.c/h     ← Lista de nós na rede
│   │   │   ├── scr_gps.c/h       ← Info GPS detalhada
│   │   │   └── scr_settings.c/h  ← Configurações
│   │   └── components/
│   │       ├── comp_statusbar.c/h ← Barra de status (GPS, LoRa, bat)
│   │       ├── comp_node_card.c/h ← Card de um nó na lista
│   │       └── comp_compass.c/h   ← Bússola apontando para nó
│   │
│   └── utils/
│       ├── nmea_parser.c/h       ← Parser NMEA otimizado
│       ├── geo_math.c/h          ← Haversine, bearing, etc.
│       └── mac_utils.c/h         ← Funções MAC address
│
├── include/                      ← Headers públicos / config global
│   └── meshtracker_config.h      ← Defines de pinos, constantes
│
├── components/                   ← Componentes ESP-IDF locais
│   └── lvgl/                     ← LVGL como componente managed
│
├── lib/                          ← Bibliotecas locais (PlatformIO)
│   └── README.md
│
├── data/                         ← Arquivos SPIFFS (fontes, ícones)
│   └── fonts/
│
└── test/                         ← Testes unitários
    ├── test_nmea_parser.c
    ├── test_mesh_protocol.c
    └── test_geo_math.c
```

---

## 4. Protocolo Mesh LoRa

### 4.1 Formato do Pacote

Cada pacote LoRa transmitido tem este formato:

```
┌────────┬────────┬─────┬────────┬──────┬──────────────┬─────┐
│ MAGIC  │ SRC_ID │ HOP │ PKT_ID │ TYPE │   PAYLOAD    │ CRC │
│ 2B     │ 6B     │ 1B  │ 2B     │ 1B   │ 0-200B       │ 2B  │
└────────┴────────┴─────┴────────┴──────┴──────────────┴─────┘
 Total máximo: 214 bytes (dentro do limite LoRa de 255B)
```

| Campo | Bytes | Descrição |
|-------|-------|-----------|
| MAGIC | 2 | `0x4D54` ("MT" — MeshTracker), identifica pacotes da rede |
| SRC_ID | 6 | MAC address do nó de ORIGEM (não muda nos relays) |
| HOP | 1 | Contador de hops (incrementa a cada relay, max 5) |
| PKT_ID | 2 | ID sequencial do pacote (detecção de duplicatas) |
| TYPE | 1 | Tipo do pacote (ver tabela abaixo) |
| PAYLOAD | 0-200 | Dados (depende do TYPE) |
| CRC | 2 | CRC16-CCITT do pacote inteiro |

### 4.2 Tipos de Pacote

| TYPE | Nome | Payload | Intervalo |
|------|------|---------|-----------|
| 0x01 | BEACON | GPS position (20B) | A cada 10s |
| 0x02 | BEACON_ACK | Nenhum | Resposta a beacon |
| 0x03 | MESSAGE | Texto UTF-8 (até 200B) | Sob demanda |
| 0x04 | PING | Nenhum | Sob demanda |
| 0x05 | PONG | RSSI + SNR (4B) | Resposta a ping |
| 0xFF | EMERGENCY | GPS + flag (21B) | A cada 5s |

### 4.3 Payload do BEACON (0x01) — 20 bytes

```c
typedef struct __attribute__((packed)) {
    int32_t  latitude;     // graus × 10^7 (ex: -155234567 = -15.5234567°)
    int32_t  longitude;    // graus × 10^7
    int16_t  altitude;     // metros (–32768 a +32767)
    uint16_t speed;        // km/h × 10 (ex: 523 = 52.3 km/h)
    uint16_t course;       // graus × 10 (ex: 1835 = 183.5°)
    uint8_t  satellites;   // número de satélites
    uint8_t  fix_quality;  // 0=sem fix, 1=GPS, 2=DGPS
    uint8_t  battery;      // porcentagem (0-100)
    uint8_t  flags;        // bit0=has_display, bit1=relay_enabled
} beacon_payload_t;        // Total: 20 bytes
```

### 4.4 Algoritmo de Relay (Repetição)

```c
// Lógica executada quando um pacote é recebido

void on_packet_received(mesh_packet_t *pkt, int16_t rssi, float snr) {
    // 1. Validar
    if (pkt->magic != 0x4D54) return;           // Não é nosso protocolo
    if (crc16(pkt) != pkt->crc) return;          // CRC inválido
    if (memcmp(pkt->src_id, own_mac, 6) == 0)    // É meu próprio pacote
        return;

    // 2. Verificar duplicata (ring buffer de PKT_IDs recentes)
    if (is_duplicate(pkt->src_id, pkt->pkt_id))
        return;
    mark_as_seen(pkt->src_id, pkt->pkt_id);

    // 3. Processar (atualizar tabela de nós, etc.)
    process_packet(pkt, rssi, snr);

    // 4. Decidir se relaya
    if (pkt->hop >= MAX_HOPS) return;            // Máximo de hops atingido
    if (!relay_enabled) return;                    // Relay desabilitado

    // 5. Incrementar hop e colocar na fila de relay
    pkt->hop++;
    recalculate_crc(pkt);

    // Delay aleatório para evitar colisões
    // (vários nós recebem o mesmo pacote simultaneamente)
    uint32_t delay_ms = 100 + (esp_random() % 500);

    relay_queue_push(pkt, delay_ms);
}
```

### 4.5 Detecção de Duplicatas

```c
#define DUP_TABLE_SIZE 64

typedef struct {
    uint8_t  src_id[6];
    uint16_t pkt_id;
    uint32_t timestamp;    // tick quando recebido
} dup_entry_t;

static dup_entry_t dup_table[DUP_TABLE_SIZE];
static uint8_t dup_index = 0;

bool is_duplicate(const uint8_t *src, uint16_t pkt_id) {
    uint32_t now = xTaskGetTickCount();
    for (int i = 0; i < DUP_TABLE_SIZE; i++) {
        if (memcmp(dup_table[i].src_id, src, 6) == 0 &&
            dup_table[i].pkt_id == pkt_id &&
            (now - dup_table[i].timestamp) < pdMS_TO_TICKS(30000)) {
            return true;  // Visto nos últimos 30 segundos
        }
    }
    return false;
}

void mark_as_seen(const uint8_t *src, uint16_t pkt_id) {
    memcpy(dup_table[dup_index].src_id, src, 6);
    dup_table[dup_index].pkt_id = pkt_id;
    dup_table[dup_index].timestamp = xTaskGetTickCount();
    dup_index = (dup_index + 1) % DUP_TABLE_SIZE;
}
```

### 4.6 Configuração do LoRa SX1278

```c
// Parâmetros LoRa otimizados para mesh
#define LORA_FREQUENCY       433E6    // 433 MHz
#define LORA_BANDWIDTH       125E3    // 125 kHz (bom alcance)
#define LORA_SPREADING_FACTOR  9      // SF9 (compromisso alcance/velocidade)
#define LORA_CODING_RATE       5      // 4/5 (menor overhead FEC)
#define LORA_TX_POWER          17     // dBm (máx do RA-02 = 20dBm)
#define LORA_PREAMBLE_LEN      8      // Símbolos de preâmbulo
#define LORA_SYNC_WORD       0x34     // Separar de LoRaWAN (0x34 ≠ 0x12)
#define LORA_CRC_ENABLED     true

// Time-on-air estimado para pacote de 30 bytes com SF9/BW125:
// ~160 ms
// Beacon a cada 10s = 1.6% duty cycle (OK para regulação)

// Throughput efetivo com SF9/BW125:
// ~1758 bps ≈ 220 bytes/s
```

### 4.7 Tabela de Nós

```c
#define MAX_NODES 32
#define NODE_TIMEOUT_MS 120000  // 2 minutos sem beacon = offline

typedef struct {
    uint8_t  mac[6];           // Identificador único
    char     name[16];         // Nome legível (derivado do MAC)
    int32_t  latitude;
    int32_t  longitude;
    int16_t  altitude;
    uint16_t speed;
    uint8_t  satellites;
    uint8_t  battery;
    int16_t  rssi;             // Último RSSI recebido
    float    snr;              // Último SNR recebido
    uint8_t  hops;             // Quantos hops até este nó
    uint32_t last_seen;        // Timestamp do último beacon
    float    distance_km;      // Distância calculada (haversine)
    float    bearing;          // Direção em graus
    bool     active;           // Slot em uso
} mesh_node_t;

static mesh_node_t node_table[MAX_NODES];
static SemaphoreHandle_t node_table_mutex;

// Gerar nome legível a partir do MAC
// Ex: MAC AA:BB:CC:DD:EE:FF → "Node-EEFF"
void mac_to_name(const uint8_t *mac, char *name) {
    snprintf(name, 16, "Node-%02X%02X", mac[4], mac[5]);
}
```

---

## 5. GPS — Driver NEO-6M

### 5.1 Parsing NMEA

O NEO-6M envia sentenças NMEA a 9600 baud. As relevantes são:

```
$GPRMC — Recommended Minimum (posição, velocidade, curso, hora)
$GPGGA — Fix information (posição, altitude, satélites, qualidade)
$GPGSA — DOP e satélites ativos
$GPGSV — Satélites em vista (para visualização)
```

```c
// Estrutura de dados GPS
typedef struct {
    bool     valid;            // Fix válido?
    int32_t  latitude;         // graus × 10^7
    int32_t  longitude;        // graus × 10^7
    int16_t  altitude;         // metros
    uint16_t speed;            // km/h × 10
    uint16_t course;           // graus × 10
    uint8_t  satellites;       // Número de satélites
    uint8_t  fix_quality;      // 0=nenhum, 1=GPS, 2=DGPS
    uint8_t  hour, minute, second;
    uint8_t  day, month;
    uint16_t year;
    float    hdop;             // Horizontal dilution of precision
} gps_data_t;

static gps_data_t gps_current;
static SemaphoreHandle_t gps_mutex;

// Task GPS — Core 0
void task_gps(void *arg) {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_driver_install(UART_NUM_1, 1024, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, 17, 18, -1, -1);  // TX=17, RX=18

    char line[256];
    int pos = 0;

    while (1) {
        uint8_t byte;
        if (uart_read_bytes(UART_NUM_1, &byte, 1, pdMS_TO_TICKS(100)) > 0) {
            if (byte == '\n') {
                line[pos] = '\0';
                parse_nmea_line(line, &gps_current);
                pos = 0;
            } else if (pos < sizeof(line) - 1) {
                line[pos++] = byte;
            }
        }
    }
}
```

### 5.2 Cálculos Geográficos

```c
// Fórmula de Haversine — distância entre dois pontos GPS
double haversine_km(int32_t lat1_e7, int32_t lon1_e7,
                    int32_t lat2_e7, int32_t lon2_e7) {
    double lat1 = lat1_e7 * 1e-7 * M_PI / 180.0;
    double lat2 = lat2_e7 * 1e-7 * M_PI / 180.0;
    double dlat = (lat2_e7 - lat1_e7) * 1e-7 * M_PI / 180.0;
    double dlon = (lon2_e7 - lon1_e7) * 1e-7 * M_PI / 180.0;

    double a = sin(dlat/2) * sin(dlat/2) +
               cos(lat1) * cos(lat2) * sin(dlon/2) * sin(dlon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));

    return 6371.0 * c;  // Raio da Terra em km
}

// Bearing — direção de um ponto para outro
double bearing_deg(int32_t lat1_e7, int32_t lon1_e7,
                   int32_t lat2_e7, int32_t lon2_e7) {
    double lat1 = lat1_e7 * 1e-7 * M_PI / 180.0;
    double lat2 = lat2_e7 * 1e-7 * M_PI / 180.0;
    double dlon = (lon2_e7 - lon1_e7) * 1e-7 * M_PI / 180.0;

    double y = sin(dlon) * cos(lat2);
    double x = cos(lat1) * sin(lat2) -
               sin(lat1) * cos(lat2) * cos(dlon);

    double bearing = atan2(y, x) * 180.0 / M_PI;
    return fmod(bearing + 360.0, 360.0);
}
```

---

## 6. Framework de Interface — Abordagem CSS-like

### 6.1 Recomendação: LVGL com Estilização CSS-like

O **LVGL (Light and Versatile Graphics Library)** é a melhor escolha para este projeto. Ele já implementa nativamente um sistema de estilos inspirado em CSS, com propriedades como padding, margin, border-radius, opacity, gradientes e flexbox layout. Suporta o ILI9341 diretamente e roda muito bem no ESP32-S3.

**Por que LVGL e não algo custom:**

| Critério | LVGL | Framework custom |
|----------|------|------------------|
| Widgets prontos | 30+ (botão, label, lista, chart, etc.) | Precisaria criar tudo |
| Layout engine | Flexbox + Grid nativos | Meses de trabalho |
| Animações | Sistema integrado com timeline | Precisaria implementar |
| Estilos CSS-like | `lv_style_set_*()` = propriedades CSS | — |
| Anti-aliasing | Sim, incluindo fontes | Complexo |
| Comunidade | Enorme, ESP32+ILI9341 testado | Sem suporte |
| Performance | Dirty-rectangle rendering, DMA | Difícil otimizar |
| Memória | ~64KB Flash, ~8KB RAM mínimo | — |

### 6.2 LVGL: O Sistema de Estilos (CSS-like)

O LVGL usa um conceito de estilos que mapeia diretamente para CSS:

```c
// ══════════════════════════════════════════
// CSS equivalente:
// .card {
//   background-color: #1a1a2e;
//   border-radius: 8px;
//   padding: 12px;
//   border: 1px solid #16213e;
//   box-shadow: 0 2px 4px rgba(0,0,0,0.3);
// }
// ══════════════════════════════════════════

static lv_style_t style_card;

void init_style_card(void) {
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, lv_color_hex(0x1a1a2e));
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_radius(&style_card, 8);
    lv_style_set_pad_all(&style_card, 12);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_border_color(&style_card, lv_color_hex(0x16213e));
    lv_style_set_shadow_width(&style_card, 8);
    lv_style_set_shadow_ofs_y(&style_card, 2);
    lv_style_set_shadow_opa(&style_card, LV_OPA_30);
}

// Aplicar estilo a um objeto (como className em HTML)
lv_obj_t *card = lv_obj_create(parent);
lv_obj_add_style(card, &style_card, 0);
```

### 6.3 Mapeamento CSS → LVGL

| CSS Property | LVGL Equivalent |
|-------------|-----------------|
| `background-color` | `lv_style_set_bg_color()` |
| `color` | `lv_style_set_text_color()` |
| `font-size` | `lv_style_set_text_font()` + fonte compilada |
| `padding` | `lv_style_set_pad_all()` / `pad_top`, `pad_left`... |
| `margin` | Usar `lv_obj_set_style_margin_*()` |
| `border-radius` | `lv_style_set_radius()` |
| `border` | `lv_style_set_border_width()` + `border_color()` |
| `box-shadow` | `lv_style_set_shadow_*()` |
| `opacity` | `lv_style_set_opa()` (0-255) |
| `display: flex` | `lv_obj_set_flex_flow()` |
| `flex-direction: column` | `LV_FLEX_FLOW_COLUMN` |
| `gap` | `lv_obj_set_flex_gap()` ou `lv_style_set_pad_row/column()` |
| `align-items: center` | `lv_obj_set_flex_align(..., LV_FLEX_ALIGN_CENTER, ...)` |
| `width` / `height` | `lv_obj_set_size()` |
| `overflow: hidden` | `lv_obj_add_flag(obj, LV_OBJ_FLAG_OVERFLOW_HIDDEN)` |
| `:hover` / `:pressed` | `lv_obj_add_style(obj, &style, LV_STATE_PRESSED)` |
| `transition` | `lv_style_set_transition()` |
| `animation` | `lv_anim_t` API |

### 6.4 Proposta: Camada Declarativa sobre LVGL

Para aproximar mais de HTML/CSS/JS, podemos criar uma **camada de abstração** que permite definir interfaces de forma declarativa em C:

```c
// ══════════════════════════════════════════
// "HTML/CSS-like" declarativo sobre LVGL
// ══════════════════════════════════════════

// Equivalente conceptual ao HTML:
// <div class="screen dark">
//   <div class="statusbar">
//     <span class="icon">📡</span>
//     <span class="text-sm">4 sats</span>
//     <span class="text-sm right">Node-A3F2</span>
//   </div>
//   <div class="card">
//     <h2>Posição GPS</h2>
//     <p>Lat: -15.7939°</p>
//     <p>Lon: -47.8828°</p>
//   </div>
//   <div class="node-list">
//     <div class="node-card" onclick="show_node_detail">
//       <span class="node-name">Node-B1C4</span>
//       <span class="node-dist">2.3 km</span>
//     </div>
//   </div>
// </div>

// Em C com nossa camada declarativa:

void create_main_screen(void) {
    // Container principal (como <body>)
    lv_obj_t *scr = lv_scr_act();
    lv_obj_add_style(scr, &theme.bg_dark, 0);

    // ─── Statusbar (como <header>) ───
    lv_obj_t *statusbar = mt_statusbar_create(scr);

    // ─── Card GPS (como <section class="card">) ───
    lv_obj_t *gps_card = mt_card_create(scr, "Posição GPS");
    lv_obj_set_width(gps_card, lv_pct(100));

    lv_obj_t *lat_label = mt_kv_label(gps_card, "Lat:", "--.------°");
    lv_obj_t *lon_label = mt_kv_label(gps_card, "Lon:", "--.------°");
    lv_obj_t *alt_label = mt_kv_label(gps_card, "Alt:", "--- m");

    // ─── Lista de nós (como <ul class="node-list">) ───
    lv_obj_t *node_list = lv_list_create(scr);
    lv_obj_set_flex_grow(node_list, 1);  // flex: 1 (preenche espaço)
    lv_obj_add_style(node_list, &theme.list, 0);

    // Guardar refs para atualização dinâmica
    ui_refs.lat_label = lat_label;
    ui_refs.lon_label = lon_label;
    ui_refs.alt_label = alt_label;
    ui_refs.node_list = node_list;
}

// ═══ Helpers (componentes reutilizáveis) ═══

// Card: container com título, borda, padding
lv_obj_t *mt_card_create(lv_obj_t *parent, const char *title) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_add_style(card, &theme.card, 0);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_height(card, LV_SIZE_CONTENT);

    if (title) {
        lv_obj_t *lbl = lv_label_create(card);
        lv_label_set_text(lbl, title);
        lv_obj_add_style(lbl, &theme.heading, 0);
    }
    return card;
}

// Key-Value label: "Lat: -15.7939°"
lv_obj_t *mt_kv_label(lv_obj_t *parent, const char *key, const char *val) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_style(row, &theme.row_transparent, 0);

    lv_obj_t *k = lv_label_create(row);
    lv_label_set_text(k, key);
    lv_obj_add_style(k, &theme.text_dim, 0);

    lv_obj_t *v = lv_label_create(row);
    lv_label_set_text(v, val);
    lv_obj_add_style(v, &theme.text_bright, 0);

    return v;  // Retorna o label de valor (para atualizar depois)
}
```

### 6.5 Sistema de Temas (CSS Global)

```c
// ui_styles.c — Define o tema completo (como um arquivo CSS)

typedef struct {
    // Backgrounds
    lv_style_t bg_dark;        // background-color: #0f0f23
    lv_style_t bg_card;        // background-color: #1a1a2e

    // Cards
    lv_style_t card;           // Combinação de bg, border, padding, radius
    lv_style_t card_highlight; // Card selecionado

    // Typography
    lv_style_t heading;        // font-size: 16px, bold, color: #e0e0e0
    lv_style_t text_normal;    // font-size: 12px, color: #b0b0b0
    lv_style_t text_dim;       // font-size: 11px, color: #666
    lv_style_t text_bright;    // font-size: 12px, color: #fff
    lv_style_t text_accent;    // font-size: 12px, color: #00ff88

    // Layout
    lv_style_t row_transparent;// flex-row, sem background
    lv_style_t list;           // Estilo para listas scrolláveis

    // Status indicators
    lv_style_t indicator_ok;   // Círculo verde
    lv_style_t indicator_warn; // Círculo amarelo
    lv_style_t indicator_err;  // Círculo vermelho

    // Statusbar
    lv_style_t statusbar;      // Barra no topo

    // Buttons
    lv_style_t btn_primary;
    lv_style_t btn_secondary;
} mt_theme_t;

extern mt_theme_t theme;

void mt_theme_init(void) {
    // ─── Dark background ───
    lv_style_init(&theme.bg_dark);
    lv_style_set_bg_color(&theme.bg_dark, lv_color_hex(0x0f0f23));
    lv_style_set_bg_opa(&theme.bg_dark, LV_OPA_COVER);
    lv_style_set_text_color(&theme.bg_dark, lv_color_hex(0xe0e0e0));

    // ─── Card ───
    lv_style_init(&theme.card);
    lv_style_set_bg_color(&theme.card, lv_color_hex(0x1a1a2e));
    lv_style_set_bg_opa(&theme.card, LV_OPA_COVER);
    lv_style_set_radius(&theme.card, 6);
    lv_style_set_pad_all(&theme.card, 8);
    lv_style_set_pad_row(&theme.card, 4);
    lv_style_set_border_width(&theme.card, 1);
    lv_style_set_border_color(&theme.card, lv_color_hex(0x2a2a4e));
    lv_style_set_border_opa(&theme.card, LV_OPA_60);

    // ─── Heading ───
    lv_style_init(&theme.heading);
    lv_style_set_text_font(&theme.heading, &lv_font_montserrat_14);
    lv_style_set_text_color(&theme.heading, lv_color_hex(0x00ff88));
    lv_style_set_pad_bottom(&theme.heading, 4);

    // ─── Text dim (como labels cinza) ───
    lv_style_init(&theme.text_dim);
    lv_style_set_text_font(&theme.text_dim, &lv_font_montserrat_12);
    lv_style_set_text_color(&theme.text_dim, lv_color_hex(0x666680));

    // ─── Text bright (valores) ───
    lv_style_init(&theme.text_bright);
    lv_style_set_text_font(&theme.text_bright, &lv_font_montserrat_12);
    lv_style_set_text_color(&theme.text_bright, lv_color_hex(0xffffff));

    // ─── Accent (valores destacados) ───
    lv_style_init(&theme.text_accent);
    lv_style_set_text_font(&theme.text_accent, &lv_font_montserrat_14);
    lv_style_set_text_color(&theme.text_accent, lv_color_hex(0x00ff88));

    // ─── Row transparente ───
    lv_style_init(&theme.row_transparent);
    lv_style_set_bg_opa(&theme.row_transparent, LV_OPA_TRANSP);
    lv_style_set_border_width(&theme.row_transparent, 0);
    lv_style_set_pad_all(&theme.row_transparent, 0);

    // ─── Statusbar ───
    lv_style_init(&theme.statusbar);
    lv_style_set_bg_color(&theme.statusbar, lv_color_hex(0x16213e));
    lv_style_set_bg_opa(&theme.statusbar, LV_OPA_COVER);
    lv_style_set_pad_hor(&theme.statusbar, 6);
    lv_style_set_pad_ver(&theme.statusbar, 3);
    lv_style_set_text_font(&theme.statusbar, &lv_font_montserrat_10);
    lv_style_set_text_color(&theme.statusbar, lv_color_hex(0xaaaacc));
}
```

### 6.6 Alternativa Script: MicroPython + LVGL

Se quiser uma experiência ainda mais próxima de script (HTML/CSS/JS), existe **MicroPython + LVGL**. A binding permite escrever Python ao invés de C, com iteração instantânea (Change → Run, sem compilar):

```python
# main.py — MicroPython + LVGL no ESP32-S3

import lvgl as lv
from ili9341 import ili9341

# Inicializar display
disp = ili9341(mosi=11, miso=13, clk=12, cs=10, dc=9, rst=14,
               spihost=2, mhz=40)

# ─── Tema (equivalente ao CSS) ───
style_card = lv.style_t()
style_card.init()
style_card.set_bg_color(lv.color_hex(0x1a1a2e))
style_card.set_radius(8)
style_card.set_pad_all(12)
style_card.set_border_width(1)
style_card.set_border_color(lv.color_hex(0x2a2a4e))

style_title = lv.style_t()
style_title.init()
style_title.set_text_color(lv.color_hex(0x00ff88))
style_title.set_text_font(lv.font_montserrat_14)

# ─── Tela (equivalente ao HTML) ───
scr = lv.scr_act()
scr.set_style_bg_color(lv.color_hex(0x0f0f23), 0)

# Card GPS
card = lv.obj(scr)
card.add_style(style_card, 0)
card.set_size(300, 120)
card.align(lv.ALIGN.TOP_MID, 0, 30)
card.set_flex_flow(lv.FLEX_FLOW.COLUMN)

title = lv.label(card)
title.set_text("Posição GPS")
title.add_style(style_title, 0)

lat_lbl = lv.label(card)
lat_lbl.set_text("Lat: -15.7939°")

lon_lbl = lv.label(card)
lon_lbl.set_text("Lon: -47.8828°")

# ─── Atualizar dados (equivalente ao JS) ───
def update_gps():
    # Ler GPS e atualizar labels
    lat_lbl.set_text(f"Lat: {gps.latitude:.4f}°")
    lon_lbl.set_text(f"Lon: {gps.longitude:.4f}°")

# Timer para atualização (como setInterval em JS)
timer = lv.timer_create(lambda t: update_gps(), 1000, None)
```

**Trade-off MicroPython vs C:**

| Aspecto | C (ESP-IDF) | MicroPython |
|---------|-------------|-------------|
| Performance | Máxima | ~5-10× mais lento |
| Iteração | Compilar+flash (~30s) | Alterar+rodar (~1s) |
| RAM livre | ~300KB | ~150KB (interpretador ocupa) |
| Acesso HW | Total (registros, DMA) | Limitado a drivers |
| LoRa mesh | Controle total | Mais difícil otimizar |
| Recomendado | Produção final | Prototipação rápida |

**Recomendação:** Prototipe em MicroPython+LVGL para testar as telas e UX. Depois migre para C (ESP-IDF) + LVGL para a versão de produção com mesh LoRa otimizado.

---

## 7. Telas da Interface

### 7.1 Fluxo de Navegação

```
                    ┌─────────────┐
                    │   SPLASH    │  (2 segundos)
                    │  MeshTracker│
                    └──────┬──────┘
                           │ auto
                    ┌──────▼──────┐
              ┌─────│    MAPA     │─────┐
              │     │ (principal) │     │
              │     └──────┬──────┘     │
           BTN UP         BTN DOWN   LONG PRESS OK
              │               │         │
      ┌───────▼───────┐ ┌────▼──────┐ ┌▼────────────┐
      │   GPS INFO    │ │   NODES   │ │  SETTINGS   │
      │ Lat/Lon/Alt   │ │  Lista    │ │  LoRa config│
      │ Sats/HDOP     │ │  de nós   │ │  Beacon int.│
      │ Speed/Course  │ │           │ │  Display    │
      └───────────────┘ └─────┬─────┘ └─────────────┘
                              │ BTN OK
                        ┌─────▼─────┐
                        │NODE DETAIL│
                        │ Posição   │
                        │ Distância │
                        │ Bússola → │
                        │ RSSI/SNR  │
                        └───────────┘
```

### 7.2 Layout da Tela Principal (Mapa)

```
┌──────────────────────────────────────┐  320px
│ ⚡ 4⛰ 3 nós  RSSI:-67   Node-A3F2  │  ← Statusbar (20px)
├──────────────────────────────────────┤
│                                      │
│     ◇ Node-B1C4                      │
│      \   2.3km                       │
│       \                              │
│        ●──── Você                    │
│       /                              │
│      /   1.1km                       │
│     ◇ Node-C8D2                      │
│                                      │
│                                      │  ← Área do mapa (200px)
├──────────────────────────────────────┤
│  Lat:-15.7939  Lon:-47.8828  42m/s   │  ← Footer GPS (20px)
└──────────────────────────────────────┘
                                          240px
```

### 7.3 Layout da Tela de Nós

```
┌──────────────────────────────────────┐
│ ⚡ 4⛰ 3 nós  RSSI:-67   Node-A3F2  │
├──────────────────────────────────────┤
│ ┌──────────────────────────────────┐ │
│ │ ● Node-B1C4        2.3 km  ↗45° │ │
│ │   RSSI:-72 SNR:8.5  1 hop  32s  │ │
│ └──────────────────────────────────┘ │
│ ┌──────────────────────────────────┐ │
│ │ ● Node-C8D2        1.1 km  ↙210°│ │
│ │   RSSI:-85 SNR:4.2  2 hops 8s   │ │
│ └──────────────────────────────────┘ │
│ ┌──────────────────────────────────┐ │
│ │ ○ Node-D4E6        5.7 km  ↑350°│ │
│ │   RSSI:-102 SNR:1.1  3 hops 45s │ │
│ └──────────────────────────────────┘ │
│                                      │
│  ● ativo  ○ fraco  ◌ offline         │
└──────────────────────────────────────┘
```

---

## 8. Ambiente de Desenvolvimento

### 8.1 Escolha: PlatformIO + ESP-IDF

| Critério | Arduino IDE | PlatformIO + Arduino | **PlatformIO + ESP-IDF** | ESP-IDF puro (CMake) |
|----------|------------|---------------------|-------------------------|---------------------|
| Setup inicial | Simples | Médio | **Médio** | Complexo |
| Autocomplete/IDE | Limitado | VS Code completo | **VS Code completo** | VS Code (com plugin) |
| Compilação | Lenta, sem cache | Incremental | **Incremental** | Incremental |
| Controle de hardware | Abstrato | Médio | **Total** | Total |
| DMA/SPI avançado | Difícil | Limitado | **Nativo** | Nativo |
| FreeRTOS multi-core | Básico | Básico | **Total (pinning, etc)** | Total |
| Gerência de libs | Manual | Automática | **Automática** | Componentes manuais |
| Versionamento config | Difícil | `platformio.ini` | **`platformio.ini`** | `sdkconfig` + CMake |
| Debug (JTAG) | Não | Sim | **Sim** | Sim |
| Upload OTA | Limitado | Sim | **Sim** | Sim |

**Por que PlatformIO + ESP-IDF para este projeto:**

O MeshTracker tem complexidade real — dois barramentos SPI rodando simultaneamente (display com DMA a 40 MHz + LoRa com interrupções), UART para GPS, FreeRTOS com tasks pinadas em cores separados, LVGL, e protocolo mesh customizado. O Arduino IDE funciona para testes rápidos, mas não expõe o controle necessário sobre DMA, core affinity, e configuração avançada do SPI. O ESP-IDF dá acesso total, e o PlatformIO resolve o setup inicial doloroso do ESP-IDF puro.

Na prática:
- Bibliotecas Arduino como `LoRa.h` e `TFT_eSPI` fazem testes rápidos mas abstraem demais — quando precisar de DMA no display em paralelo com LoRa recebendo via interrupção, esbarram em limitações.
- PlatformIO permite misturar: usar ESP-IDF como framework base e puxar componentes Arduino (como RadioLib) quando fizer sentido.
- VS Code + PlatformIO dá autocomplete real das APIs do ESP-IDF, navegação por definições, e compilação incremental (~5s para mudanças pequenas vs ~30s full build).

### 8.2 Instalação e Setup

```bash
# 1. Instalar VS Code
# https://code.visualstudio.com/

# 2. Instalar extensão PlatformIO no VS Code
# Extensions → buscar "PlatformIO IDE" → Install

# 3. Criar projeto (via terminal do PlatformIO ou GUI)
mkdir meshtracker && cd meshtracker
pio init --board esp32-s3-devkitc-1 --project-option "framework=espidf"

# 4. Verificar instalação
pio run              # Compila
pio run -t upload    # Compila + flash
pio device monitor   # Monitor serial (115200 baud)

# Atalhos úteis no VS Code:
# Ctrl+Alt+B  → Build
# Ctrl+Alt+U  → Upload
# Ctrl+Alt+S  → Monitor Serial
```

### 8.3 platformio.ini

```ini
; ══════════════════════════════════════════════════════
; MeshTracker — PlatformIO Configuration
; ══════════════════════════════════════════════════════

[env:meshtracker]
platform = espressif32
board = esp32-s3-devkitc-1
framework = espidf

; ─── Hardware do N16R8 ───
board_build.flash_size = 16MB
board_build.partitions = partitions.csv
board_upload.flash_size = 16MB

; ─── PSRAM Octal SPI (N16R8) ───
board_build.arduino.memory_type = qio_opi
build_flags =
    -DBOARD_HAS_PSRAM

; ─── Monitor Serial ───
monitor_speed = 115200
monitor_filters = esp32_exception_decoder

; ─── Upload ───
upload_speed = 921600
; upload_port = /dev/ttyUSB0       ; Descomentar se necessário

; ─── Dependências ───
; RadioLib: Suporta SX1278 (RA-02) com API limpa
; Funciona com ESP-IDF via wrapper
lib_deps =
    jgromes/RadioLib@^7.1.0

; ─── Componentes ESP-IDF (managed components) ───
; Adicionar no idf_component.yml:
;   lvgl/lvgl: "~9.2"
;   espressif/esp_lcd_ili9341: "*"

; ─── Ambiente opcional para testes rápidos com Arduino ───
; Descomentar para usar Arduino como framework alternativo
; [env:meshtracker-arduino]
; platform = espressif32
; board = esp32-s3-devkitc-1
; framework = arduino
; lib_deps =
;     jgromes/RadioLib@^7.1.0
;     bodmer/TFT_eSPI@^2.5.0
;     mikalhart/TinyGPSPlus@^1.1.0
;     lvgl/lvgl@^9.2.0
; build_flags =
;     -DUSER_SETUP_LOADED
;     -DILI9341_DRIVER
;     -DTFT_MOSI=11
;     -DTFT_SCLK=12
;     -DTFT_CS=10
;     -DTFT_DC=9
;     -DTFT_RST=14
;     -DSPI_FREQUENCY=40000000
```

### 8.4 idf_component.yml (Managed Components)

Criar em `main/idf_component.yml`:

```yaml
## Componentes gerenciados pelo ESP-IDF Component Manager
## PlatformIO baixa automaticamente na primeira compilação

dependencies:
  lvgl/lvgl: "~9.2"
  espressif/esp_lcd_ili9341: "*"

  ## Opcional: driver de LCD panel io já incluso no ESP-IDF
  ## espressif/esp_lcd_panel_io: "*"
```

### 8.5 meshtracker_config.h (Defines centrais)

Criar em `include/meshtracker_config.h`:

```c
#ifndef MESHTRACKER_CONFIG_H
#define MESHTRACKER_CONFIG_H

// ═══════════════════════════════════════
// Versão
// ═══════════════════════════════════════
#define MT_VERSION_MAJOR  0
#define MT_VERSION_MINOR  1
#define MT_VERSION_PATCH  0
#define MT_VERSION_STRING "0.1.0"

// ═══════════════════════════════════════
// Display ILI9341 — SPI2 (FSPI, IO MUX)
// ═══════════════════════════════════════
#define PIN_LCD_SCK       12    // FSPICLK (IO MUX direto)
#define PIN_LCD_MOSI      11    // FSPID   (IO MUX direto)
#define PIN_LCD_MISO      13    // FSPIQ   (IO MUX direto)
#define PIN_LCD_CS        10    // FSPICS0 (IO MUX direto)
#define PIN_LCD_DC         9    // GPIO
#define PIN_LCD_RST       14    // GPIO
#define PIN_LCD_BL         8    // LEDC PWM ch0
#define LCD_SPI_HOST      SPI2_HOST
#define LCD_SPI_FREQ_HZ   (40 * 1000 * 1000)   // 40 MHz
#define LCD_WIDTH         320
#define LCD_HEIGHT        240

// ═══════════════════════════════════════
// LoRa RA-02 SX1278 — SPI3 (GPIO Matrix)
// ═══════════════════════════════════════
#define PIN_LORA_SCK      36
#define PIN_LORA_MOSI     35
#define PIN_LORA_MISO     37
#define PIN_LORA_CS       38
#define PIN_LORA_RST      39
#define PIN_LORA_DIO0     40    // IRQ — interrupção RX done
#define PIN_LORA_DIO1     41    // Timeout / CAD (opcional)
#define LORA_SPI_HOST     SPI3_HOST
#define LORA_SPI_FREQ_HZ  (10 * 1000 * 1000)   // 10 MHz
#define LORA_FREQUENCY    433.0   // MHz
#define LORA_BANDWIDTH    125.0   // kHz
#define LORA_SF           9       // Spreading Factor
#define LORA_CR           5       // Coding Rate 4/5
#define LORA_TX_POWER     17      // dBm (max RA-02 = 20)
#define LORA_SYNC_WORD    0x34    // Diferente de LoRaWAN (0x12)
#define LORA_PREAMBLE     8       // Símbolos

// ═══════════════════════════════════════
// GPS NEO-6M — UART1
// ═══════════════════════════════════════
#define PIN_GPS_TX        17    // ESP32 TX → GPS RX
#define PIN_GPS_RX        18    // GPS TX → ESP32 RX
#define PIN_GPS_PPS       16    // Pulse per second (opcional)
#define GPS_UART_NUM      UART_NUM_1
#define GPS_BAUD_RATE     9600

// ═══════════════════════════════════════
// Botões
// ═══════════════════════════════════════
#define PIN_BTN_UP         4
#define PIN_BTN_OK         5
#define PIN_BTN_DOWN       6
#define BTN_DEBOUNCE_MS   50
#define BTN_LONG_PRESS_MS 1000

// ═══════════════════════════════════════
// Mesh
// ═══════════════════════════════════════
#define MESH_MAGIC        0x4D54    // "MT"
#define MESH_MAX_HOPS     5
#define MESH_MAX_NODES    32
#define MESH_BEACON_INTERVAL_MS  10000  // 10 segundos
#define MESH_NODE_TIMEOUT_MS     120000 // 2 minutos
#define MESH_DUP_TABLE_SIZE      64
#define MESH_RELAY_ENABLED       true
#define MESH_RELAY_DELAY_MIN_MS  100
#define MESH_RELAY_DELAY_MAX_MS  500

// ═══════════════════════════════════════
// FreeRTOS Task Config
// ═══════════════════════════════════════
#define TASK_LORA_RX_STACK    4096
#define TASK_LORA_RX_PRIO     6
#define TASK_LORA_RX_CORE     0

#define TASK_LORA_TX_STACK    4096
#define TASK_LORA_TX_PRIO     5
#define TASK_LORA_TX_CORE     0

#define TASK_GPS_STACK        4096
#define TASK_GPS_PRIO         4
#define TASK_GPS_CORE         0

#define TASK_DISPLAY_STACK    8192
#define TASK_DISPLAY_PRIO     5
#define TASK_DISPLAY_CORE     1

#define TASK_UI_STACK         4096
#define TASK_UI_PRIO          4
#define TASK_UI_CORE          1

#endif // MESHTRACKER_CONFIG_H
```

### 8.6 sdkconfig.defaults

```ini
# CPU
CONFIG_ESP_DEFAULT_CPU_FREQ_240=y

# PSRAM (N16R8 = Octal PSRAM)
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y

# Flash (N16R8 = 16MB)
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y

# SPI — ISR em IRAM para menor latência
CONFIG_SPI_MASTER_ISR_IN_IRAM=y

# UART — ISR em IRAM para GPS
CONFIG_UART_ISR_IN_IRAM=y

# FreeRTOS
CONFIG_FREERTOS_HZ=1000
CONFIG_FREERTOS_UNICORE=n

# Cache (otimizado para PSRAM)
CONFIG_ESP32S3_DATA_CACHE_64KB=y
CONFIG_ESP32S3_DATA_CACHE_LINE_32B=y
CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB=y

# Log level (INFO em dev, WARNING em produção)
CONFIG_LOG_DEFAULT_LEVEL_INFO=y

# LVGL
CONFIG_LV_COLOR_DEPTH_16=y
CONFIG_LV_COLOR_16_SWAP=y
CONFIG_LV_MEM_SIZE_KILOBYTES=48
CONFIG_LV_USE_PERF_MONITOR=y
CONFIG_LV_FONT_MONTSERRAT_10=y
CONFIG_LV_FONT_MONTSERRAT_12=y
CONFIG_LV_FONT_MONTSERRAT_14=y
CONFIG_LV_FONT_MONTSERRAT_16=y
CONFIG_LV_FONT_MONTSERRAT_20=y
```

### 8.7 Partition Table

```csv
# Name,    Type, SubType, Offset,   Size,    Flags
nvs,       data, nvs,     ,         0x6000,
otadata,   data, ota,     ,         0x2000,
phy_init,  data, phy,     ,         0x1000,
factory,   app,  factory, ,         0x300000,
storage,   data, spiffs,  ,         0x100000,
coredump,  data, coredump,,         0x10000,
```

### 8.8 Comandos Úteis (PlatformIO CLI)

```bash
# ─── Build e Upload ───
pio run                          # Compilar
pio run -t upload                # Compilar + flash
pio run -t upload -t monitor     # Flash + abrir monitor serial

# ─── Monitor Serial ───
pio device monitor               # Monitor com decoder de exceções
pio device monitor -f esp32_exception_decoder

# ─── Limpar build ───
pio run -t clean                 # Limpa objetos compilados
pio run -t fullclean             # Limpa tudo (inclusive toolchain cache)

# ─── Menuconfig (ESP-IDF) ───
pio run -t menuconfig            # Abre o menuconfig do ESP-IDF
                                 # Útil para ajustar PSRAM, Wi-Fi, etc.

# ─── Debug (necessita JTAG) ───
pio debug                        # Inicia GDB debug session

# ─── Testes ───
pio test                         # Roda testes unitários

# ─── Verificar tamanho do firmware ───
pio run -t size                  # Mostra uso de Flash e RAM

# ─── Upload de filesystem SPIFFS ───
pio run -t uploadfs              # Envia conteúdo de data/ para SPIFFS

# ─── Atualizar dependências ───
pio pkg update                   # Atualiza libs e platform
```

### 8.9 Workflow de Desenvolvimento Recomendado

```
┌─────────────────────────────────────────────────────────┐
│                  Ciclo de Desenvolvimento                │
│                                                          │
│  1. Editar código no VS Code                            │
│     ↓                                                    │
│  2. Ctrl+Alt+B (Build) — ~5s incremental                │
│     ↓                                                    │
│  3. Ctrl+Alt+U (Upload) — ~8s via USB 921600 baud       │
│     ↓                                                    │
│  4. Ctrl+Alt+S (Monitor) — ver logs/debug               │
│     ↓                                                    │
│  5. Iterar                                               │
│                                                          │
│  Tempo total por iteração: ~15 segundos                  │
│  (vs ~45s com ESP-IDF puro, ~60s com Arduino IDE)        │
└─────────────────────────────────────────────────────────┘
```

---

## 9. Estimativas de Performance

### 9.1 Consumo de Energia

| Componente | Modo Ativo | Modo Sleep |
|-----------|-----------|-----------|
| ESP32-S3 (240MHz dual) | ~80 mA | ~8 µA (deep) |
| ILI9341 + backlight | ~40 mA | ~1 mA (display off) |
| LoRa RA-02 TX (17dBm) | ~120 mA (pico) | ~1 µA (sleep) |
| LoRa RA-02 RX | ~12 mA | — |
| NEO-6M GPS | ~45 mA | ~10 µA (backup) |
| **TOTAL (ativo)** | **~300 mA** | **~20 µA** |

Com bateria LiPo 2000mAh: **~6 horas** de uso contínuo.
Com beacon a cada 30s + display off entre usos: **~24+ horas**.

### 9.2 Alcance LoRa Estimado

| Cenário | SF | BW | Alcance Estimado |
|---------|----|----|------------------|
| Urbano denso | SF9 | 125kHz | 1-3 km |
| Suburbano | SF9 | 125kHz | 3-5 km |
| Campo aberto | SF9 | 125kHz | 5-10 km |
| Linha de visada | SF12 | 125kHz | 10-15+ km |

Com 3 nós em mesh (2 relays): alcance total **até 30 km** em campo aberto.

---

## 10. Roadmap

| Fase | Escopo | Prazo estimado |
|------|--------|---------------|
| **v0.1** | Hardware montado, display funcionando, GPS lendo | 2 semanas |
| **v0.2** | LoRa TX/RX básico, beacon com MAC+GPS | 2 semanas |
| **v0.3** | Protocolo mesh com relay e detecção de duplicatas | 2 semanas |
| **v0.4** | UI LVGL: tela mapa + lista de nós + statusbar | 2 semanas |
| **v0.5** | Otimizações: duty cycle, sleep, bateria | 2 semanas |
| **v1.0** | Testes de campo, correções, documentação final | 2 semanas |

---

## 11. Referências e Projetos Relacionados

**Ambiente de Desenvolvimento:**
- **PlatformIO** — IDE unificado para embarcados. Documentação: https://docs.platformio.org. Extensão VS Code: buscar "PlatformIO IDE".
- **ESP-IDF** — Framework oficial da Espressif. Documentação ESP32-S3: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/

**LoRa e Mesh:**
- **RadioLib** — Biblioteca multi-plataforma para SX1278/SX1276/SX1262. Suporta ESP-IDF e Arduino. Melhor opção para o RA-02. GitHub: https://github.com/jgromes/RadioLib
- **Meshtastic** — Firmware open-source para ESP32+LoRa que implementa mesh completo. Funciona com SX1262 (mais novo que SX1278). Boa referência de protocolo e arquitetura.
- **MeshCore** — Alternativa ao Meshtastic com roles de companion, repeater e room server. Usa path routing inteligente.
- **RadioHead** — Biblioteca Arduino com `RHMesh` para mesh networking sobre LoRa SX127x. Boa para prototipação rápida, mas limitada no ESP-IDF.

**Display e UI:**
- **LVGL** — Framework de UI com documentação extensa em https://docs.lvgl.io. Suporta ESP32+ILI9341 nativamente com estilos CSS-like, flexbox, animações.
- **lv_micropython** — Binding MicroPython para LVGL, permite prototipação rápida de UIs sem compilar.
- **esp_lcd** — Driver oficial Espressif para LCDs via SPI/I8080/RGB. Integra com LVGL diretamente.

**GPS:**
- **TinyGPSPlus** — Parser NMEA leve para Arduino. Referência para implementação custom em ESP-IDF.
- **u-blox NEO-6M datasheet** — Protocolo UBX para configuração avançada (mudar baud rate, polling rate, constelações).


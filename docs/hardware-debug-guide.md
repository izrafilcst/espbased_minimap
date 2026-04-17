# Guia de Debug de Hardware — MeshTracker

## O que esperar do hardware no primeiro boot

### Boot sequence esperada (serial 115200 baud)

```
I (xxx) lora_sx1278: === lora_init START ===
I (xxx) lora_sx1278: SX1278: CS=38 DIO0=40 RST=39 SCK=36 MOSI=35 MISO=37
I (xxx) lora_sx1278: === lora_init OK (433.000 MHz SF9 BW125kHz) ===
I (xxx) gps_neo6m:   === gps_init OK ===
I (xxx) buttons:     === buttons_init OK ===
I (xxx) node_table:  === node_table_init OK ===
I (xxx) mesh_relay:  === mesh_relay_init OK ===
I (xxx) mesh_beacon: SRC_ID (MAC): XX:XX:XX:XX:XX:XX
I (xxx) mesh_beacon: === mesh_beacon_init OK (intervalo 30000ms) ===
I (xxx) ui_engine:   === ui_engine_init OK ===
```

Após ~1.5 s: tela splash → tela MAP. A cada 30 s: log `Beacon TX PKT_ID=0x0001`.

Se alguma linha não aparecer, o subsistema correspondente falhou — o restante do boot continua, mas aquele módulo estará inativo.

### O que não vai funcionar de imediato

| Módulo | Motivo | Resolução |
|--------|--------|-----------|
| GPS (fix) | Requer sinal de satélite | Usar ao ar livre, aguardar 1–3 min |
| Display | Depende de wiring correto | Conferir pinos no CLAUDE.md |
| LoRa RX | Só detecta pacotes de outros nós | Ligar dois dispositivos |

---

## Ferramentas de debug disponíveis

### 1. Monitor serial com decoder de exceções

```bash
pio device monitor -e esp32-s3-devkitc-1
```

Configurado em `platformio.ini` com:
```ini
monitor_filters = esp32_exception_decoder, colorize
```

Traduz endereços de memória em nomes de funções e números de linha automaticamente. Resolve a maioria dos crashes sem precisar de JTAG.

### 2. Build debug — logs verbosos

```bash
pio run -e esp32-s3-devkitc-1-debug -t upload
pio device monitor -e esp32-s3-devkitc-1-debug
```

Liga `CORE_DEBUG_LEVEL=4`, habilitando todos os `ESP_LOGD` que ficam silenciosos no build de release. Útil para rastrear inicialização de SPI, GPIO e UART.

### 3. JTAG — breakpoints reais

O ESP32-S3-DevKitC tem JTAG embutido via USB (não precisa de adaptador externo).

**Via VS Code (extensão ESP-IDF):**
- Menu → *Run and Debug* → selecionar `ESP-IDF Debug`

**Via linha de comando:**
```bash
openocd -f board/esp32s3-builtin.cfg
```

Permite:
- Breakpoints em qualquer linha C/C++
- Inspeção de variáveis e registradores em tempo real
- Step-by-step em tasks FreeRTOS

### 4. Isolar módulos pelo log de boot

Cada `_init()` retorna `ESP_OK` ou loga um erro antes de retornar `ESP_FAIL`. Acompanhando o serial, você sabe exatamente até onde o boot progrediu e qual subsistema falhou, sem precisar de hardware adicional.

---

## Guru Meditation — o que é e como ler

Guru Meditation é o equivalente do "tela azul" no ESP32. O watchdog ou o processador detectaram um estado irrecuperável e o chip reinicia automaticamente.

### Exemplo de saída

```
Guru Meditation Error: Core  0 panic'ed (LoadProhibited)
PC: 0x40081234  EXCVADDR: 0x00000004

Backtrace:
0x40081234:0x3ffb1234 0x40082345:0x3ffb1250 ...
        |                    |
     endereço            stack pointer
```

O `esp32_exception_decoder` (já configurado no monitor) converte os endereços em:
```
0x40081234: lora_rx_task (lora_sx1278.cpp:99)
```

### Tipos de erro e causas comuns neste projeto

| Tipo de erro | Causa provável | Módulo suspeito |
|---|---|---|
| `LoadProhibited` | NULL pointer dereference | `s_radio` acessado antes de `spiBegin()` |
| `LoadProhibited` | Acesso a PSRAM não inicializada | `node_table` antes de `node_table_init()` |
| `StoreProhibited` | Escrita em endereço inválido | Buffer overflow em `mesh_protocol` |
| `Task watchdog` | Task bloqueando CPU por > 10 s | LVGL travado em `lv_timer_handler()` |
| `StackOverflow` | Stack de task insuficiente | `lora_rx_task` / `lvgl_task` |
| `assert failed` | Dupla inicialização de recurso | SPI bus inicializado duas vezes |

### Procedimento de diagnóstico

1. Copiar o backtrace completo do monitor serial
2. O decoder já resolve endereços → identificar função e linha
3. Se o decoder não resolver (build release), usar o build debug
4. Para crashes intermitentes ou em ISR: usar JTAG com breakpoint na função suspeita

---

## Checklist de validação de hardware

- [ ] Serial mostra todos os `_init OK` na sequência correta
- [ ] Display exibe tela splash e transiciona para MAP após 1.5 s
- [ ] Botões UP/DOWN navegam entre telas sem travar
- [ ] Log `Beacon TX` aparece a cada ~30 s
- [ ] GPS loga coordenadas após fix (ao ar livre)
- [ ] Com dois nós ligados: log `Beacon RX` aparece no nó receptor

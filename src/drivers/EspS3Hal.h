/**
 * @file EspS3Hal.h
 * @brief RadioLib HAL customizado para ESP32-S3 usando ESP-IDF SPI master.
 *
 * Substitui o EspHal.h do exemplo oficial do RadioLib (que só suporta
 * ESP32 original via acesso direto aos registradores SPI2).
 *
 * Esta implementação usa a API pública do ESP-IDF (driver/spi_master.h)
 * e é compatível com ESP32-S3.
 */

#ifndef ESP_S3_HAL_H
#define ESP_S3_HAL_H

#include <RadioLib.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Constantes estilo Arduino necessárias ao RadioLib */
#define LOW     0
#define HIGH    1
#define INPUT   (GPIO_MODE_INPUT)
#define OUTPUT  (GPIO_MODE_OUTPUT)
#ifndef RISING
  #define RISING  0x01
#endif
#ifndef FALLING
  #define FALLING 0x02
#endif

class EspS3Hal : public RadioLibHal {
public:
    /**
     * @param sck       SCK GPIO
     * @param miso      MISO GPIO
     * @param mosi      MOSI GPIO
     * @param cs        CS GPIO (gerenciado manualmente)
     * @param spiHost   Host SPI: SPI2_HOST ou SPI3_HOST
     * @param freqHz    Frequência SPI em Hz
     */
    EspS3Hal(int8_t sck, int8_t miso, int8_t mosi,
             int8_t cs, spi_host_device_t spiHost = SPI3_HOST,
             uint32_t freqHz = 10000000)
        : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
          _sck(sck), _miso(miso), _mosi(mosi), _cs(cs),
          _spiHost(spiHost), _freqHz(freqHz),
          _spiDev(NULL) {}

    void init() override { spiBegin(); }
    void term() override { spiEnd(); }

    /* ── GPIO ────────────────────────────────────────────────────────────── */
    void pinMode(uint32_t pin, uint32_t mode) override {
        if (pin == RADIOLIB_NC) return;
        gpio_config_t cfg = {};
        cfg.pin_bit_mask = 1ULL << pin;
        cfg.mode         = (gpio_mode_t)mode;
        cfg.pull_up_en   = GPIO_PULLUP_DISABLE;
        cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cfg.intr_type    = GPIO_INTR_DISABLE;
        gpio_config(&cfg);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        if (pin == RADIOLIB_NC) return;
        gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
        if (pin == RADIOLIB_NC) return 0;
        return (uint32_t)gpio_get_level((gpio_num_t)pin);
    }

    void attachInterrupt(uint32_t pin, void (*cb)(void), uint32_t mode) override {
        if (pin == RADIOLIB_NC) return;
        /* ISR service pode já ter sido instalado — toleramos ESP_ERR_INVALID_STATE */
        esp_err_t ret = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
        (void)ret;
        gpio_set_intr_type((gpio_num_t)pin, (gpio_int_type_t)(mode & 0x7));
        gpio_isr_handler_add((gpio_num_t)pin, (gpio_isr_t)(void *)cb, NULL);
    }

    void detachInterrupt(uint32_t pin) override {
        if (pin == RADIOLIB_NC) return;
        gpio_isr_handler_remove((gpio_num_t)pin);
        gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_DISABLE);
    }

    /* ── Tempo ───────────────────────────────────────────────────────────── */
    void delay(unsigned long ms) override {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void delayMicroseconds(unsigned long us) override {
        uint64_t end = (uint64_t)esp_timer_get_time() + us;
        while ((uint64_t)esp_timer_get_time() < end) { __asm__ volatile("nop"); }
    }

    unsigned long millis() override {
        return (unsigned long)(esp_timer_get_time() / 1000ULL);
    }

    unsigned long micros() override {
        return (unsigned long)esp_timer_get_time();
    }

    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override {
        if (pin == RADIOLIB_NC) return 0;
        this->pinMode(pin, INPUT);
        uint64_t start = (uint64_t)esp_timer_get_time();
        while ((uint32_t)gpio_get_level((gpio_num_t)pin) == state) {
            if ((uint64_t)esp_timer_get_time() - start > timeout) return 0;
        }
        return (long)((uint64_t)esp_timer_get_time() - start);
    }

    /* ── SPI ─────────────────────────────────────────────────────────────── */
    void spiBegin() {
        /* CS como GPIO de saída gerenciado manualmente */
        gpio_reset_pin((gpio_num_t)_cs);
        gpio_set_direction((gpio_num_t)_cs, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)_cs, 1);   /* CS HIGH (inativo) */

        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num   = _mosi;
        buscfg.miso_io_num   = _miso;
        buscfg.sclk_io_num   = _sck;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = 64;
        spi_bus_initialize(_spiHost, &buscfg, SPI_DMA_DISABLED);

        spi_device_interface_config_t devcfg = {};
        devcfg.clock_speed_hz = (int)_freqHz;
        devcfg.mode           = 0;       /* SX1278: CPOL=0 CPHA=0 */
        devcfg.spics_io_num   = -1;      /* CS manual */
        devcfg.queue_size     = 1;
        spi_bus_add_device(_spiHost, &devcfg, &_spiDev);
    }

    void spiBeginTransaction() {
        gpio_set_level((gpio_num_t)_cs, 0);   /* CS LOW */
    }

    void spiTransfer(uint8_t *out, size_t len, uint8_t *in) {
        if (!_spiDev || len == 0) return;
        spi_transaction_t t = {};
        t.length    = len * 8;
        t.tx_buffer = out;
        t.rx_buffer = in;
        spi_device_polling_transmit(_spiDev, &t);
    }

    void spiEndTransaction() {
        gpio_set_level((gpio_num_t)_cs, 1);   /* CS HIGH */
    }

    void spiEnd() {
        if (_spiDev) {
            spi_bus_remove_device(_spiDev);
            _spiDev = NULL;
        }
        spi_bus_free(_spiHost);
    }

private:
    int8_t           _sck, _miso, _mosi, _cs;
    spi_host_device_t _spiHost;
    uint32_t         _freqHz;
    spi_device_handle_t _spiDev;
};

#endif /* ESP_S3_HAL_H */

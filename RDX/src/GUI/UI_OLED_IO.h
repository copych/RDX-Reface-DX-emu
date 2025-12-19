#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

/* ============================================================
   CONFIGURATION (compile-time defaults, overrideable)
   ============================================================ */


#if (OLED_USE_I2C + OLED_USE_SPI) != 1
#error "Select exactly ONE: OLED_USE_I2C=1 or OLED_USE_SPI=1"
#endif

// ----------------------- I2C -----------------------
#ifndef OLED_I2C_ADDR
#define OLED_I2C_ADDR 0x3C
#endif

#ifndef OLED_I2C_PORT
#define OLED_I2C_PORT Wire
#endif

#ifndef OLED_I2C_SDA
#define OLED_I2C_SDA 9     // autodetect if -1
#endif

#ifndef OLED_I2C_SCL
#define OLED_I2C_SCL 8
#endif

// ----------------------- SPI -----------------------
#ifndef OLED_SPI_PORT
#define OLED_SPI_PORT SPI
#endif

#ifndef OLED_PIN_CS
#define OLED_PIN_CS 5
#endif
#ifndef OLED_PIN_DC
#define OLED_PIN_DC 4
#endif
#ifndef OLED_PIN_RST
#define OLED_PIN_RST 6   // -1 = unused
#endif

#ifndef OLED_SPI_SCK
#define OLED_SPI_SCK 8
#endif
#ifndef OLED_SPI_MOSI          // SDA
#define OLED_SPI_MOSI 9
#endif

#ifndef OLED_SPI_FREQ
#define OLED_SPI_FREQ 8000000
#endif



/* ============================================================
   UI_OLED_IO
   Low-level I/O driver for both I2C & SPI
   ============================================================ */

class UI_OLED_IO {
public:
    UI_OLED_IO() {}

#if OLED_USE_SPI
    // Allow user to override SPI pins at runtime
    inline void setSpiPins(int sck, int mosi, int dc, int cs, int rst = -1) {
        spi_sck_  = sck;
        spi_mosi_ = mosi;
        pin_dc_   = dc;
        pin_cs_   = cs;
        pin_rst_  = rst;
    }
#endif

    // --------------------------------------------------------
    // begin()
    // --------------------------------------------------------
    void begin() {
#if OLED_USE_I2C
        if (OLED_I2C_SDA >= 0 && OLED_I2C_SCL >= 0)
            OLED_I2C_PORT.begin(OLED_I2C_SDA, OLED_I2C_SCL);
        else
            OLED_I2C_PORT.begin();
#endif

#if OLED_USE_SPI
        // If user didn't override pins, use defaults
        if (spi_sck_ < 0) {
            spi_sck_  = OLED_SPI_SCK;
            spi_mosi_ = OLED_SPI_MOSI;
            pin_dc_   = OLED_PIN_DC;
            pin_cs_   = OLED_PIN_CS;
            pin_rst_  = OLED_PIN_RST;
        }

        pinMode(pin_dc_, OUTPUT);
        pinMode(pin_cs_, OUTPUT);
        digitalWrite(pin_cs_, HIGH);

        OLED_SPI_PORT.begin(spi_sck_, -1, spi_mosi_);

        if (pin_rst_ >= 0) {
            pinMode(pin_rst_, OUTPUT);
            digitalWrite(pin_rst_, HIGH);
            delay(1);
            digitalWrite(pin_rst_, LOW);
            delay(10);
            digitalWrite(pin_rst_, HIGH);
        }
#endif
    }

    // --------------------------------------------------------
    // Send a single command byte
    // --------------------------------------------------------
    inline void cmd(uint8_t c) {
#if OLED_USE_I2C
        sendI2C(0x00, &c, 1);
#else
        writeCommandSPI(c);
#endif
    }

    // --------------------------------------------------------
    // Send N command bytes
    // --------------------------------------------------------
    inline void cmd(const uint8_t* seq, size_t n) {
#if OLED_USE_I2C
        sendI2C(0x00, seq, n);
#else
        for (size_t i = 0; i < n; i++) writeCommandSPI(seq[i]);
#endif
    }

    // --------------------------------------------------------
    // Send N data bytes
    // --------------------------------------------------------
    inline void data(const uint8_t* d, size_t n) {
#if OLED_USE_I2C
        sendI2C(0x40, d, n);
#else
        writeDataSPI(d, n);
#endif
    }

private:

/* ============================================================
   I2C implementation
   ============================================================ */

#if OLED_USE_I2C
    static constexpr size_t I2C_CHUNK = 30;

    inline void sendI2C(uint8_t prefix, const uint8_t* p, size_t n) {
        while (n) {
            OLED_I2C_PORT.beginTransmission(OLED_I2C_ADDR);
            OLED_I2C_PORT.write(prefix);

            size_t chunk = (n > I2C_CHUNK ? I2C_CHUNK : n);
            OLED_I2C_PORT.write(p, chunk);

            OLED_I2C_PORT.endTransmission();

            p += chunk;
            n -= chunk;
        }
    }
#endif


/* ============================================================
   SPI implementation
   ============================================================ */

#if OLED_USE_SPI
    int spi_sck_  = -1;
    int spi_mosi_ = -1;
    int pin_dc_   = -1;
    int pin_cs_   = -1;
    int pin_rst_  = -1;

    inline void writeCommandSPI(uint8_t c) {
        digitalWrite(pin_dc_, LOW);
        digitalWrite(pin_cs_, LOW);
        OLED_SPI_PORT.beginTransaction(SPISettings(OLED_SPI_FREQ, MSBFIRST, SPI_MODE0));
        OLED_SPI_PORT.transfer(c);
        OLED_SPI_PORT.endTransaction();
        digitalWrite(pin_cs_, HIGH);
    }

    inline void writeDataSPI(const uint8_t* p, size_t n) {
        digitalWrite(pin_dc_, HIGH);
        digitalWrite(pin_cs_, LOW);
        OLED_SPI_PORT.beginTransaction(SPISettings(OLED_SPI_FREQ, MSBFIRST, SPI_MODE0));
        while (n--) OLED_SPI_PORT.transfer(*p++);
        OLED_SPI_PORT.endTransaction();
        digitalWrite(pin_cs_, HIGH);
    }
#endif
};


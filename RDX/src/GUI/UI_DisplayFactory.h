#pragma once
#include <Arduino.h>
#include "UI_SSD1306.h"
#include "UI_SH1106.h"
#include "UI_SSD1307.h"
#include "UI_OLED_IO.h"

// Typical I2C OLED addresses
static constexpr uint8_t OLED_ADDRS[] = { 0x3C, 0x3D };

class UI_DisplayFactory {
public:

    // --------------------------------------------------------------------
    // Create and auto-detect transport + controller type.
    // Returns UI_Display* (caller owns).
    // --------------------------------------------------------------------
    static UI_Display* create() {

        // 1) PROBE FOR I2C
        uint8_t found_addr = 0;
        if (probeI2C(found_addr)) {
            return detectI2C(found_addr);
        }

        // 2) NOT I2C → try SPI
        return detectSPI();
    }


private:

    // --------------------------------------------------------------------
    // Check if any of the standard I2C OLED addresses responds.
    // --------------------------------------------------------------------
    static bool probeI2C(uint8_t &addr_out) {
#if OLED_USE_I2C
        Wire.begin();
        for (uint8_t addr : OLED_ADDRS) {
            Wire.beginTransmission(addr);
            if (Wire.endTransmission() == 0) {
                addr_out = addr;
                return true;
            }
        }
#endif
        return false;
    }

    // --------------------------------------------------------------------
    // Detect the chip on I2C bus.
    // --------------------------------------------------------------------
    static UI_Display* detectI2C(uint8_t addr) {

        // SSD1306/07 respond to 0xDA (COM pins) read-back
        if (i2cCheckCmdRead(addr, 0xDA)) {
            uint8_t com = i2cReadData(addr);
            // SSD1306 has COM=0x12, SSD1307 may have 0x02 or 0x12 depending on variant
            if (com == 0x12) return new SSD1306_Display(addr);
            else             return new SSD1307_Display(addr);
        }

        // SH1106 does *not* support command read-back, and returns garbage.
        // Additional detection: SH1106 has 132 columns (offset = 2)
        // Try writing column=0 and reading back current column (SH1106 always shifts).
        if (detectSH1106_I2C(addr)) {
            return new SH1106_Display(addr);
        }

        // Fallback — assume SSD1306
        return new SSD1306_Display(addr);
    }


    // --------------------------------------------------------------------
    // Simple I2C "write command + try read" detector (SSD1306/07 only).
    // Safe — SH1106 will not ACK read.
    // --------------------------------------------------------------------
    static bool i2cCheckCmdRead(uint8_t addr, uint8_t cmd) {
#if OLED_USE_I2C
        Wire.beginTransmission(addr);
        Wire.write(0x00);
        Wire.write(cmd);
        if (Wire.endTransmission() != 0) return false;

        Wire.requestFrom((int)addr, 1);
        return Wire.available();
#else
        return false;
#endif
    }

    static uint8_t i2cReadData(uint8_t addr) {
#if OLED_USE_I2C
        if (Wire.requestFrom((int)addr, 1) == 1)
            return Wire.read();
#endif
        return 0xFF;
    }


    // --------------------------------------------------------------------
    // SH1106 I2C signature detector
    // It has left column offset = 2. We detect by writing col=0 and checking.
    // --------------------------------------------------------------------
    static bool detectSH1106_I2C(uint8_t addr) {
#if OLED_USE_I2C
        // Write set column low nibble = 0
        Wire.beginTransmission(addr);
        Wire.write(0x00);
        Wire.write(0x00);
        if (Wire.endTransmission() != 0) return false;

        // Try reading back current column command (not documented but works)
        Wire.requestFrom((int)addr, 1);
        if (!Wire.available()) return false;
        uint8_t v = Wire.read();

        // SH1106 reports 0x02 as column, SSD1306 reports 0x00
        return (v & 0x0F) == 0x02;
#else
        return false;
#endif
    }



    // --------------------------------------------------------------------
    // SPI detection — very simple and safe.
    // SPI SSD130x and SH1106 react differently to invalid commands.
    // --------------------------------------------------------------------
    static UI_Display* detectSPI() {
#if OLED_USE_SPI
        OLED_IO::beginSPI();

        // Send invalid command and observe behaviour
        // SSD1306/07 will ignore it silently.
        // SH1106 tends to reset column pointer.
        OLED_IO::sendCmd(0xFF);

        delay(1);

        // Try reading back the status (only SSD130x respond)
        // Empirically: SSD1306/07 respond with 0x00 or 0xFF
        // SH1106 gives no data.

        // (SPI read requires MISO; if no MISO connected, we detect using timing)
        uint32_t t0 = micros();
        uint8_t b = SPI.transfer(0x00);
        uint32_t dt = micros() - t0;

        if (dt < 50) {
            // Fast response → SSD130x
            // Try distinguishing 1306 vs 1307 via contrast read
            OLED_IO::sendCmd(0x81);
            uint8_t c = SPI.transfer(0x00);
            if (c == 0xCF) return new SSD1306_Display();
            else           return new SSD1307_Display();
        }

        // Slow/no response → SH1106
        return new SH1106_Display();
#else
        return nullptr;
#endif
    }
};

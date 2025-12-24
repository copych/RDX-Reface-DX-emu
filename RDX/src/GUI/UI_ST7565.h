#pragma once
#include <Arduino.h>
#include "UI_Display.h"
#include "UI_OLED_IO.h"

// ST7565_Display - generic 128x64 ST7565 driver skeleton
class ST7565_Display : public UI_Display {
public:
    ST7565_Display() {
        width_  = OLED_WIDTH;
        height_ = OLED_HEIGHT;
        buf_    = buffer_;
    }

    void begin() {
        io_.begin();
        resetIfNeeded();
        initDisplay();
        clear();
        update();
    }

    int getWidth()  const { return width_; }
    int getHeight() const { return height_; }

    void setFontByGliphSize(uint8_t, uint8_t) { /* no-op */ }

    void clear() {
        memset(buffer_, 0x00, sizeof(buffer_));
    }

    void clearRegion(int x, int y, int w, int h) {
        setColor(UIDisplayColor::BLACK);
        for (int yy = y; yy < y + h; ++yy) drawHLine(x, yy, w);
    }

    void update() {
        // ST7565 uses page addressing: pages = height/8
        uint8_t pages = height_ >> 3;
        size_t idx = 0;
        for (uint8_t p = 0; p < pages; ++p) {
            // set page
            io_.cmd(0xB0 | p);
            // set column address to 0 (lower + upper nibble)
            io_.cmd(0x10); // column MSB = 0
            io_.cmd(0x00); // column LSB = 0

            // send full width data for this page
            io_.data(&buffer_[idx], width_);
            idx += width_;
        }
    }

    void setPixel(int x, int y, bool c) {
        if ((unsigned)x >= width_ || (unsigned)y >= height_) return;

        // apply mirroring/offset here if your module needs it:
        // x = width_ - 1 - x;    // example: horizontal mirror
        // x += COL_OFFSET;       // example: column offset

        uint16_t pos = (y >> 3) * width_ + x;
        uint8_t bit = y & 7;
        if (c) buffer_[pos] |=  (1 << bit);
        else   buffer_[pos] &= ~(1 << bit);
    }

    bool getPixel(int x, int y) const {
        if ((unsigned)x >= width_ || (unsigned)y >= height_) return false;

        // same mirror/offset as setPixel if needed
        uint16_t pos = (y >> 3) * width_ + x;
        return (buffer_[pos] >> (y & 7)) & 1;
    }

private:
    UI_OLED_IO io_;
    uint8_t buffer_[(OLED_WIDTH * OLED_HEIGHT) / 8];

    void resetIfNeeded() {
        // if your UI_OLED_IO provides a reset pin, the io_.begin() already toggles it.
        // otherwise, you can toggle a separate RST pin here if needed.
    }

    void initDisplay() {
        // ---- WARNING ----
        // ST7565 init sequences vary between module variants.
        // Replace the sequence below with your module's recommended sequence from its datasheet.
        // The following is a common pattern (informational), may need adjustment.
        static const uint8_t init_seq[] = {
            0xAE, // DISPLAY OFF
            0xA2, // Set bias 1/9 (try 0xA3 for 1/7 if your module needs)
            0xA0, // ADC selection: normal
            0xC8, // COM output scan direction: reverse
            0x2F, // Power control: booster, regulator, follower on
            0x27, // Regulator resistor ratio (contrast baseline) - tune
            0x81, 0x10, // Electronic volume (contrast) - tune 0x00..0x3F
            0x40, // Set display start line = 0
            0xAF  // DISPLAY ON
        };
        io_.cmd(init_seq, sizeof(init_seq));
        delay(50); // allow LCD bias circuits to stabilise
    }
};

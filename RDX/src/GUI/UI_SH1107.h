#pragma once
#include <Arduino.h>
#include "UI_Display.h"
#include "UI_OLED_IO.h"

class SH1107_Display : public UI_Display {
public:
    SH1107_Display() {
        width_  = OLED_WIDTH;
        height_ = OLED_HEIGHT;
        buf_    = buffer_;
    }

    void begin() {
        io_.begin();
        initDisplay();
        clear();
        update();
    }

    inline int getWidth()  const override { return width_; }
    inline int getHeight() const override { return height_; }

    inline void clear() override {
        memset(buffer_, 0, sizeof(buffer_));
    }

    inline void update() override {
        uint8_t pages = height_ >> 3;
        size_t idx = 0;

        uint8_t column_offset = 32;

        for (uint8_t p = 0; p < pages; p++) {
            io_.cmd(0xB0 | p);
            io_.cmd(0x00 + (column_offset & 0x0F));      // lower nibble
            io_.cmd(0x10 + ((column_offset >> 4) & 0x0F)); // higher nibble
            io_.data(buf_ + idx, width_);
            idx += width_;
        }
    }

    inline void setPixel(int x, int y, bool c) override {
        if ((unsigned)x >= width_ || (unsigned)y >= height_) return;
        x = width_ - 1 - x;
        uint16_t pos = (y >> 3) * width_ + x;
        uint8_t bit  = (y & 7);
        if (c) buf_[pos] |=  (1 << bit);
        else   buf_[pos] &= ~(1 << bit);
    }

    inline bool getPixel(int x, int y) const override {
        if ((unsigned)x >= width_ || (unsigned)y >= height_) return false;
        x = width_ - 1 - x;
        uint16_t pos = (y >> 3) * width_ + x;
        return (buf_[pos] >> (y & 7)) & 1;
    }

private:
    UI_OLED_IO io_;
    uint8_t buffer_[(OLED_WIDTH * OLED_HEIGHT) / 8];

    void initDisplay() {
        static const uint8_t seq[] = {
            0xAE,
            0xD5, 0x51,
            0xC0,
            0xA1,
            0xA8, OLED_HEIGHT - 1,
            0xAD, 0x30,
            0x81, 0x80,
            0xD9, 0x22,
            0xDB, 0x35,
            0xA4,
            0xA6,
            0xAF
        };
        io_.cmd(seq, sizeof(seq));
    }
};

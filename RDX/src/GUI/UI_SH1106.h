#pragma once
#include <Arduino.h>
#include "UI_Display.h"
#include "UI_OLED_IO.h"

class SH1106_Display : public UI_Display {
public:
    SH1106_Display() {
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

        for (uint8_t p = 0; p < pages; p++) {
            io_.cmd(0xB0 | p);
            io_.cmd(0x02);
            io_.cmd(0x10);
            io_.data(buf_ + idx, width_);
            idx += width_;
        }
    }

    inline void setPixel(int x, int y, bool c) override {
        if ((unsigned)x >= width_ || (unsigned)y >= height_) return;
        uint16_t pos = (y >> 3) * width_ + x;
        uint8_t bit  = (y & 7);
        if (c) buf_[pos] |=  (1 << bit);
        else   buf_[pos] &= ~(1 << bit);
    }

    inline bool getPixel(int x, int y) const override {
        if ((unsigned)x >= width_ || (unsigned)y >= height_) return false;
        uint16_t pos = (y >> 3) * width_ + x;
        return (buf_[pos] >> (y & 7)) & 1;
    }

private:
    UI_OLED_IO io_;
    uint8_t buffer_[(OLED_WIDTH * OLED_HEIGHT) / 8];

    void initDisplay() {
        static const uint8_t seq[] = {
            0xAE, 0xD5, 0x80,
            0xA8, OLED_HEIGHT - 1,
            0xD3, 0x00,
            0x40,
            0xAD, 0x8B,
            0xA1, 0xC8,
            0xDA, 0x12,
            0x81, 0x7F,
            0xD9, 0x22,
            0xDB, 0x40,
            0xA4, 0xA6,
            0xAF
        };
        io_.cmd(seq, sizeof(seq));
    }
};

#pragma once
#include <Arduino.h>
#include "font_mo.h"

enum class UIDisplayBrush : uint8_t {
    SOLID = 0,
    DOTTED,
    DASHED
};

enum class UIDisplayColor : uint8_t {
    BLACK = 0,
    WHITE = 1,
    INVERT = 2  // For displays that support it
};


enum class UITextScale : uint8_t {
    X1 = 1,
    X2 = 2
};


inline const char* toText(int v) {
    static char buf[16];
    snprintf(buf, sizeof(buf), "%d", v);
    return buf;
}

// --- Simple 5x7 font for text rendering ---
constexpr uint8_t FONT_WIDTH = 5;
constexpr uint8_t FONT_HEIGHT = 7;

class UI_Display {
public:
    virtual ~UI_Display() {}

inline void setTextScale(UITextScale s) { textScale_ = s; }

    // ---------------- Hardware-dependent methods ----------------
    virtual void setPixel(int x, int y, bool color) = 0;
    virtual bool getPixel(int x, int y) const = 0;
    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
    virtual void update() = 0;
    virtual void clear() = 0; 

    // ---------------- Brush & Color ----------------
    inline void setBrush(UIDisplayBrush b) { brush_ = b; }
    inline UIDisplayBrush getBrush() const { return brush_; }

    inline void setColor(UIDisplayColor c) { color_ = c; }
    inline UIDisplayColor getColor() const { return color_; }
 

    int getFontHeight() { return FONT_HEIGHT * (int)textScale_; };
    int getFontWidth() { return FONT_WIDTH; };
    int getTextWidth(const char* txt)  { return (strlen(txt) * (FONT_WIDTH+1) -1) * (int)textScale_ ; }

protected:
		UITextScale textScale_ = UITextScale::X1; 
    uint8_t width_ = 0;
    uint8_t height_ = 0;
    uint8_t* buf_ = nullptr;
    inline bool colorSolid() const { return color_ == UIDisplayColor::WHITE; }
    inline bool colorClear() const { return color_ == UIDisplayColor::BLACK; }
    inline bool colorInvert() const { return color_ == UIDisplayColor::INVERT; }

    // Brush mask for lines/circles
    inline bool brushMask(int idx) const {
        switch (brush_) {
            case UIDisplayBrush::SOLID:  return true;
            case UIDisplayBrush::DOTTED: return (idx & 1) == 0;  // 1px on / 1px off
            case UIDisplayBrush::DASHED: return (idx & 3) < 2;   // 2px on / 2px off
        }
        return true;
    }

    UIDisplayBrush brush_ = UIDisplayBrush::SOLID;
    UIDisplayColor  color_ = UIDisplayColor::WHITE;

    // ---------------- Primitive drawing ----------------
public:
    inline void drawHLine(int x, int y, int w) {
        if (w <= 0 || y < 0 || y >= getHeight()) return;
        int start = x, end = x + w - 1;
        if (end < 0 || start >= getWidth()) return;
        if (start < 0) start = 0;
        if (end >= getWidth()) end = getWidth() - 1;

        for (int i = start, idx = 0; i <= end; ++i, ++idx) {
            if (!brushMask(idx)) continue;
            if (colorInvert())      setPixel(i, y, !getPixel(i,y));
            else if (colorSolid())  setPixel(i, y, true);
            else                    setPixel(i, y, false);
        }
    }

    inline void drawVLine(int x, int y, int h) {
        if (h <= 0 || x < 0 || x >= getWidth()) return;
        int start = y, end = y + h - 1;
        if (end < 0 || start >= getHeight()) return;
        if (start < 0) start = 0;
        if (end >= getHeight()) end = getHeight() - 1;

        for (int i = start, idx = 0; i <= end; ++i, ++idx) {
            if (!brushMask(idx)) continue;
            if (colorInvert())      setPixel(x, i, !getPixel(x,i));
            else if (colorSolid())  setPixel(x, i, true);
            else                    setPixel(x, i, false);
        }
    }

    inline void drawLine(int x0, int y0, int x1, int y1) {
        if (x0 == x1) { drawVLine(x0, min(y0,y1), abs(y1-y0)+1); return; }
        if (y0 == y1) { drawHLine(min(x0,x1), y0, abs(x1-x0)+1); return; }

        int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
        int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        int idx = 0;

        while (true) {
            if (brushMask(idx)) {
                if (colorInvert())      setPixel(x0, y0, !getPixel(x0,y0));
                else if (colorSolid())  setPixel(x0, y0, true);
                else                    setPixel(x0, y0, false);
            }
            if (x0 == x1 && y0 == y1) break;
            int e2 = err << 1;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
            idx++;
        }
    }

    inline void drawRect(int x, int y, int w, int h) {
        drawHLine(x, y, w);
        drawHLine(x, y+h-1, w);
        drawVLine(x, y, h);
        drawVLine(x+w-1, y, h);
    }

    inline void fillRect(int x, int y, int w, int h) {
        for (int yy = y; yy < y+h; ++yy) drawHLine(x, yy, w);
    }

    inline void drawCircle(int x0, int y0, int di) {
        int r = di / 2;
        int even = 1 - (di - r * 2); // 1 when d is even, 0 otherwise
        int x = 0, y = r, d = 1 - r;
        int idx = 2;
        auto px = [&](int px, int py, int i) {
            if (!brushMask(i)) return;
            if (colorInvert())      setPixel(px, py, !getPixel(px,py));
            else if (colorSolid())  setPixel(px, py, true);
            else                    setPixel(px, py, false);
        };
        while (x <= y - even ) {
            px(x0 + x - even, y0 + y - even, idx - even); // 5 up left
            px(x0 + y - even, y0 + x - even, idx); // 4 up left
            px(x0 - x, y0 + y - even, idx); // 7 up
            px(x0 - y, y0 + x - even, idx-even); // 8 up
            px(x0 + x - even, y0 - y, idx); // 1 left
            px(x0 + y - even, y0 - x, idx-even); // 2 left
            px(x0 - x, y0 - y, idx - even); // 11 stays
            px(x0 - y, y0 - x, idx); // 10 stays
            x++; idx++;
            if (d < 0) d += 2*x + 1;
            else { d += 2*(x - y) + 1; y--; }
        }
    }




    // ---------------- Text ----------------

		inline void drawChar(int x, int y, char c) {
				uint8_t uc = uint8_t(c);
				if (uc < 0x10 || uc > 0xFF) return;   // ignore non-printable
				const uint8_t* glyph = font_mo[uc - 0x10];
				const uint8_t scale = (uint8_t)textScale_;

				for (int col = 0; col < 5; ++col) {
						uint8_t bits = glyph[col];

						for (int row = 0; row < 7; ++row) {
								if (bits & (1 << row)) {
										// scaled block draw
										if (scale == 1) {
												applyPixel(x + col, y + row);
										} else {
												int xs = x + col * scale;
												int ys = y + row * scale;

												// unrolled small square for scale==2
												applyPixel(xs,     ys);
												applyPixel(xs+1,   ys);
												applyPixel(xs,   ys+1);
												applyPixel(xs+1, ys+1);
										}
								}
						}
				}
		}

    inline void drawText(int x, int y, const char* txt)  {
        while (*txt) {
            drawChar(x, y, *txt++);
            x += (FONT_WIDTH + 1) * (int)textScale_ ;
        }
    }

	inline void drawTextBytes(int x, int y, const uint8_t* txt, int len) {
		for (int i = 0; i < len; ++i) {
			char c = (char)txt[i];
			if (c == 0) break;     // optional early stop
			drawChar(x, y, c);
			x += (FONT_WIDTH + 1) * (int)textScale_;
		}
	}


    inline void applyPixel(int x, int y) {
        // global color rules
        if (color_ == UIDisplayColor::WHITE)
            setPixel(x, y, true);
        else if (color_ == UIDisplayColor::BLACK)
            setPixel(x, y, false);
        else if (color_ == UIDisplayColor::INVERT) {
            bool cur = getPixel(x,y);
            setPixel(x, y, !cur);
        }
    }


		void drawBitmap(
        int x, int y,
        const uint8_t* bitmap,
        int w, int h,
        bool hFlip = false,
        bool vFlip = false
    ) {
        int bytesPerRow = (w + 7) / 8;

        for (int j = 0; j < h; ++j) {
            int srcRow = vFlip ? (h - 1 - j) : j;

            for (int i = 0; i < w; ++i) {
                int srcCol = hFlip ? (w - 1 - i) : i;

                int byteIndex = srcRow * bytesPerRow + (srcCol / 8);
                int bitIndex  = 7 - (srcCol % 8);

                if ((bitmap[byteIndex] >> bitIndex) & 1)
                    applyPixel(x + i, y + j);
            }
        }
    }


		void drawBitmapFragment(
        int dstX, int dstY,
        const uint8_t* src,
        int srcW, int srcH,
        int fragX, int fragY,
        int fragW, int fragH,
        bool hFlip = false,
        bool vFlip = false
    ) {
        int bytesPerRow = (srcW + 7) / 8;

        for (int j = 0; j < fragH; ++j) {
            int srcRow = fragY + (vFlip ? (fragH - 1 - j) : j);
            if (srcRow < 0 || srcRow >= srcH) continue;

            for (int i = 0; i < fragW; ++i) {
                int srcCol = fragX + (hFlip ? (fragW - 1 - i) : i);
                if (srcCol < 0 || srcCol >= srcW) continue;

                int byteIndex = srcRow * bytesPerRow + (srcCol / 8);
                int bitIndex  = 7 - (srcCol % 8);

                if ((src[byteIndex] >> bitIndex) & 1)
                    applyPixel(dstX + i, dstY + j);
            }
        } 
    }






};



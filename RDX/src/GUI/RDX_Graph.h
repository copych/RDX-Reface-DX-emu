#pragma once
#include "UI_Display.h"
#include "bmp_curves.h"

enum class UIEnvMode : uint8_t {
    ENV_RATES = 0,
    ENV_LEVELS
};

enum class UIEnvType : uint8_t {
    ENV_AMP = 0,
    ENV_PITCH
};


void drawPager(UI_Display& display, uint8_t x, uint8_t y, uint8_t total, uint8_t index) {
  display.setTextScale(UITextScale::X1);
  display.setBrush(UIDisplayBrush::SOLID);
  display.setColor(UIDisplayColor::WHITE);
  uint8_t x0 = display.getWidth() - (display.getFontWidth() + 1) * total ;
  for (int i = 0 ; i < total; ++i) {
    if (i != index) {
      display.drawChar(x0 + i * (display.getFontWidth() + 1), 0, char(128)); 
    } else {
      display.drawChar(x0 + i * (display.getFontWidth() + 1), 0, char(129)); 
    }
  }
}

void drawEG(UI_Display& display, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t levels[4], uint8_t rates[4], UIEnvType env_type, UIEnvMode env_mode) {
  int minV, maxV, cV;
  switch(env_type) {
    case UIEnvType::ENV_AMP:
      minV = 0;
      maxV = 127;
      cV = 0; 
      break;
    case UIEnvType::ENV_PITCH:
      minV = 16;
      maxV = 112;
      cV = 64; 
      break;
  }
  
  int maxSeg = (w - 2) / 5;
  int curX=x + 2, curY = 0, oldX = 0, oldY = 0;
  int gH = h - 6; // markers and a gap
  int l = 0;
  // draw rulers
  display.setColor(UIDisplayColor::WHITE);
  oldY = y + gH - map(cV, minV, maxV, 0, gH);
  oldX = x + 1;
  for (int i = 0; i < 4 ; ++i) {
    if (i == 0) {
      display.setBrush(UIDisplayBrush::SOLID);
      display.fillRect(x, y + h - 2, 3, 2);
      display.setPixel(x + 1, y + h - 4, true);
      display.setPixel(x + 1, y + h - 3, true);
    }

    int dx = map(rates[i], 0, 127, maxSeg, 0);
    curX += dx;
    l = map(levels[i], minV, maxV, 0, gH);
    curY = y + gH - l;
    display.setBrush(UIDisplayBrush::DOTTED);
    switch (env_mode) {
      case UIEnvMode::ENV_RATES:
          display.drawVLine(curX, y, gH);
        break;
      case UIEnvMode::ENV_LEVELS:
          display.drawHLine(x + 1, curY, w - 1);
        break;
    }
    
    display.setBrush(UIDisplayBrush::SOLID);
    display.drawLine(oldX, oldY, curX, curY);

    if (i == 2) {
      display.setBrush(UIDisplayBrush::DOTTED);
      curX += maxSeg;
      if (env_mode == UIEnvMode::ENV_RATES) display.drawVLine(curX, y, gH);

      display.setBrush(UIDisplayBrush::SOLID);
      display.drawHLine(curX - maxSeg, curY, maxSeg);
      display.fillRect(curX-1, y + h - 2, 3, 2);
      display.setPixel(curX, y + h - 4, true);
      display.setPixel(curX, y + h - 3, true);
    }
    oldX = curX;
    oldY = curY;
  
  }
  
  display.setBrush(UIDisplayBrush::SOLID);
  display.drawHLine(x + 1, y + gH - map(cV, minV, maxV, 0, gH), w - 2);
}

// Scaling curve types
/*
enum RDX_ScaleCurve : uint8_t {
    RDX_SCALE_NEG_LIN = 0,
    RDX_SCALE_NEG_EXP = 1,
    RDX_SCALE_POS_EXP = 2,
    RDX_SCALE_POS_LIN = 3
};
*/

void drawKeyScaling(UI_Display& display, uint8_t x0, uint8_t y0, uint8_t w, uint8_t h, uint8_t l_curve, uint8_t r_curve, uint8_t l_val, uint8_t r_val) {
  // center vertical line
    int x = x0 + 2;
    int y = y0 + 15;
    int yc = (h - y + y0) / 2 - 2;
    display.setTextScale(UITextScale::X1);
    display.setBrush(UIDisplayBrush::SOLID);
    display.setColor(UIDisplayColor::BLACK);
    display.drawVLine(x + w / 2, y , yc * 2);

    display.setBrush(UIDisplayBrush::DOTTED);
    display.setColor(UIDisplayColor::WHITE);
    display.drawVLine(x + w / 2, y , yc - 2);
    display.drawVLine(x + w / 2, y + yc + 3 ,  yc - 2 );
    display.setPixel( x + w / 2, y + yc - 1 , true);
    display.setPixel( x + w / 2, y + yc , true);

    int yb = y + yc - 15;
    display.setBrush(UIDisplayBrush::SOLID);
    switch(l_curve) {
      case 0: // neg lin
        display.drawText(x, y0, " LIN" ) ;
        if(l_val>0) {
          display.drawBitmap( x, yb, curve_lin, 30, 31, true, true);
        } else {
          display.fillRect(x, yb + 14, 30, 2);
        }
        break;
      case 1: // neg exp
        display.drawText(x, y0, " EXP" ) ;
        if(l_val>0) {
          display.drawBitmap( x, yb, curve_exp, 30, 31, true, true);
        } else {
          display.fillRect(x, yb + 14, 30, 2);
        }
        break;
      case 2: // pos exp
        display.drawText(x, y0, " EXP" ) ;
        if(l_val>0) {
          display.drawBitmap( x, yb, curve_exp, 30, 31, true, false);
        } else {
          display.fillRect(x, yb + 14, 30, 2);
        }
        break;
      case 3: // pos lin
        display.drawText(x, y0, " LIN" ) ;
        if(l_val>0) {
          display.drawBitmap( x, yb, curve_lin, 30, 31, true, false);
        } else {
          display.fillRect(x, yb + 14, 30, 2);
        }
        break;
    }
    switch(r_curve) {
      case 0: // neg lin
        display.drawText(x + 32, y0, " LIN" ) ;
        if(r_val>0) {
          display.drawBitmap( x + 31, yb, curve_lin, 30, 31, false, true);
        } else {
          display.fillRect(x + 31, yb + 14, 30, 2);
        }
        break;
      case 1: // neg exp
        display.drawText(x + 32, y0, " EXP" ) ;
        if(r_val>0) {
          display.drawBitmap( x + 31, yb, curve_exp, 30, 31, false, true);
        } else {
          display.fillRect(x + 31, yb + 14, 30, 2);
        }
        break;
      case 2: // pos exp
        display.drawText(x + 32, y0, " EXP" ) ;
        if(r_val>0) {
          display.drawBitmap( x + 31, yb, curve_exp, 30, 31, false, false);
        } else {
          display.fillRect(x + 31, yb + 14, 30, 2);
        }
        break;
      case 3: // pos lin
        display.drawText(x + 32, y0, " LIN" ) ;        
        if(r_val>0) {
          display.drawBitmap( x + 31, yb, curve_lin, 30, 31, false, false);
        } else {
          display.fillRect(x + 31, yb + 14, 30, 2);
        }
        break;
    }
}

void drawFX(UI_Display& display, uint8_t x, uint8_t y,uint8_t fx_id) {
    display.setTextScale(UITextScale::X1);
    display.setBrush(UIDisplayBrush::SOLID);
    display.setColor(UIDisplayColor::WHITE);
    fx_id %= 8;
    display.drawBitmapFragment(x, y, bmp_fx, 32*8, 32, fx_id*32, 0, 32, 32);
}

void drawWave(UI_Display& display, uint8_t x, uint8_t y, uint8_t wave_id) {
    display.setTextScale(UITextScale::X1);
    display.setBrush(UIDisplayBrush::SOLID);
    display.setColor(UIDisplayColor::WHITE);
    wave_id %= 7;
    display.drawBitmapFragment(x, y, bmp_waves, 32*7, 32, wave_id*32, 0, 32, 32);
}

// draws vertical lines from the bottom height=h, number of zones=n (not dividers)
void drawDividers(UI_Display& display, uint8_t n, uint8_t y0, uint8_t h) {
    display.setTextScale(UITextScale::X1);
    display.setBrush(UIDisplayBrush::SOLID);
    display.setColor(UIDisplayColor::WHITE);
    int dx = display.getWidth() / n; 
    for (int i = 1; i < n ; i++ ) {
        display.drawVLine(dx * i, y0, h);
    }
}
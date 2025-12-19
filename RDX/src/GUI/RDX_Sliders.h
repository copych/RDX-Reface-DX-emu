#pragma once
#include "UI_Display.h"



// vertical 0..127 zebra gauge with optional value at the top
void drawSlider1(UI_Display& display, uint8_t x, uint8_t y, uint8_t w, uint8_t h, int v, int minV, int maxV, bool show_val = true, bool show_sign = true) {
  int c = 0;
  char txt[5];
  if (show_sign) {
    snprintf(txt, sizeof(txt), "%d", v);
  } else {
    snprintf(txt, sizeof(txt), "%d", abs(v));
  }
  uint8_t textW = 0;
  uint8_t textH = 0;
  uint8_t sH = h;
  
  display.setBrush(UIDisplayBrush::SOLID);
  display.setColor(UIDisplayColor::WHITE);
  if (show_val) {
    textH = display.getFontHeight();
    textW = display.getTextWidth(txt);
    sH = h - textH - 2;
    display.drawText(x + (w - textW), y , txt);
  }
  
  int j = map(v, 0, 127, 0, sH);
  bool drawn = false;
  for (int i = 0; i < j; i++) {
    display.setColor((UIDisplayColor)c);
    if (c > 0) {
      display.drawHLine(x, y + h - i, w);
      drawn = true;
    }
    c = i % 2;
  }
  if (!drawn) {    
    display.setColor(UIDisplayColor::WHITE);
    display.drawHLine(x, y + h, w);
  }
}

// vertical -127..127 narrow zebra gauge with center and icons for feedback
void drawSlider2(UI_Display& display, uint8_t x, uint8_t y, uint8_t w, uint8_t h, int v, int minV, int maxV, bool show_val = true, bool show_sign = true) {
  int c = 0;
  uint8_t sW = 8; 
  char txt[5];
  if (show_sign) {
    snprintf(txt, sizeof(txt), "%d", v);
  } else {
    snprintf(txt, sizeof(txt), "%d", abs(v));
  }
  uint8_t textW = 0;
  uint8_t textH = 0;
  uint8_t sH = h;
  
  display.setBrush(UIDisplayBrush::SOLID);
  display.setColor(UIDisplayColor::WHITE);
  if (show_val) {
    textH = display.getFontHeight();
    textW = display.getTextWidth(txt);
    sH = h - textH - 2;
    display.drawText(x + (w - textW), y , txt);
  }
  
  int l;
  
  if (v >= 1) {
    l = map(v, 1, maxV, 2, sH/2) ;
  } else if (v <= -1) { 
    l = map(-v, 1, -minV, 2, sH/2) ;
  } else {
    l = 0;
  }
  display.setColor(UIDisplayColor::WHITE);
  display.drawHLine(x , y + h - sH/2, w);
  if (v>0) {
    display.drawChar(x + 1, y + h - sH + sH/4 , static_cast<char>(134));
    for (int i = 0; i <= l; i++) {
      display.setColor((UIDisplayColor)c);
      if (c > 0) display.drawHLine(x + w - sW, y + h - sH / 2 - i, sW);
      c = i%2;
    }
  } else {
    if(v<0) display.drawChar(x + 1, y + h - sH/4 -2 , static_cast<char>(135));
    for (int i = 0; i <= l ; i++) {
      display.setColor((UIDisplayColor)c);
      if (c > 0) display.drawHLine(x + w - sW, y + h - sH / 2 + i, sW);
      c = i%2;
    }
  }  
}


// vertical -v..v slider with center marks
void drawSlider3( UI_Display& display, 
                  uint8_t x, uint8_t y, 
                  uint8_t w, uint8_t h, 
                  int v, int minV, int maxV, 
                  bool show_val = true, 
                  bool show_sign = true
                  ) {
  int c = 0;
  char txt[5];
  snprintf(txt, sizeof(txt), "%d", abs(v));
  uint8_t textW = 0;
  uint8_t textH = 0;
  uint8_t sH = h;
  
  display.setColor(UIDisplayColor::WHITE);
  display.setBrush(UIDisplayBrush::SOLID);
  if (show_val) {
    textH = display.getFontHeight();
    textW = display.getTextWidth(txt);
    sH = h - textH - 2;
    display.drawText(x + (w - textW), y , txt);
  }
  
  display.drawHLine(x + 1, y + h - sH / 2, 2);
  display.drawHLine(x + w - 3 , y + h - sH / 2, 2);
  display.drawVLine(x , y + h - sH / 2 - 1, 3);
  display.drawVLine(x + w - 1  , y + h - sH / 2 - 1, 3);

  display.setBrush(UIDisplayBrush::DOTTED);
  display.drawVLine(x + w / 2, y + h - sH + 2, sH - 2);

  display.setBrush(UIDisplayBrush::SOLID);
  int p = map(v, minV, maxV, 1, sH-1);

  if (show_sign) {
    if (v>0) {
      display.drawChar(x , y , '+');
    } else if (v<0) {
      display.drawChar(x , y , '-');
    }
  }

  display.fillRect(x+4, y + h - p - 1, w - 8, 3);
}


// vertical -127..127 slider with center marks
void drawSlider4(UI_Display& display, uint8_t x, uint8_t y, uint8_t w, uint8_t h, int v, int minV, int maxV, bool show_val = true, bool show_sign = true) {
  int c = 0;
  char txt[5];
  snprintf(txt, sizeof(txt), "%d", abs(v));
  uint8_t textW = 0;
  uint8_t textH = 0;
  uint8_t sH = h;
  
  display.setColor(UIDisplayColor::WHITE);
  display.setBrush(UIDisplayBrush::SOLID);
  if (show_val) {
    textH = display.getFontHeight();
    textW = display.getTextWidth(txt);
    sH = h - textH - 2;
    display.drawText(x + (w - textW), y , txt);
  }

  display.setBrush(UIDisplayBrush::DOTTED);
  display.drawVLine(x + w / 2, y + h - sH + 2, sH - 2);

  display.setBrush(UIDisplayBrush::SOLID);
  int p = map(v, minV, maxV, 1, sH-1);

  if (show_sign) {
    if (v>0) {
      display.drawChar(x , y , '+');
    } else if (v<0) {
      display.drawChar(x , y , '-');
    }
  }

  display.fillRect(x+4, y + h - p - 1, w - 8, 3);
}



// vertical -127..127 narrow zebra gauge with center and icons for feedback
void drawSlider5(UI_Display& display, uint8_t x, uint8_t y, uint8_t w, uint8_t h, int v, int minV, int maxV, bool show_val = true, bool show_sign = true) {
  int c = 0;
  uint8_t sW = w; 
  char txt[5];
  if (show_sign) {
    snprintf(txt, sizeof(txt), "%+d", v);
  } else {
    snprintf(txt, sizeof(txt), "%d", abs(v));
  }
  uint8_t textW = 0;
  uint8_t textH = 0;
  uint8_t sH = h;
  
  display.setBrush(UIDisplayBrush::SOLID);
  display.setColor(UIDisplayColor::WHITE);
  if (show_val) {
    textH = display.getFontHeight();
    textW = display.getTextWidth(txt);
    sH = h - textH - 2;
    display.drawText(x + (w - textW), y , txt);
  }
  
  int l;
  
  if (v >= 1) {
    l = map(v, 1, maxV, 2, sH/2) ;
  } else if (v <= -1) { 
    l = map(-v, 1, -minV, 2, sH/2) ;
  } else {
    l = 0;
  }
  display.setColor(UIDisplayColor::WHITE);
  display.drawHLine(x , y + h - sH/2, w);
  if (v>0) {
    for (int i = 0; i <= l; i++) {
      display.setColor((UIDisplayColor)c);
      if (c > 0) display.drawHLine(x + w - sW, y + h - sH / 2 - i, sW);
      c = i%2;
    }
  } else {
    for (int i = 0; i <= l ; i++) {
      display.setColor((UIDisplayColor)c);
      if (c > 0) display.drawHLine(x + w - sW, y + h - sH / 2 + i, sW);
      c = i%2;
    }
  }  
}

void drawFreq(UI_Display& display, int x, int y, int w, int h, int8_t i, int8_t f) {
  char txt[7];
  display.setBrush(UIDisplayBrush::SOLID);
  display.setColor(UIDisplayColor::WHITE);
  display.setTextScale(UITextScale::X2);
  snprintf(txt, sizeof(txt), "%d", i);
  int wt = display.getTextWidth(txt);
  int ht = display.getFontHeight();
  display.drawText(x + w - wt , y , txt);
  display.setTextScale(UITextScale::X1);
  snprintf(txt, sizeof(txt), ".%02d", f);
  wt = display.getTextWidth(txt);
  display.drawText(x + w - wt , y + ht + 4, txt);
  display.setTextScale(UITextScale::X1);
}
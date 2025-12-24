#pragma once


#include "RDX_State.h"
#include "src/GUI/UI_Display.h"

#ifdef OLED_SSD1306
//#include "src/GUI/UI_SSD1306.h"
// SSD1306_Display display;
#endif

#ifdef OLED_SH1106
#include "src/GUI/UI_SH1106.h"
SH1106_Display display;
#endif

#ifdef OLED_SH1107
//#include "src/GUI/UI_SH1107.h"
// SH1107_Display display;
#endif

#ifdef LCD_ST7565
//#include "src/GUI/UI_ST7565.h"
// ST7565_Display display;
#endif

#include "src/GUI/bmp_fx.h"
#include "src/GUI/bmp_waves.h"
#include "src/GUI/bmp_slider.h"
#include "src/GUI/UI_Algos.h"
#include "src/GUI/RDX_Sliders.h"
#include "src/GUI/RDX_Graph.h"


#include "RDX_PresetManager.h"
extern PresetManager pm;


class RDX_GUI {
  public:
    RDX_GUI () {};
    
    inline void begin() { display.begin(); }

    inline void draw() {
      if (updateCounter_ > 0) {
        updateCounter_--;
        return;
      } 
      if(!needUpdate_) return;

      display.clear();
      display.setColor(UIDisplayColor::WHITE);
      display.setBrush(UIDisplayBrush::SOLID);
      display.setTextScale(UITextScale::X2);
      display.drawTextBytes(4, 10, patch_.common.voiceName, 10);

      display.setTextScale(UITextScale::X1);
      display.drawText(2, 0, "Bank  -");

      uint8_t m, l;
      m = (pm.currentIndex()-1) / 8 + 1;
      l = (pm.currentIndex()-1) % 8 + 1;
      char t[5];
      snprintf(t, sizeof(t), "%d", m);
      display.drawText(2 + 6 * 5, 0, t);
      snprintf(t, sizeof(t), "%d", l);
      display.drawText(2 + 6 * 7, 0, t);

      display.setBrush(UIDisplayBrush::DOTTED);
      display.drawHLine(0, 28, 128);

      drawAlgo(display, 30,  patch_.common.algorithm, 33, true);
      display.update();
      needUpdate_ = false;
    }

    inline void push() { needUpdate_ = true; }

    inline void pause(int32_t n) { updateCounter_ = n; }

  private:
  
    RDX_Patch&          patch_          = RDX_State::getState().workingPatch; 
    bool needUpdate_ = false;
    int32_t updateCounter_ = 0; 

};
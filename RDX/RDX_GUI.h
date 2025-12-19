#pragma once

#define OLED_USE_SPI 0
#define OLED_USE_I2C 1

#define OLED_WIDTH 128
#define OLED_HEIGHT 64

#define OLED_I2C_SDA -1 // -1 for autodetection
#define OLED_I2C_SCL -1

#include "RDX_State.h"
#include "src/GUI/UI_Display.h"

//#include "src/GUI/UI_SSD1306.h"
#include "src/GUI/UI_SH1106.h"
//#include "src/GUI/UI_SH1107.h"

// SSD1306_Display display;
SH1106_Display display;
// SH1107_Display display;

#include "src/GUI/bmp_fx.h"
#include "src/GUI/bmp_waves.h"
#include "src/GUI/bmp_slider.h"
#include "src/GUI/UI_Algos.h"
#include "src/GUI/RDX_Sliders.h"
#include "src/GUI/RDX_Graph.h"

class RDX_GUI {
  public:
    RDX_GUI () {};
    
    inline void begin() { display.begin(); }

    inline void draw() {
      if(!needUpdate_) return;

      display.clear();

      display.setBrush(UIDisplayBrush::SOLID);

      display.setColor(UIDisplayColor::WHITE);

      display.drawText(2, 0, "Bank");
      display.setTextScale(UITextScale::X2);
      display.drawTextBytes(4, 10, patch_.common.voiceName, 10);
      
      display.setTextScale(UITextScale::X1);
      display.setBrush(UIDisplayBrush::DOTTED);
      
      display.drawHLine(0, 28, 128);
      drawAlgo(display, 30,  patch_.common.algorithm, 33, true);
      display.update();

      delay(500);
      needUpdate_ = false;
    }

    inline void push() { needUpdate_ = true;}

    inline void pause(int) {;}

  private:
  
    RDX_Patch&          patch_          = RDX_State::getState().workingPatch; 
    bool needUpdate_ = false;

};
#pragma once
#include "UI_Display.h"

static const int OP_PX = 13;

static const bool carriers[12][4] = {
  {1,0,0,0},
  {1,0,0,0},
  {1,0,0,0},
  {1,0,0,0},
  {1,0,0,0},
  {1,1,0,0},
  {1,1,0,0},
  {1,0,1,0},
  {1,1,1,0},
  {1,1,1,0},
  {1,1,1,0},
  {1,1,1,1}
};

static const uint8_t offsets[12][4] = {
  {0,1,1,1},
  {0,0,0,1},
  {0,1,1,0},
  {0,1,0,1},
  {0,2,1,0},
  {0,0,1,1},
  {0,0,1,1},
  {0,1,0,1},
  {0,0,0,2},
  {0,0,0,1},
  {0,0,0,1},
  {0,0,0,0}
};


// Drawing of RDX Algorithms

void drawAlgo(UI_Display& display, int y0, uint8_t algo_id, int hTotal, bool showId = false) {
  bool compact = false;
  int8_t maxCarrier = 0; 
  int vSink = 4;
  int labelOff = 3;
  int8_t x[4], y[4];
  int fh2 = display.getFontHeight()/2;
  int fw2 = display.getFontWidth()/2;
  int nn = display.getWidth() / 4;
  int x0 = nn / 2;
  int ww = OP_PX / 2;
  int lh = showId ? display.getFontHeight() + labelOff : 0;
  int hMax = hTotal - lh;
  int vLink = (hMax - 3 * OP_PX - vSink) / 2; 
  if (vLink < 0) {
    compact = true;
    vLink = (hMax - OP_PX) / 2;
  }

  int8_t yy = y0 + hMax - OP_PX / 2 - vSink ;
  display.setBrush(UIDisplayBrush::SOLID);
  display.setColor(UIDisplayColor::WHITE);
  for ( int i=0; i<4; i++ ) {
    x[i] = i * nn + x0;
    if (compact) {
      y[i] = yy ;
    }else{
      y[i] = yy - (OP_PX + vLink) * offsets[algo_id][i] ;
    }
  }

  for (uint8_t id = 0 ; id < 4 ; id++) {
    display.drawChar(x[id] - fw2, y[id] - fh2, static_cast<char>(id + '1')); // draw op number
    if (carriers[algo_id][id]) {
      maxCarrier = id;
      display.drawRect(x[id] - ww, y[id] - ww, OP_PX, OP_PX);
      display.drawVLine(x[id], y[id] + ww, vSink+1);
    } else {
      display.drawCircle(x[id], y[id], OP_PX );
    }
  }

  if (maxCarrier > 0) {
    display.drawHLine(nn/2, yy + ww + vSink, maxCarrier * nn + 1);
  }

  if (showId) {
    int offset = algo_id>=9 ? display.getFontWidth() : fw2; 
    display.drawText(x[0] + (x[maxCarrier] - x[0]) / 2 - offset, y0 + hTotal - display.getFontHeight(), toText(algo_id + 1));
  }

  switch(algo_id){
    case 0:
      display.drawHLine(x[2] + ww, y[2], nn - OP_PX + 1); // short 3-4
      display.drawHLine(x[1] + ww, y[1], nn - OP_PX + 1); // short 2-3
      if (compact) {
        display.drawHLine(x[0] + ww, y[0], nn - OP_PX + 1); // short 1-2
      } else {
        display.drawHLine(x[0] , y[1], nn - ww + 1);        // h angle 2-1
        display.drawVLine(x[0] , y[1], ww + vLink + 1 );    // v 2 to 1 
      }
      break;
    
    case 1:
      display.drawHLine(x[1] + ww, y[1], nn - OP_PX + 1); // short 2-3
      display.drawHLine(x[0] + ww, y[0], nn - OP_PX + 1); // short 1-2
      if (compact) {
        display.drawHLine(x[1], y[0] - ww - vLink, x[3] - x[1]); // upper 2 ops 2-4
        display.drawVLine(x[1], y[0] - ww - vLink, vLink);       // link up 2
        display.drawVLine(x[3], y[0] - ww - vLink, vLink);       // link up 4
      } else {
        display.drawHLine(x[1], y[3], x[3] - x[1] - ww + 1); // angle 2 ops
        display.drawVLine(x[1], y[3], ww + vLink + 1);        // v 4 to 2
      }
      break;
    
    case 2:
      display.drawHLine(x[1] + ww, y[1], nn - OP_PX + 1); // short 2-3
      if (compact) {
        display.drawHLine(x[0] + ww, y[0], nn - OP_PX + 1);
        display.drawHLine(x[0], y[0] - ww - vLink, x[3] - x[0]);
        display.drawVLine(x[0], y[0] - ww - vLink, vLink);
        display.drawVLine(x[3], y[0] - ww - vLink, vLink);
      } else {
        display.drawHLine(x[0], y[1], nn - ww + 1);        // h angle 2-1
        display.drawVLine(x[0], y[1], ww + vLink + 1 );    // v to 1 
        display.drawHLine(x[0] + ww, y[0], x[3] - x[0] - OP_PX + 1); // long 4-1
      }
      break;
 
    case 3:
      if (compact) {
        display.drawHLine(x[0] , y[0] - ww - vLink, x[2] - x[0]); // upper 2 ops
        display.drawVLine(x[0] , y[0] - ww - vLink, vLink+1);       // link up 1
        display.drawVLine(x[2] , y[0] - ww - vLink, vLink+1);       // link up 3
        display.drawHLine(x[1] , y[0] + ww + vLink, x[3] - x[1]); // upper 2 ops
        display.drawVLine(x[1] , y[0] + ww, vLink+1);       // link dn 2
        display.drawVLine(x[3] , y[0] + ww, vLink+1);       // link dn 4
        display.drawHLine(x[0] + ww, y[0], nn - OP_PX + 1); // short 1-2
        display.drawHLine(x[2] + ww, y[2], nn - OP_PX + 1); // short 3-4
      } else {
        display.drawHLine(x[0] , y[1], nn - ww + 1);        // h angle 2-1
        display.drawVLine(x[0] , y[1], ww + vLink + 1 );    // v to 1 
        display.drawHLine(x[0] + ww, y[0], 2 * nn - OP_PX + 1); // dbl 1-3
        display.drawHLine(x[1] + ww, y[1], 2 * nn - OP_PX + 1); // dbl 2-4
        display.drawHLine(x[2] + ww, y[2], nn - ww + 1);        // h dn angle 3-4
        display.drawVLine(x[3] , y[3] + ww, ww + vLink + 1 );    // v to 4 
      }
      break;

    case 4:
      if (compact) {
        display.drawHLine(x[0] , y[0] - ww - vLink, x[3] - x[0]); // upper 4 ops
        display.drawVLine(x[0] , y[0] - ww - vLink, vLink+1);       // link up 1
        display.drawVLine(x[1] , y[0] - ww - vLink, vLink+1);       // link up 2
        display.drawVLine(x[2] , y[0] - ww - vLink, vLink+1);       // link up 3
        display.drawVLine(x[3] , y[0] - ww - vLink, vLink+1);       // link up 3
      } else {
        display.drawHLine(x[0] , y[2], x[2] - x[0] - ww + 1);       // h angle 3-1
        display.drawVLine(x[0] , y[2], ww + vLink + 1 );            // v 3 to 1 
        display.drawHLine(x[0] + ww, y[0], x[3] - x[0] - OP_PX + 1);// long 4-1
        
        display.drawHLine(x[0] - ww - vSink, y[1], nn + vSink + 1); // 
        display.drawHLine(x[0] - ww - vSink, y[0], vSink + 1 );     //
        display.drawVLine(x[0] - ww - vSink, y[1], y[0] - y[1] + 1);//  
      }
      break;

    case 5:
      display.drawHLine(x[2] + ww, y[2], nn - OP_PX + 1); // short 3-4
      if (compact) {
        display.drawHLine(x[1] + ww, y[1], nn - OP_PX + 1); // short 2-3
      } else {
        display.drawHLine(x[1] , y[2], x[1] - x[0] - ww + 1);        // h angle 2-1
        display.drawVLine(x[1] , y[2], ww + vLink + 1 );    // v 2 to 1 
      }
      break;

    case 6:
      display.drawHLine(x[2] + ww, y[2], nn - OP_PX + 1); // short 3-4
      if (compact) {
        display.drawHLine(x[1] + ww, y[1], nn - OP_PX + 1); // short 2-3
        display.drawHLine(x[0], y[0] - ww - vLink, x[2] - x[0]); // upper 2 ops
        display.drawVLine(x[0], y[0] - ww - vLink, vLink+1);       // link up 1
        display.drawVLine(x[2], y[0] - ww - vLink, vLink+1);       // link up 3
      } else {
        display.drawHLine(x[0] , y[2], x[2] - x[0] - ww + 1);        // h angle 2-1
        display.drawVLine(x[0] , y[2], ww + vLink + 1 );    // v 2 to 1 
        
        display.drawHLine(x[1] + ww , y[1], x[2] - x[1] - ww + 1);       // h angle 2-3
        display.drawVLine(x[2] , y[2] + ww, ww + vLink + 1 );            // v 2 to 3
      }
      break;

    case 7:
      if (compact) {
        display.drawHLine(x[0] + ww, y[0], nn - OP_PX + 1); // short 1-2
        display.drawHLine(x[2] + ww, y[2], nn - OP_PX + 1); // short 3-4
      } else {
        display.drawHLine(x[0] , y[1], nn - ww + 1);        // h angle 2-1
        display.drawVLine(x[0] , y[1], ww + vLink + 1 );    // v 2 to 1 
        display.drawHLine(x[2] , y[1], nn - ww + 1);        // h angle 4-3
        display.drawVLine(x[2] , y[1], ww + vLink + 1 );    // v 3 to 4 
      }
      break;

    case 8:
      if (compact) {
        display.drawHLine(x[2] + ww, y[2], nn - OP_PX + 1);         // short 3-4
        display.drawVLine(x[0] , y[0] - ww - vLink, vLink+1);       // link up 1
        display.drawVLine(x[1] , y[0] - ww - vLink, vLink+1);       // link up 2
        display.drawVLine(x[3] , y[0] - ww - vLink, vLink+1);       // link up 4
        display.drawHLine(x[0] , y[0] - ww - vLink, x[3] - x[0]);   // upper 3 ops
      } else {
         
        display.drawHLine(x[0], y[3], x[3] - x[0] - ww + 1);        // long 4-1
        display.drawVLine(x[0], y[3], y[0] - y[3] - ww + 1);        // link up 1

        display.drawHLine(x[1] , y[3] + OP_PX + vLink, x[3] - x[1] + 1);  // h dn angle 3-4
display.drawVLine(x[3] , y[3] + ww, OP_PX + vLink - ww + 1 );    // v to 4 
display.drawVLine(x[1] , y[3] + OP_PX + vLink , OP_PX + vLink - ww + 1 );    // v to 4 

        display.drawHLine(x[2] + ww, y[2], x[3] - x[2] + vSink + 1);        // h dn angle 3-4
        display.drawHLine(x[3] + ww, y[3], vSink + 1);        // h dn angle 3-4
        display.drawVLine(x[3] + ww + vSink, y[3], y[0] - y[3] + 1);        // link up 1
 //       

   //     


  //      
//        display.drawVLine(x[3] , y[3] + ww, (y[3]-y[2]) / 2 + 1 );    // v to 4 
      }
      break;

    case 9:
      if (compact) {
        display.drawHLine(x[2] + ww, y[2], nn - OP_PX + 1); // short 3-4
        display.drawHLine(x[1], y[0] - ww - vLink, x[3] - x[1]); // upper 2 ops 2-4
        display.drawVLine(x[1], y[0] - ww - vLink, vLink);       // link up 2
        display.drawVLine(x[3], y[0] - ww - vLink, vLink);       // link up 4
      } else {         
        display.drawHLine(x[1], y[3], x[3] - x[1] - ww + 1); // angle 2 ops
        display.drawVLine(x[1], y[3], ww + vLink + 1);        // v 4 to 2
        display.drawHLine(x[2] + ww, y[2], nn - ww + 1);        // h dn angle 3-4
        display.drawVLine(x[3] , y[3] + ww, ww + vLink + 1 );    // v to 4 

      }
      break;

  
    case 10:
      if (compact) {
        display.drawHLine(x[2] + ww, y[2], nn - OP_PX + 1); // short 3-4
      } else {         
        display.drawHLine(x[2] , y[3], nn - ww + 1);        // h angle 4-3
        display.drawVLine(x[2] , y[3], ww + vLink + 1 );    // v 3 to 4 

      }
      break;
    
    case 11:
    default:
      break;
  }
}

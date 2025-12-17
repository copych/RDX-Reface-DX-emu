#pragma once

#include "config.h"
#include "misc.h"
#include "src/InputManager/src/mux4067.h"
#include "src/InputManager/src/InputManager.h"

extern RDX_Synth synth;
extern PresetManager pm;

/*
// Create multiplexer
#if defined(CONFIG_IDF_TARGET_ESP32S3)
Mux4067stack Mux(4, 5, 6, 7, 15, 16);
#elif defined(CONFIG_IDF_TARGET_ESP32)
Mux4067stack Mux(27, 14, 12, 13, 32, 33);
#endif
*/




struct TunableParams {
    float param[4]  = {0.5f, 0.5f, 0.5f, 0.5f};
};
TunableParams tunableParams;

enum ParamSel { P_1 = 20, P_2, P_3, P_4};
ParamSel currentSel = P_1;


// Create input manager (pass multiplexer pointer)
InputManager inputManager(nullptr);

void onEncoder(int id, int dir) {
    int cur_id = currentSel - (int)(P_1);
    cur_id = constrain(cur_id, 0, 3);
    tunableParams.param[cur_id] = fclamp(tunableParams.param[cur_id] + dir * 0.01f, 0.f, 1.f);
    ESP_LOGI("CTRL", "param %d: %f", cur_id, tunableParams.param[cur_id]);
}

void onButton(int id, MuxButton::btnEvents evt) {
    ESP_LOGI("CTRL", "Button %d event: %d", id, evt);
    currentSel = (ParamSel)id;

    RDX_Patch patch;
    if (evt == MuxButton::EVENT_CLICK) {        
        if (id == 21) {
            pm.loadPrev(patch);
            synth.applyPatch(patch);
            ESP_LOGI("CTRL", "%s", patch.common.voiceName);
        } else if (id == 22) {
            pm.loadNext(patch);
            synth.applyPatch(patch);
            ESP_LOGI("CTRL", "%s", patch.common.voiceName);
        }
    }

}



void initControls() {
    
  //  Mux.reset();
    /*
    // Configure multiplexed inputs
    inputManager.addMuxedEncoder(0, 0, 0, 1, 0); // Muxed Encoder 0
    inputManager.addMuxedEncoder(1, 0, 1, 1, 1); // Muxed Encoder 1
    
    inputManager.addMuxedButton(0, 0, 3); // Muxed button 0
    inputManager.addMuxedButton(1, 0, 4); // Muxed button 1
    */
    // Configure direct encoders
	
    inputManager.addDirectEncoder(10, ENC0_A_PIN, ENC0_B_PIN, MuxEncoder::MODE_QUAD_STEP); // Direct encoder on GPIO ENC0_A_PIN (A) and ENC0_B_PIN (B)
  //  inputManager.addDirectEncoder(11, 27, 14, MuxEncoder::MODE_QUAD_STEP); // Quad step encoder
    
    // Configure direct buttons
    inputManager.addDirectButton(20, BTN0_PIN, true);  // Direct button on GPIO BTN0_PIN (active low)
    inputManager.addDirectButton(21, BTN1_PIN, true);  // Direct button on GPIO BTN0_PIN (active low)
    inputManager.addDirectButton(22, BTN2_PIN, true);  // Direct button on GPIO BTN0_PIN (active low)
    inputManager.addDirectButton(23, BTN3_PIN, true);  // Direct button on GPIO BTN1_PIN (active low)
    inputManager.addDirectButton(24, BTN4_PIN, true);  // Direct button on GPIO BTN1_PIN (active low)
    inputManager.addDirectButton(25, BTN5_PIN, true);  // Direct button on GPIO BTN1_PIN (active low)
  /*  
    // Custom inputs with special settings
    inputManager.addDirectEncoder(12, 15, 16, MuxEncoder::MODE_DOUBLE_STEP); // Double step encoder
    inputManager.addDirectButton(22, 17, true, true, false, 20, 10, 800, 200); // Button with auto-click
    */
    // Initialize with callbacks
    inputManager.initialize(onEncoder, onButton);
    
    ESP_LOGI("CTRL", "Total encoders: %d" , inputManager.getTotalEncoderCount());
    ESP_LOGI("CTRL", "Total buttons: %d",  inputManager.getTotalButtonCount());
}


inline void processControls() {
    // Single call to process all inputs (both multiplexed and direct)
    inputManager.process();
}
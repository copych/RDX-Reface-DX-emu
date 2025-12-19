// RDX.ino

#pragma once

#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")
#pragma GCC optimize ("unsafe-math-optimizations")
#pragma GCC optimize ("no-math-errno")

#include <Arduino.h>
#include "esp_log.h"
#include "config.h"
#include "misc.h"
int VOICES = MAX_VOICES;

#include "RDX_PresetManager.h"

#include "RDX_Synth.h"
#include "RDX_Midi.h"
#include "src/i2s/i2s_in_out.h"
#include "RDX_FX.h"

#include "controls.h"

#include <FS.h>
#include <LittleFS.h>
//#include <SD_MMC.h>



constexpr char* TAG = "RDX";

float DRAM_ATTR outL[DMA_BUFFER_LEN];
float DRAM_ATTR outR[DMA_BUFFER_LEN];

// debug
volatile int time1, time2 = 0;
uint32_t audioWM = 0, midiWM = 0, guiWM = 0;
float rmsL = 0.f, rmsR = 0.f; 

RDX_Synth synth;
I2S_Audio audio;

PresetManager pm;

TaskHandle_t audioTaskHandle;
TaskHandle_t midiTaskHandle;
TaskHandle_t guiTaskHandle = nullptr;
 

static FXHost fx;


#ifdef ENABLE_GUI
    #include "RDX_GUI.h"
    RDX_GUI gui;
#endif



// ------------------- Audio Task -----------------------
static void IRAM_ATTR audioTask(void*) {
    vTaskDelay(30);
    ESP_LOGI(TAG, "Starting Audio task");
    vTaskDelay(50); 
    while (true) {
        uint32_t start = micros();

        synth.renderAudioBlock(outL, outR); 
        
        uint32_t end = micros();

        time1 = end - start; 

		fx.process(outL, outR );

        time2 = micros() - end;

        audio.writeBuffers(outL, outR);
        
    }
}


// ------------------- MIDI Task ------------------------
static void IRAM_ATTR midiTask(void*) {
    const int budgetMicros = 1e+06f * DMA_BUFFER_LEN / SAMPLE_RATE ;
    vTaskDelay(40);
    ESP_LOGI(TAG, "Starting MIDI task");
    vTaskDelay(40);
    int d = 0;
    while (true) {
        processMidi();   // incoming messages
        vTaskDelay(1);

        processControls();
        taskYIELD();
        
        synth.updateCache(); // cache some not-so-critical params to local members to speed up hot paths

        if (++d % 1024 == 0) {
            midiWM = uxTaskGetStackHighWaterMark(midiTaskHandle);
            audioWM = uxTaskGetStackHighWaterMark(audioTaskHandle);
            #if 1   // --- diagnostics
                for (int i = 0; i < DMA_BUFFER_LEN; ++i) {
                    rmsL += outL[i] * outL[i];
                    rmsR += outR[i] * outR[i];
                }
                rmsL = sqrtf(rmsL / DMA_BUFFER_LEN);
                rmsR = sqrtf(rmsR / DMA_BUFFER_LEN);
            #endif
            ESP_LOGI("STATE","synth %d + fx %d = %d of %d micros, RMS %f Free stack: audio %ld B midi %ld B gui %ld B", time1, time2, time1+time2, budgetMicros, rmsL + rmsR, audioWM, midiWM, guiWM);
//            for (int i = 0 ; i < VOICES; ++i) {
  //              ESP_LOGI("STATE","voice %d\t active %d\t score %f" , i, synth.getVoice(i).isActive(), synth.getVoice(i).calcScore());
    //        }
        }
    }
}

#ifdef ENABLE_GUI
// ------------------- GUI Task ------------------------
static void IRAM_ATTR gui_task(void*) {
    vTaskDelay(50);
    ESP_LOGI(TAG, "Starting GUI task");
    vTaskDelay(30);
    while (true) {
        gui.draw();
        vTaskDelay(1);
    }
}
#endif

// ------------------- Setup ---------------------------
void setup() {
    Serial.begin(115200);
    ESP_LOGI(TAG, "RDX Synth setup");


// ----------------- Filesystems --------------------
 //   SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3);
 //   if (!SD_MMC.begin()) ESP_LOGE(TAG, "SD init failed");
    if (!LittleFS.begin()) ESP_LOGE(TAG, "LittleFS init failed");

  setupMidi() ;
  
  initControls();
  
#ifdef ENABLE_GUI
  gui.begin();
  gui.push();
#endif
   
    // ----------------- Audio -------------------------
    audio.setSampleRate(SAMPLE_RATE);
    audio.init(I2S_Audio::MODE_OUT);

    // ----------------- Synth init ---------------------
    synth.init(); 


    RDX_Patch patch;
  //  pm.begin(FS_Type::SD_MMC);
    pm.begin(FS_Type::LITTLEFS);

    if (pm.open(FS_Type::LITTLEFS, "/patches") ) {
        pm.openByIndex(25, patch); 
    } else {
        patch = synth.DigiChordPatch(); // hardcoded patch
    }
    synth.applyPatch(patch);


    // ----------------- EFFECTS -----------------------

    logMemoryStats("Before FX init");
    fx.init(SAMPLE_RATE);
    logMemoryStats("After FX init");
    fx.setSlot(0, FX_THRU);
    fx.setSlot(1, FX_THRU);


    // ----------------- Tasks -------------------------
    xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 8, &audioTaskHandle, 0);
    xTaskCreatePinnedToCore(midiTask, "midi", 4096, nullptr, 5, &midiTaskHandle, 1);
#ifdef ENABLE_GUI
    xTaskCreatePinnedToCore(gui_task, "gui", 4096, nullptr, 4, &guiTaskHandle, 1);
#endif
}

// ------------------- Loop ----------------------------
void loop() {
    logMemoryStats("After setup()");
    vTaskDelay(10);
    vTaskDelete(nullptr); // all tasks run in FreeRTOS 
}



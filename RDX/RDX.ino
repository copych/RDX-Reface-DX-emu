// RDX.ino

#pragma once
#pragma GCC optimize ("O3")
#pragma GCC optimize ("fast-math")
//#pragma GCC optimize ("unsafe-math-optimizations")
#pragma GCC optimize ("no-math-errno")

#include <Arduino.h>
#include <atomic>
#include "esp_log.h"
#include "config.h"
#include "misc.h"
int VOICES = MAX_VOICES;

#include "RDX_PresetManager.h"

#include "RDX_Synth.h"
#include "RDX_Midi.h"
#include "src/i2s/i2s_in_out.h"
#include "RDX_FX.h"
#include "RDX_CPU_Budget.h"

#include <FS.h>
#include <LittleFS.h>
//#include <SD_MMC.h>



constexpr char* TAG = "RDX";

float DRAM_ATTR outL[DMA_BUFFER_LEN];
float DRAM_ATTR outR[DMA_BUFFER_LEN];

// debug
volatile int time1 = 0, time2 = 0, synthRenderUs = 0, synthFixedUs = 0;
uint32_t audioWM = 0, midiWM = 0, guiWM = 0;
float rmsL = 0.f, rmsR = 0.f; 

RDX_Synth synth;
I2S_Audio audio;

PresetManager pm;

TaskHandle_t audioTaskHandle;
TaskHandle_t midiTaskHandle;
TaskHandle_t guiTaskHandle = nullptr;
 

static FXHost fx;
static RDX_CPUBudget cpuBudget;
// Core0 publishes only an over-budget completed block, without logging or
// waiting. Core1 atomically takes the latest event at each controller update.
// Packed as (processing microseconds << 8) | voice slots at block start.
static std::atomic<uint32_t> audioOverrunRecord{0};
static constexpr uint32_t AUDIO_TARGET_US =
    (1000000UL * DMA_BUFFER_LEN / SAMPLE_RATE) - 50UL;


#ifdef ENABLE_GUI
    #include "RDX_GUI.h"
    RDX_GUI gui;
#endif

// controls.h callbacks use fx/gui, so include it after those instances exist.
#include "controls.h"


// ------------------- Output conditioning -----------------------
// Memoryless soft clipper used before FX.
// Exactly linear through +/-0.9, then smoothly approaches +/-1.0.
// The slope is continuous at the knee, so ordinary-level material is untouched.
static inline IRAM_ATTR __attribute__((always_inline)) float softClipSample(float x) {
    constexpr float KNEE = 0.9f;
    constexpr float HEADROOM = 1.0f - KNEE;

    const float ax = fabsf(x);
    if (ax <= KNEE) return x;

    const float over = ax - KNEE;
    const float y = KNEE + HEADROOM * over / (HEADROOM + over);
    return copysignf(y, x);
}

static inline IRAM_ATTR __attribute__((always_inline)) void softClipBlock(float* left, float* right, uint32_t n) {
    for (uint32_t i = 0; i < n; ++i) {
        left[i]  = softClipSample(left[i]);
        right[i] = softClipSample(right[i]);
    }
}


// ------------------- Audio Task -----------------------
static void IRAM_ATTR audioTask(void*) {
    vTaskDelay(10);
        
    ESP_LOGI(TAG, "Starting Audio task");
    vTaskDelay(20); 
    while (true) {
        // During the program-change handoff Core1 owns synth/FX state.
        // Core0 emits a silent block and deliberately touches neither engine.
        if (synth.programChangeMuted()) {
            for (uint32_t i = 0; i < DMA_BUFFER_LEN; ++i) {
                outL[i] = 0.0f;
                outR[i] = 0.0f;
            }
            audio.writeBuffers(outL, outR);
            continue;
        }

        uint32_t start = micros();
        const uint8_t renderedSlots = (uint8_t)VOICES;

        synth.renderAudioBlock(outL, outR);
        const uint32_t renderEnd = micros();
        softClipBlock(outL, outR, DMA_BUFFER_LEN);
        const uint32_t end = micros();

        // Per-block instrumentation only: render cost versus fixed clip cost.
        // The silent render baseline is learned on Core1, not measured per voice.
        synthRenderUs = renderEnd - start;
        synthFixedUs = end - renderEnd;
        time1 = end - start; 

		fx.process(outL, outR );

        time2 = micros() - end;

        // Fade is deliberately after FX so tails/state changes cannot click.
        synth.applyProgramChangeFade(outL, outR, DMA_BUFFER_LEN);
        const uint32_t processingUs = micros() - start;
        if (processingUs >= AUDIO_TARGET_US) {
            const uint32_t bounded = processingUs > 0xFFFFFFUL ? 0xFFFFFFUL : processingUs;
            audioOverrunRecord.store((bounded << 8) | renderedSlots,
                                     std::memory_order_relaxed);
        }
        audio.writeBuffers(outL, outR);
        
    }
}


// ------------------- MIDI Task ------------------------
static void IRAM_ATTR midiTask(void*) {
    const int budgetMicros = 1e+06f * DMA_BUFFER_LEN / SAMPLE_RATE ;
    vTaskDelay(20);
    ESP_LOGI(TAG, "Starting MIDI task");
    vTaskDelay(20);
    int d = 0;
    while (true) {
        processMidi();   // incoming messages
        vTaskDelay(1);

        processControls();
        taskYIELD();

        // Patch mutation and potentially expensive FX slot reset happen only
        // after Core0 has reached the silent handoff state.
        if (synth.serviceProgramChange()) {
            fx.syncPatch();
            synth.finishProgramChange();
        }
        
        synth.flushUpdates();    // sync params to local members to speed up hot paths
        // Core1 only: timing models are patch/algorithm/parameter dependent.
        static uint32_t lastBudgetRevision = UINT32_MAX;
        const uint32_t budgetRevision = synth.budgetModelRevision();
        if (lastBudgetRevision != budgetRevision) {
            cpuBudget.resetModel();
            audioOverrunRecord.exchange(0, std::memory_order_relaxed);
            lastBudgetRevision = budgetRevision;
        }

        // Core1 only: reuse existing per-block timings. Never calculate
        // CPU budgets inside the Core0 sample or FX loops.
        const uint32_t budgetNow = millis();
        if (cpuBudget.due(budgetNow)) {
            // Consume even in mute mode so a previous patch's overrun cannot
            // affect the new patch after a program-change handoff.
            const uint32_t overrunRecord =
                audioOverrunRecord.exchange(0, std::memory_order_relaxed);
            if (!synth.programChangeMuted()) {
                cpuBudget.update(budgetNow, synthRenderUs, synthFixedUs, time2,
                                 synth.activeVoiceCount(), VOICES,
                                 synth.currentPatch().common.monoPoly == RDX_MODE_POLY,
                                 overrunRecord, synth);
            }
        }

        if (++d % 1024 == 0) {
            midiWM = uxTaskGetStackHighWaterMark(midiTaskHandle);
            audioWM = uxTaskGetStackHighWaterMark(audioTaskHandle);
            guiWM = uxTaskGetStackHighWaterMark(guiTaskHandle);
            #if 1   // --- diagnostics
                for (int i = 0; i < DMA_BUFFER_LEN; ++i) {
                    rmsL += outL[i] * outL[i];
                    rmsR += outR[i] * outR[i];
                }
                rmsL = sqrtf(rmsL / DMA_BUFFER_LEN);
                rmsR = sqrtf(rmsR / DMA_BUFFER_LEN);
            #endif
            ESP_LOGI("STATE","synth %d (render %d + fixed %d) + fx %d = %d of %d micros, voices %d active %d, est voice %.1f us, base %.1f us, RMS %f Free stack: audio %ld B midi %ld B gui %ld B", time1, synthRenderUs, synthFixedUs, time2, time1+time2, budgetMicros, VOICES, synth.activeVoiceCount(), cpuBudget.voiceCostUs(), cpuBudget.renderBaselineUs(), rmsL + rmsR, audioWM, midiWM, guiWM);
//            for (int i = 0 ; i < VOICES; ++i) {
  //              ESP_LOGI("STATE","voice %d\t active %d\t score %f" , i, synth.getVoice(i).isActive(), synth.getVoice(i).calcScore());
    //        }
        }
    }
}

#ifdef ENABLE_GUI
// ------------------- GUI Task ------------------------
static void IRAM_ATTR gui_task(void*) {
    vTaskDelay(30);
    ESP_LOGI(TAG, "Starting GUI task");
    vTaskDelay(20);
    while (true) {
        gui.draw();
        vTaskDelay(1);
    }
}
#endif

// ------------------- Setup ---------------------------
void setup() {

 //   Serial.begin(115200);
    vTaskDelay(100);
    ESP_LOGI(TAG, "RDX Synth setup");

// ----------------- Filesystems --------------------
 //   SD_MMC.setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3);
 //   if (!SD_MMC.begin()) ESP_LOGE(TAG, "SD init failed");
    if (!LittleFS.begin()) ESP_LOGE(TAG, "LittleFS init failed");

// ----------------- Midi in/out --------------------
    setupMidi() ;
    vTaskDelay(800); // USB requires this pause to stabilize

// ----------------- Controls --------------------
    initControls();
  
#ifdef ENABLE_GUI
    gui.begin();
    gui.push();
#endif


// ----------------- Audio -------------------------
    audio.setSampleRate(SAMPLE_RATE);
    audio.init(I2S_Audio::MODE_OUT);
    vTaskDelay(100);


// ----------------- Synth init ---------------------
    synth.init(); 

// ----------------- EFFECTS -----------------------

    logMemoryStats("Before FX init");
    fx.init(SAMPLE_RATE);
    logMemoryStats("After FX init");
    fx.setSlot(0, FX_THRU);
    fx.setSlot(1, FX_THRU);


    RDX_Patch patch;
  //  pm.begin(FS_Type::SD_MMC);
    pm.begin(FS_Type::LITTLEFS);

    if (pm.open(FS_Type::LITTLEFS, "/patches") ) {
        pm.openByIndex(25, patch); 
    } else {
        patch = synth.DigiChordPatch(); // hardcoded patch
    }
    synth.applyPatch(patch);


    // ----------------- Tasks -------------------------
    xTaskCreatePinnedToCore(audioTask, "audio", 4096, nullptr, 8, &audioTaskHandle, 0);
    xTaskCreatePinnedToCore(midiTask, "midi", 4096, nullptr, 5, &midiTaskHandle, 1);
#ifdef ENABLE_GUI
    xTaskCreatePinnedToCore(gui_task, "gui", 4096, nullptr, 4, &guiTaskHandle, 1);
#endif
    vTaskDelay(50);
    ESP_LOGI("main", "setup complete",0);
}

// ------------------- Loop ----------------------------
void loop() {
  vTaskDelay(25);
  logMemoryStats("After setup()");
  char res[800];
  
  vTaskDelay(25);
  vTaskList(res) ;
  ESP_LOGI("", "\r\n%s\n\n", res);

  heap_caps_print_heap_info( MALLOC_CAP_INTERNAL );
            

  vTaskDelay(30);

  ESP_LOGI("","LOOP: killing task");
  taskYIELD();


  vTaskDelete(NULL);
}









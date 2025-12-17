#pragma once
#include <stdint.h>
#include <esp_log.h>
#include <cstring> 

#include "fx_base.h"
#include "fx_reverb.h"
#include "fx_delay.h"
#include "fx_flanger.h"
#include "fx_phaser.h"
#include "fx_chorus.h"
#include "fx_touch_wah.h"
#include "fx_distortion.h"

#define FX_BLOCK_SIZE   DMA_BUFFER_LEN
#define FX_SAMPLE_RATE  SAMPLE_RATE
#define FX_SLOTS        2

// =========================================================
// Effect type enum  matches Reface DX table
// =========================================================
enum FX_ID : uint8_t {
    FX_THRU = 0,
    FX_DISTORTION,
    FX_TOUCHWAH,
    FX_CHORUS,
    FX_FLANGER,
    FX_PHASER,
    FX_DELAY,
    FX_REVERB,
    FX_COUNT
};
 


// =========================================================
// FX Host  manages two slots and static pool
// =========================================================
class FXHost {
public:
    void init(float sampleRate = FX_SAMPLE_RATE) {
        //here we assume that we have 210 kB of DRAM and 4+ MB of PSRAM
        sampleRate_ = sampleRate;

        timing[FX_THRU]  = 0; // us per block of 128 samples
        timing[FX_DISTORTION] = 34;
        timing[FX_TOUCHWAH] = 85;
        timing[FX_CHORUS] = 62;
        timing[FX_FLANGER] = 70;
        timing[FX_PHASER] = 95;
        timing[FX_DELAY] = 56;
        timing[FX_REVERB] = 152;

        uint32_t dram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
        uint32_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        szDRAM = (dram_largest - 8000) / FX_SLOTS / sizeof(float); // small buffers for scratch * 2 slots ~101kB per slot
        szPSRAM = std::min((uint32_t)sampleRate * 7, psram_largest / FX_SLOTS / sizeof(float)) ; // seconds * 2 slots 

        if (!scratchDRAM[0]) {
            scratchDRAM[0] = (float*) heap_caps_malloc(szDRAM * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            scratchDRAM[1] = (float*) heap_caps_malloc(szDRAM * sizeof(float), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        }

        if (!scratchPSRAM[0]) {
            scratchPSRAM[0] = (float*) heap_caps_malloc(szPSRAM * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            scratchPSRAM[1] = (float*) heap_caps_malloc(szPSRAM * sizeof(float), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }

        if (!scratchDRAM[1] || !scratchPSRAM[1]) {
            ESP_LOGE("FX","Unable to allocate scratch arrays DRAM: %#010x, PSRAM: %#010x",scratchDRAM[1], scratchPSRAM[1] );
            delay(100);
            while (1) {;}
        }
        // Initialize all instances
        for (int i = 0; i < FX_COUNT; ++i) {
            for (int s = 0; s < FX_SLOTS; ++s) {
                auto* fx = getInstance((FX_ID)i, s);
                fx->init(sampleRate, s);
                fx->prepare(scratchDRAM[s], szDRAM, scratchPSRAM[s], szPSRAM, sampleRate); // pass both fast and slow slot buffers, so fx could decide for themselves
                vTaskDelay(10);
            }
        }
        // Default to thru
        setSlot(0, FX_THRU);
        setSlot(1, FX_THRU);
        ESP_LOGI("FXHost", "Initialized FXHost @ %.1f Hz", sampleRate_);
    }

    inline IRAM_ATTR __attribute__((always_inline, hot)) void process(float* left, float* right ) {
        int fx_time = 0;
        for (int s = 0; s < FX_SLOTS; ++s) {
            if (fx_[s] != common_.effects[s][0]) setSlot(s, (FX_ID)common_.effects[s][0]);
            if (slots_[s]) slots_[s]->processBlock(left, right, FX_BLOCK_SIZE  );
            fx_time += timing[(FX_ID)common_.effects[s][0]];
        }
        if (common_.monoPoly == RDX_MODE_POLY) {
            VOICES = (2850 - fx_time) / 340; // 340ms per voice , polyphony estimation
        } else {
            VOICES = 1;
        }
    }

    virtual void prepare(float* buf, uint32_t len, float sampleRate) { (void)buf; (void)len; (void)sampleRate; }

    inline void resetSlot(uint8_t slot) {
        if (slot < FX_SLOTS) {
            memset(scratchPSRAM[slot], 0, szPSRAM * sizeof(float));
            memset(scratchDRAM[slot], 0, szDRAM * sizeof(float));
        }
    }

    inline void setSlot(uint8_t slot, FX_ID id) {
        if (slot >= FX_SLOTS || id >= FX_COUNT) return;
        resetSlot(slot);
        slots_[slot] = getInstance(id, slot);
        slots_[slot]->enable(false);
        slots_[slot]->reset();
        slots_[slot]->enable(true);
        fx_[slot] = id;
        ESP_LOGI("FXHost", "Slot %d -> FX %d", slot, id);
    }

    inline FXBase* getSlot(uint8_t slot) { return slots_[slot]; }

private:

    int szDRAM = 0;
    int szPSRAM = 0; 
    float* scratchDRAM[2] = {nullptr, nullptr};
    float* scratchPSRAM[2] = {nullptr, nullptr};

    RDX_Common& common_ = RDX_State::getState().workingPatch.common ;
    float sampleRate_ = FX_SAMPLE_RATE;
    uint8_t fx_[2]={0,0};
    FXBase* slots_[FX_SLOTS] = {nullptr, nullptr};

    // Two copies per effect type  -> static pool
    FxThru      thru_[FX_SLOTS];
    FxDistortion distortion_[FX_SLOTS];
    FxTouchWah  touchwah_[FX_SLOTS];
    FxChorus    chorus_[FX_SLOTS];
    FxFlanger   flanger_[FX_SLOTS];
    FxPhaser    phaser_[FX_SLOTS];
    FxDelay     delay_[FX_SLOTS];
    FxReverb    reverb_[FX_SLOTS];

    int timing[FX_COUNT] = {0} ;



    inline FXBase* getInstance(FX_ID id, uint8_t slot) {
        switch (id) {
            case FX_THRU:       return &thru_[slot];
            case FX_DISTORTION: return &distortion_[slot];
            case FX_TOUCHWAH:   return &touchwah_[slot];
            case FX_CHORUS:     return &chorus_[slot];
            case FX_FLANGER:    return &flanger_[slot];
            case FX_PHASER:     return &phaser_[slot];
            case FX_DELAY:      return &delay_[slot];
            case FX_REVERB:     return &reverb_[slot];
            default:            return &thru_[slot];
        }
    }
};

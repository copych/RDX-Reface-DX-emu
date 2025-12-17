#pragma once
#include <stdint.h>
#include <stddef.h>

class  FXBase {
public:
    virtual ~FXBase() {}
    virtual void init(float sampleRate, uint8_t slot) {
        sampleRate_ = sampleRate; 
        slotId_ = slot; 
    }
    virtual inline void reset() { 
        if (prepared_) {            
            enabled_ = true;
        } 
    }
    virtual inline void IRAM_ATTR __attribute__((always_inline)) processBlock(float* left, float* right, uint32_t n) = 0;
    virtual void setParam(uint8_t idx, float value) { (void)idx; (void)value; }
    virtual bool prepare(float* scratchFast, uint32_t fastSize, float* scratchSlow, uint32_t slowSize, int sampleRate) {
        (void)scratchFast;
        (void)fastSize;
        (void)scratchSlow;
        (void)slowSize;
        (void)sampleRate;
        return true;
    }
    inline void enable(bool s) { enabled_ = s; }
    inline bool enabled() const { return enabled_; }

protected:
    bool enabled_ = false;
    bool prepared_ = false;
    float sampleRate_ = (float)SAMPLE_RATE;
    uint8_t slotId_ = 0;
};


class FxThru : public FXBase {
public:
    inline void processBlock(float* l, float* r, uint32_t n) override {
        // pass thru
    }

};
 
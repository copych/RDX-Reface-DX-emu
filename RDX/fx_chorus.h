/*
 * FxChorus - Chorus audio effect
 * Based on a basic understanding of the chorus effect
 *
 * May 2025
 * Author: Evgeny Aslovskiy AKA Copych
 * License: MIT
 */

#pragma once
#include "config.h"
#include "RDX_Constants.h"
#include "fx_base.h"

class  FxChorus : public FXBase {
public:
    FxChorus() = default;

    virtual bool prepare(float* scratchFast, uint32_t fastSize, float* scratchSlow, uint32_t slowSize, int sampleRate) {
        sampleRate_ = sampleRate;
        writeIndex_ = 0;
        lfoPhase_ = 0.0f;

        if (fastSize < MAX_DELAY * 2) return false;

        bufferL_ = scratchFast;
        bufferR_ = scratchFast + MAX_DELAY;

        std::memset(bufferL_, 0, sizeof(float) * MAX_DELAY);
        std::memset(bufferR_, 0, sizeof(float) * MAX_DELAY);

        setLfoFreq(0.5f);
        setDepth(0.025f);
        setBaseDelay(0.03f);

        prepared_ = true;
        ESP_LOGI("CHO", "prepared slot %d | FAST=%d floats (%.1f kB)  ",        slotId_, MAX_DELAY * 2, MAX_DELAY * 2 * 4 / 1024.0f );
        return true;
    }

    inline void reset() override {
        if (prepared_) {
            writeIndex_ = 0;
            lfoPhase_ = 0.0f;
            setLfoFreq(0.5f);
            setDepth(0.025f);
            setBaseDelay(0.03f);
            enabled_ = true;
        }
    }

    inline void processBlock(float* left, float* right, uint32_t frames) override {
        if (unlikely(!prepared_)) return;
 
        const uint8_t depthParam = st.effects[slotId_][1];
        const uint8_t rateParam  = st.effects[slotId_][2];

        // simple parameter mapping
        // 5–25 ms typical modulation depth
        setDepth(depthParam); // 0.5–5% delay mod depth
        //setLfoFreq(LFO_SPEED[rateParam]);     // 0.1–5 Hz
        setRate(rateParam); // checks if it changes

        for (uint32_t n = 0; n < frames; ++n) {
            float offset = sin01(lfoPhase_) * depthMul_;
            lfoPhase_ += lfoFreq_ * DIV_SAMPLE_RATE;
            if (lfoPhase_ >= 1.0f) lfoPhase_ -= 1.0f;
            
            const float delayOffsetL = baseDelayMul_ + offset;
            const float delayOffsetR = baseDelayMul_ - offset;

            const float delayedL = getInterpolatedSample(bufferL_, writeIndex_ - delayOffsetL);
            const float delayedR = getInterpolatedSample(bufferR_, writeIndex_ - delayOffsetR);

            bufferL_[writeIndex_] = left[n];
            bufferR_[writeIndex_] = right[n];

            left[n]  += delayedL * WET_DRY_MIX;
            right[n] += delayedR * WET_DRY_MIX;

            if (++writeIndex_ >= MAX_DELAY) writeIndex_ = 0;
        }
    }
    
    inline void setRate(uint8_t r) {
        if (rate_ == r) return;
        
        rate_ = r;
        setLfoFreq(LFO_SPEED[r] * 0.2436f);
        
    }
    
    inline void setLfoFreq(float freq) { 
        lfoFreq_ = freq; 
        ESP_LOGI("CHO","freq %f", freq);
    }

    inline void setDepth(uint8_t d) {
        if (d == dep_) return;
        
        dep_ = d;
        depth_ = PM_DEPTH[d] * (MAX_DEPTH - MIN_DEPTH) + MIN_DEPTH;
        depthMul_ = depth_ * sampleRate_;
    }

    inline void setBaseDelay(float delay) { 
        baseDelay_ = delay; 
        baseDelayMul_ = delay * sampleRate_; 
    }

private:
    RDX_Common& st = RDX_State::getState().workingPatch.common ;
    inline IRAM_ATTR float getInterpolatedSample(float* buf, float index) const {
        while (index < 0) index += MAX_DELAY;
        const int idx = (int)index;
        const float frac = index - idx;
        int idxNext = (idx + 1) ;
        if (idxNext >= MAX_DELAY) {idxNext -= MAX_DELAY;}
        return (1.0f - frac) * buf[idx] + frac * buf[idxNext];
    }

    // Constants
    static constexpr int MAX_DELAY = 4096; 
    static constexpr float WET_DRY_MIX = 0.25f;
    static constexpr float MAX_DEPTH = 0.005;
    static constexpr float MIN_DEPTH = 0.0005;
    float* bufferL_ = nullptr;
    float* bufferR_ = nullptr;

    float baseDelayMul_ = 0.0f;
    uint8_t dep_ = 0;
    float depth_ = 0.0015f;
    uint8_t rate_ = 0; 
    float depthMul_ = 0.0f;
    int writeIndex_ = 0;
    float lfoPhase_ = 0.0f;
    float lfoFreq_ = 0.5f;
    float baseDelay_ = 0.03f;
};

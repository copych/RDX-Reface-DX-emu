// fx_reverb_simple.h
#pragma once

#include "fx_base.h"
#include "esp32-hal.h"
#include <cmath>
#include <cstring>

constexpr int NUM_COMBS = 4;
constexpr int NUM_ALLPASSES = 2;

static const float comb_lengths_ms[NUM_COMBS] = {47.3f, 53.7f, 71.1f, 89.5f};
static const float allpass_lengths_ms[NUM_ALLPASSES] = {9.7f, 3.2f};

class FxReverb : public FXBase {
public:
    FxReverb() = default;

    bool prepare(float* scratchFast, uint32_t fastSize,
                 float* scratchSlow, uint32_t slowSize,
                 int sampleRate) override
    {
        (void)scratchSlow;
        (void)slowSize;

        prepared_ = false;
        sampleRate_ = sampleRate;

        if (!scratchFast || fastSize == 0) {
            ESP_LOGE("Reverb", "slot %d: no DRAM scratch", slotId_);
            return false;
        }

        // Work out the preferred topology first. fastSize is a hard memory
        // budget: prepare() must never write beyond the supplied scratch area.
        uint32_t nominalTotal = 0;

        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < NUM_COMBS; ++i) {
                nominalCombSize_[ch][i] =
                    int((comb_lengths_ms[i] / 1000.f) * sampleRate_) + ch * 17;
                nominalTotal += nominalCombSize_[ch][i];
            }

            for (int i = 0; i < NUM_ALLPASSES; ++i) {
                nominalAllSize_[ch][i] =
                    int((allpass_lengths_ms[i] / 1000.f) * sampleRate_) + i + ch;
                nominalTotal += nominalAllSize_[ch][i];
            }
        }

        float sizeScale = 1.0f;
        if (fastSize < nominalTotal) {
            sizeScale = (float)fastSize / (float)nominalTotal;
        }

        // Below this point the topology becomes too short to remain useful.
        static constexpr float MIN_SIZE_SCALE = 0.35f;
        if (sizeScale < MIN_SIZE_SCALE) {
            ESP_LOGE("Reverb",
                     "slot %d: insufficient DRAM scratch: %.1f kB available, "
                     "%.1f kB nominal, scale %.3f < %.2f",
                     slotId_,
                     fastSize * sizeof(float) / 1024.0f,
                     nominalTotal * sizeof(float) / 1024.0f,
                     sizeScale, MIN_SIZE_SCALE);
            return false;
        }

        // Build the actual delay layout from the budget. floor() via integer
        // conversion guarantees that the sum cannot exceed fastSize.
        uint32_t used = 0;

        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < NUM_COMBS; ++i) {
                int len = (int)(nominalCombSize_[ch][i] * sizeScale);
                if (len < 1) len = 1;
                combSize_[ch][i] = len;
                used += len;
            }

            for (int i = 0; i < NUM_ALLPASSES; ++i) {
                int len = (int)(nominalAllSize_[ch][i] * sizeScale);
                if (len < 1) len = 1;
                allSize_[ch][i] = len;
                used += len;
            }
        }

        // Defensive guard against future topology changes or rounding changes.
        if (used > fastSize) {
            ESP_LOGE("Reverb",
                     "slot %d: calculated layout exceeds scratch: %u > %u floats",
                     slotId_, (unsigned)used, (unsigned)fastSize);
            return false;
        }

        float* ptr = scratchFast;

        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < NUM_COMBS; ++i) {
                const int len = combSize_[ch][i];
                combBuf_[ch][i] = ptr;
                ptr += len;
                std::memset(combBuf_[ch][i], 0, len * sizeof(float));
                combIdx_[ch][i] = 0;
                combLPF_[ch][i] = 0.f;
            }

            for (int i = 0; i < NUM_ALLPASSES; ++i) {
                const int len = allSize_[ch][i];
                allBuf_[ch][i] = ptr;
                ptr += len;
                std::memset(allBuf_[ch][i], 0, len * sizeof(float));
                allIdx_[ch][i] = 0;
            }
        }

        ESP_LOGI("Reverb",
                 "prepared slot %d: %.1f kB DRAM used / %.1f kB available, "
                 "nominal %.1f kB, scale %.3f",
                 slotId_,
                 used * sizeof(float) / 1024.0f,
                 fastSize * sizeof(float) / 1024.0f,
                 nominalTotal * sizeof(float) / 1024.0f,
                 sizeScale);

        prepared_ = true;
        return true;
    }

    inline void reset() override {
        if (!prepared_) return;
        damping_ = 0.3f;
        lastTime_ = -1.f;
        prev_in = 0.0f;
        prev_out = 0.0f;
    }

    inline void processBlock(float* L, float* R, uint32_t n) override {
        if (!prepared_) return;

        float depth = st.effects[slotId_][1] / 127.0f * 0.2f;
        float time  = st.effects[slotId_][2] / 127.0f;
        updateFeedback(time);

        for (uint32_t i=0; i<n; ++i) {
            float in = 0.5f * (L[i] + R[i]);
            prev_out = in - prev_in + DcTimeConst * prev_out;
            prev_in = in;
            float wetL = processCh(0, prev_out);
            float wetR = processCh(1, prev_out);
            L[i] = (1.0f - depth) * L[i] + depth * wetL;
            R[i] = (1.0f - depth) * R[i] + depth * wetR;
        }
    }

private:
    RDX_Common& st = RDX_State::getState().workingPatch.common;
    float* combBuf_[2][NUM_COMBS];
    int combSize_[2][NUM_COMBS];
    int nominalCombSize_[2][NUM_COMBS];
    int combIdx_[2][NUM_COMBS];
    float combLPF_[2][NUM_COMBS];
    float combGain_[NUM_COMBS];

    float* allBuf_[2][NUM_ALLPASSES];
    int allSize_[2][NUM_ALLPASSES];
    int nominalAllSize_[2][NUM_ALLPASSES];
    int allIdx_[2][NUM_ALLPASSES];
    float allGain_[NUM_ALLPASSES] = {0.7f, 0.7f};

    float damping_ = 0.3f;
    float lastTime_ = -1.f;

    // DC blocking
    float prev_in = 0.0f;
    float prev_out = 0.0f;
    float DcTimeConst = 0.996f; // ~15-20 Hz, float R = 1.0f - 2.0f * M_PI * fc / sampleRate; // fc in Hz

    inline void updateFeedback(float t) {
        if (fabsf(t - lastTime_) < 1e-4f) return;
        float rt60 = 0.25f * powf(24.f, t); // 0.25–6 s range
        for (int i=0; i<NUM_COMBS; ++i) {
            float delaySec = combSize_[0][i] * DIV_SAMPLE_RATE;
            float g = powf(10.0f, -3.0f * delaySec / rt60);
            combGain_[i] = fminf(g, 0.95f);
        }
        lastTime_ = t;
    }

    inline float processCh(int ch, float in) {
        float sum = 0.f;
        for (int i=0; i<NUM_COMBS; ++i) sum += processComb(ch, i, in);
        sum *= 0.25f;
        for (int i=0; i<NUM_ALLPASSES; ++i) sum = processAll(ch, i, sum);
        return sum;
    }

    inline float processComb(int ch, int i, float x) {
        float* buf = combBuf_[ch][i];
        int& idx = combIdx_[ch][i];
        int N = combSize_[ch][i];
        float y = buf[idx];
        combLPF_[ch][i] = y + damping_ * (combLPF_[ch][i] - y);
        buf[idx] = x + combLPF_[ch][i] * combGain_[i];
        if (++idx >= N) { idx = 0; }
        return y;
    }

    inline float processAll(int ch, int i, float x) {
        float* buf = allBuf_[ch][i];
        int& idx = allIdx_[ch][i];
        int N = allSize_[ch][i];
        float g = allGain_[i];
        float y = buf[idx];
        buf[idx] = x + y * g;
        float out = y - g * x;
        if (++idx >= N) { idx = 0; }
        return out;
    }
};

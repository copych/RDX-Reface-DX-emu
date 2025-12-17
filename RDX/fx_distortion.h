#pragma once
#include "misc.h"
#include "fx_base.h"

class FxDistortion : public FXBase {
public:
    void init(float sampleRate, uint8_t slot) override {
        FXBase::init(sampleRate, slot);
        setDrive(0.5f);
        setTone(0.5f);
        reset();
    }

    inline void reset() override {
        if (!prepared_) return;
        lpL_ = lpR_ = 0.f;
        makeupGain_ = 1.f;
        tone_ = 0.5f;
        toneCoef_ = 0.05f;
        driveParam_ = 0.5f;
        driveGain_  = 1.f;
    }

    inline void setDrive(float dr) {
        if (dr == driveParam_) return;
        driveParam_ = dr;

        // perceptually uniform gain (approx)
        float g = 1.0f + 24.0f * driveParam_ * driveParam_; // pre-gain
        driveGain_  = g;

        // normalize makeup gain (inverse of avg gain at medium drive)
        // mild compensation so perceived loudness stays stable
        makeupGain_ = 0.4f / (1.0f + 0.25f * driveParam_ * driveParam_);
    }

    inline void setTone(float t) {
        if (t == tone_) return;
        tone_ = t;
        calcToneCoeff();
    }

    inline bool prepare(float*, uint32_t, float*, uint32_t, int sampleRate) override {
        sampleRate_ = sampleRate;
        calcToneCoeff();
        prepared_ = true;
        return prepared_;
    }

    inline void processBlock(float* l, float* r, uint32_t n) override {
        if (!enabled_) return;

        setDrive(st.effects[slotId_][1] / 127.0f);
        setTone(st.effects[slotId_][2] / 127.0f);

        const float dg = driveGain_;
        const float mg = makeupGain_;
        const float a  = toneCoef_;

        for (uint32_t i = 0; i < n; ++i) {
            float xL = (1.0f-dryMix_) * saturate_cubic(l[i] * dg) + dryMix_ * l[i];
            float xR = (1.0f-dryMix_) * saturate_cubic(r[i] * dg) + dryMix_ * r[i];

            // simple 1-pole LP tone
            lpL_ += a * (xL - lpL_);
            lpR_ += a * (xR - lpR_);

            l[i] = lpL_ * mg;
            r[i] = lpR_ * mg;
        }
    }

private:
    RDX_Common& st = RDX_State::getState().workingPatch.common;
    float driveParam_ = 0.5f;
    float driveGain_  = 1.f;
    float makeupGain_ = 1.f;
    float tone_ = 0.5f;
    float toneCoef_ = 0.05f;
    float lpL_ = 0.f, lpR_ = 0.f;
    float dryMix_ = 0.5f;

    inline void calcToneCoeff() {
        // Map tone [0..1] → fc [300..8000 Hz]
        float fc = 300.f + tone_ * (8000.f - 300.f);
        float x = expf(-2.f * M_PI * fc * DIV_SAMPLE_RATE);
        toneCoef_ = 1.f - x;
    }
};

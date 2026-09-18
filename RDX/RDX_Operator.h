// RDX_Operator.h
#pragma once

#include "misc.h"
#include "RDX_Types.h"
#include "RDX_State.h"
#include "RDX_Envelope.h"
#include "RDX_Constants.h" // provides rdxGain(), RDX_GAIN[], sinTable[], sin01()


// ===============================
// RDX Operator (float-domain, uses RDX_GAIN)
// ===============================
class  IRAM_ATTR __attribute__((always_inline)) RDX_Operator {
public:
    RDX_Operator(int idx)
        : idx_(idx),
          params_(RDX_State::getState().workingPatch.ops[idx]) {}

    inline void setParams(int note, int vel, float baseHz) {
        note_ = (uint8_t)note;
        baseHz_ = baseHz;

        recalcPhaseInc();

        recalcScaling();
        velogain_ = velocityGain( vel, params_.velSens, 1.08f);
        
        // Cache OUT LEVEL gain and feedback scale/sign to avoid per-sample table lookups
        outGain_  = rdxGain(params_.outLevel * velogain_ ) * scaling_;
        ESP_LOGD("OP", "%d: scaling %f out %f (op level %d velo %d)", idx_, scaling_, outGain_, params_.outLevel, vel ) ;
        env_.initAEG(params_.egRate, params_.egLevel, true);

        fbRectify_ = (params_.fbType != RDX_FB_SAW) ; 
        fbScale_  = FEEDBACK_K[params_.feedback]   ; 
        enabled_ = params_.enable;
    }

    inline void updateParams() {
        recalcPhaseInc();
        recalcScaling();
        outGain_  = rdxGain(params_.outLevel * velogain_ ) * scaling_;
        env_.initAEG(params_.egRate, params_.egLevel, false);
        fbRectify_ = (params_.fbType != RDX_FB_SAW) ; 
        fbScale_  = FEEDBACK_K[params_.feedback]   ; 
        enabled_ = params_.enable;
    }

    inline RDX_OpParams& params() { return params_; }
    inline const RDX_OpParams& params() const { return params_; }

    inline void reset() {
        phase_    = 0.0f;
        fbAcc_   = 0.0f;
        fbFilter_ = 0.f; 
        env_.reset();
    }

    inline void gate(bool on) {
        env_.gate(on);
    }


    inline IRAM_ATTR __attribute__((always_inline, hot)) float compute(float inputPhaseOffset, float phaseModSemitones = 0.f) {
        if (!params_.enable) return 0.f;

        // Optional rectification 
        if (fbRectify_  && fbAcc_ < 0.f) fbAcc_ = -fbAcc_;
    
        // Lowpass filter the feedback path
        fbFilter_ += fbLpCoef_ * (fbAcc_ - fbFilter_);  // 1-pole IIR

        // Lookup phase = base + inbus offset + filtered feedback + optional jitter
        float lookupPhase = wrap01(phase_ + inputPhaseOffset + fbFilter_ * fbScale_ );

        // Advance own oscillator phase by note + PEG/LFO modulation
        phase_ = (phase_ + phaseInc_ * semitonesToRatio(phaseModSemitones));
        if(phase_>1.0f) phase_ -= 1.0f;

        // Sine lookup and feedback state update
        fbAcc_  = sin01(lookupPhase);

        // Apply gain + AEG
        return fbAcc_ * outGain_ * env_.processAEG();
    }

    inline void updateGain() {
        outGain_ = rdxGain(params_.outLevel * velogain_) * scaling_;
    }

    inline void updateScaling() {
        recalcScaling();
        updateGain();
    }

    inline void updateFeedback() {
        fbRectify_ = (params_.fbType != RDX_FB_SAW);
        fbScale_   = FEEDBACK_K[params_.feedback];
    }
    
    inline void recalcPhaseInc(){
        float freqHz = 0.0f;
		if (params_.freqMode == 0) {   
            freqHz = baseHz_ * ctl_.freqCoef[idx_]; 
		} else {  
			freqHz = ctl_.freqCoef[idx_]; 
		}
		phaseInc_ = freqHz * ctl_.detuneCoef[idx_] * DIV_SAMPLE_RATE;
    }


    inline bool isActive() const { return env_.isActive(); }
    inline float getEnvLevel() const { return env_.getLevel(); }

private:
    RDX_OpParams& params_;
    RDX_Envelope env_;
    RDX_Controls& ctl_ = RDX_State::getState().controls;

    float scaling_ = 1.0f;
    float velogain_ = 1.0f;
    bool  enabled_ = true;
    float fbFilter_ = 0.f;   // LPF state
    float fbLpCoef_ = 0.356f;  // tweak 0.05–0.3 for smoother/rougher harmonics
    float baseHz_ = 440.0f;
    uint8_t note_ = 60;

    int idx_ = 0;
    float phase_     = 0.0f;   // normalized [0..1)
    float phaseInc_  = 0.0f;   // per-sample increment
    float fbAcc_     = 0.0f;   // last output for feedback  
    // cached precomputes
    float outGain_   = 1.0f;   // rdxGain(outLevel)
    float fbScale_   = 0.0f;   // feedback scaled coeff
    bool  fbRectify_ = false;   // true for squarish, false for sawish

    static constexpr float KSC_MAX_LEVEL_SHIFT = 63.0f;
    static constexpr float KSC_EXP_CURVE = 1.0f;
    static constexpr float KSC_REF_LEVEL = 127.0f;
    static constexpr float KSC_REF_GAIN = levelLUT.forward[127];

    inline void recalcScaling() {
        scaling_ = calcScalingFactor(
            note_,
            params_.scaleLD, (RDX_ScaleCurve)params_.scaleLC,
            params_.scaleRD, (RDX_ScaleCurve)params_.scaleRC
        );
    }

    inline IRAM_ATTR __attribute__((always_inline)) float calcScalingFactor(
        uint8_t note,
        uint8_t lDepth, RDX_ScaleCurve lCurve,
        uint8_t rDepth, RDX_ScaleCurve rCurve
    ) {
        constexpr uint8_t BP = 60;
        constexpr float INV_LEFT_RANGE = 1.0f / 60.0f;
        constexpr float INV_RIGHT_RANGE = 1.0f / 67.0f;
        constexpr float INV_127 = 1.0f / 127.0f;

        uint8_t depth;
        RDX_ScaleCurve curve;
        float distance;

        if (note < BP) {
            depth = lDepth;
            curve = lCurve;
            distance = (float)(BP - note) * INV_LEFT_RANGE;
        } else if (note > BP) {
            depth = rDepth;
            curve = rCurve;
            distance = (float)(note - BP) * INV_RIGHT_RANGE;
        } else {
            return 1.0f;
        }

        if (depth == 0) return 1.0f;

        const bool isExp =
            curve == RDX_SCALE_NEG_EXP ||
            curve == RDX_SCALE_POS_EXP;

        const bool isNegative =
            curve == RDX_SCALE_NEG_LIN ||
            curve == RDX_SCALE_NEG_EXP;

        // EXP geometry is a cheap blend between linear x and x^2.
        // KSC_EXP_CURVE = 0 -> linear, 1 -> square.
        float curveK = distance;
        if (isExp) {
            curveK += KSC_EXP_CURVE * (distance * distance - distance);
        }

        const float depthK = (float)depth * INV_127;
        float levelShift = depthK * curveK * KSC_MAX_LEVEL_SHIFT;
        if (isNegative) levelShift = -levelShift;

        // Reuse the existing extended operator-level LUT.  With the default
        // +/-63 shift the lookup range is 64..190, safely inside 0..191.
        const float shiftedLevel = KSC_REF_LEVEL + levelShift;
        return rdxGain(shiftedLevel) / KSC_REF_GAIN;
    }


 

    inline IRAM_ATTR __attribute__((always_inline)) float velocityGain(uint8_t vel, uint8_t sens, float max_out = 1.1f) {
        float normSens = sens / 127.0f;
        float factor = (1.0f - normSens) + VELO_SENS[vel] * normSens;
        return max_out * factor;
    }


};

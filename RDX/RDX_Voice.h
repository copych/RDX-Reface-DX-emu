// RDX_Voice.h
#pragma once
#include <Arduino.h>
#include <cmath>
#include "RDX_Types.h"
#include "RDX_Constants.h"
#include "RDX_Operator.h"
#include "RDX_Envelope.h"
#include "RDX_PEG.h"
#include "RDX_State.h"
#include "RDX_LFO.h"


class  RDX_Voice {
public:
    inline void init() {
    
        for (auto& op : ops_) {
            op.reset();
        }
        peg_.reset();
    }

inline void noteOn(uint8_t note, uint8_t vel) {
 //   ctl_.pushNote(note);
    note_ = note;
    velocity_ = vel;
    active_ = true;

    const float noteTarget = float(note) + patch_.common.transpose - 64.f;
    const bool monoMode   = (patch_.common.monoPoly != RDX_MODE_POLY);
    const bool monoFull   = (patch_.common.monoPoly == RDX_MODE_MONO_FULL);
    const bool overlapping = gate_;  // another note held

    bool doRetrig = false;
    bool doGlide  = false;

    if (monoMode) {
        if (overlapping) {
            // both mono modes glide on overlap
            doGlide  = monoMode;
            doRetrig = false;
        } else {
            // new note after silence
            doGlide  = monoFull  ; // only full glides always
            doRetrig = true ;
        }
    } else {
        // polyphonic
        doRetrig = true;
        doGlide  = false;
    }

    // --- Portamento setup ---
    if (doGlide ) {
        portamentoStartNote_  = currentNoteSemitone_;
        portamentoTargetNote_ = noteTarget;
        portamentoPos_        = 0.f;
        portamentoInc_        = 1.f / (ctl_.portaTimeS * (float)sampleRate_);
    } else {
        portamentoStartNote_  = noteTarget;
        portamentoTargetNote_ = noteTarget;
        portamentoPos_        = 1.f;
        portamentoInc_        = 0.f;
    }

    if (monoFull && !overlapping && patch_.common.portaTime == 0 ) {
        // full mono, isolated note, zero portamento → hard phase reset
        portamentoStartNote_  = noteTarget;
        portamentoTargetNote_ = noteTarget;
        portamentoPos_        = 1.f;
        portamentoInc_        = 0.f;

        noteOnBaseNote_       = noteTarget;
        currentNoteSemitone_  = noteTarget;

        // reset phase of all operators (avoid click)
        for (int i = 0; i < 4; ++i)
            ops_[i].reset();
    }

    // --- Envelope retrigger ---
    if (doRetrig) {
        noteOnBaseNote_      = noteTarget;
        currentNoteSemitone_ = noteTarget;

        peg_.initPEG(patch_.common.pegRate, patch_.common.pegLevel);
        syncLFO();
        peg_.gate(true);

        const float baseHz = midiNoteToHz(noteTarget);
        for (int i = 0; i < 4; ++i) {
            ops_[i].reset();
            ops_[i].setParams(noteTarget, vel, baseHz);
            ops_[i].gate(true);
        }
    }

    gate_ = true;
    sustained_ = false;  // reset on key press
    justAllocated_ = false;
}

inline void setJustAllocated() { justAllocated_ = true; }

inline void noteOff() {
    gate_ = false;       // key released

    if (ctl_.sustain && !gate_) {
        // defer shutdown if pedal is down
        sustained_ = true;
        return;
    }

    for (auto& op : ops_) op.gate(false);
    peg_.gate(false);
    active_ = false;
    sustained_ = false;
}

inline IRAM_ATTR __attribute__((always_inline)) void updateMods() {
    float peg_value = peg_.processPEG();

    // --- Portamento ---
    if (portamentoPos_ < 1.f) {
        portamentoPos_ += portamentoInc_;
        if (portamentoPos_ >= 1.f) {
            portamentoPos_ = 1.f;
            currentNoteSemitone_ = portamentoTargetNote_;
        }
    }

    const float currentNote = portamentoStartNote_ +
                              (portamentoTargetNote_ - portamentoStartNote_) * portamentoPos_;
    const float portaOffsetSemitones = currentNote - noteOnBaseNote_;

    // --- LFO + mod sources ---
    lfoValue_ += lfoIncrement_;
    const float modWheelLfo = lfoValue_ * ctl_.modWheelFactor;
    const float pitchBend = ctl_.pitchbendSemitones;
    const float pmMult = lfoValue_ * pmDepth_;
    //const float lfoNorm = lfoValue_ * MIDI_NORM;
    for (int i = 0; i < 4; ++i) {
        float phaseMod = 0.f;
        phaseMod += peg_value * pegEnable_[i];
        phaseMod += pmMult * lfoPMDEnable_[i];
        phaseMod += modWheelLfo;
        phaseMod_[i] = phaseMod + pitchBend + portaOffsetSemitones;

        if (lfoAMD_[i] > 0) {
            ampMod_[i] = 1.0f + AM_DEPTH[lfoAMD_[i]] * (lfoValue_*2.0f - 1.0f) - modWheelLfo;
            fclamp(ampMod_[i], 0.f, 1.f);
        } else {
            ampMod_[i] = 1.0f;
        }
    }
}

    

	inline bool isActive() const {
        switch(algorithm_) {
            case 5:
            case 6:
                return ops_[0].isActive() || ops_[1].isActive() ;
            case 7:
                return ops_[0].isActive() || ops_[2].isActive() ;
            case 8:
            case 9:
            case 10:
                return ops_[0].isActive() || ops_[1].isActive() || ops_[2].isActive() ;
            case 11:
                return ops_[0].isActive() || ops_[1].isActive() || ops_[2].isActive() || ops_[3].isActive() ;
            default:
                return ops_[0].isActive();
        }
    }


	inline float ampScore() const {
        // provide sum of carrier envelope levels
        switch(algorithm_) {
            case 5:
            case 6:
                return ONE_DIV_SQRT2 * (ops_[0].getEnvLevel() + ops_[1].getEnvLevel()) ;
            case 7:
                return ONE_DIV_SQRT2 * (ops_[0].getEnvLevel() + ops_[2].getEnvLevel()) ;
            case 8:
            case 9:
            case 10:
                return ONE_DIV_SQRT3 * (ops_[0].getEnvLevel() + ops_[1].getEnvLevel() + ops_[2].getEnvLevel()) ;
            case 11:
                return 0.5f * (ops_[0].getEnvLevel() + ops_[1].getEnvLevel() + ops_[2].getEnvLevel() + ops_[3].getEnvLevel()) ;
            default:
                return ops_[0].getEnvLevel() ;
        }
    }


    inline uint8_t note() const { return note_; }


    inline void syncLFO() {
        lfo_.init(patch_.common.lfoSpeed, patch_.common.lfoDelay, (RDX_LFO::Waveform)patch_.common.lfoWave);
        algorithm_ = patch_.common.algorithm;
    }


    inline  IRAM_ATTR __attribute__((always_inline)) void  updateLfo() {
        lfo_.updateState();          // advance once per block
        lfoValue_ = lfo_.getValue();      // cache start-of-block value
        lfoIncrement_ = lfo_.getIncrement(); // cache per-sample increment
    }


    inline float calcScore() {
        if ( !isActive() ) {
            return score_ = 0.0f;
        }
        if ( justAllocated_ ) {
            return score_ = 1e8f;
        }

        score_ = ampScore() * (float)velocity_ ;

        if (!gate_ && !sustained_) {
            score_ *= 0.1f;
        }
        
        return score_;
    }


    inline IRAM_ATTR __attribute__((always_inline, hot)) float step() {
     //   if (!ops_[0].isActive()) {return 0.0f  ;}
        updateMods();
        switch(algorithm_) {
            case 0: // 4->3->2->1
                return (ampMod_[0] * ops_[0].compute(
                            ampMod_[1] * ops_[1].compute(
                                ampMod_[2] * ops_[2].compute(
                                    ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]) , phaseMod_[2]
                                ) , phaseMod_[1]
                            ) , phaseMod_[0]
                        ));

            case 1: // (4+3)->2->1
                return (ampMod_[0] * ops_[0].compute(
                            ampMod_[1] * ops_[1].compute(
                                ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]) +
                                ampMod_[2] * ops_[2].compute(0.0f, phaseMod_[2]),
                                phaseMod_[1]
                            ) , phaseMod_[0]
                        ));

            case 2: // 3->2 ; (2+4)->1
                return (ampMod_[0] * ops_[0].compute(
                          ampMod_[1] * ops_[1].compute(
                            ampMod_[2] * ops_[2].compute(0.0f, phaseMod_[2]) , phaseMod_[1] 
                          )
                        + ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]) 
                        , phaseMod_[0]
                        ));

            case 3: { // 4->(2,3) ; (2+3)->1
                const float m4 = ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]);
                return (ampMod_[0] * ops_[0].compute(
                            ampMod_[1] * ops_[1].compute(m4 , phaseMod_[1]) +
                            ampMod_[2] * ops_[2].compute(m4 , phaseMod_[2]),
                            phaseMod_[0]
                        ));
            }

            case 4: // (2+3+4)->1
                return (ampMod_[0] * ops_[0].compute(
                            ampMod_[1] * ops_[1].compute(0.0f, phaseMod_[1]) +
                            ampMod_[2] * ops_[2].compute(0.0f, phaseMod_[2]) +
                            ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]),
                            phaseMod_[0]
                        ));

            case 5: // 4->3->2 ; 1||2
                return ((ampMod_[0] * ops_[0].compute(0.0f, phaseMod_[0]) +
                        ampMod_[1] * ops_[1].compute(
                            ampMod_[2] * ops_[2].compute(
                                ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]) , phaseMod_[2]
                            ) , phaseMod_[1]
                        )));

            case 6: { // 4->3 ; 3->(2,1) ; 1||2
                const float m3 = ampMod_[2] * ops_[2].compute(
                                    ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]) , phaseMod_[2]);
                return ((ampMod_[0] * ops_[0].compute(m3 , phaseMod_[0]) +
                        ampMod_[1] * ops_[1].compute(m3 , phaseMod_[1])));
            }

            case 7: // 2->1 ; 4->3 ; 1||3
                return ((ampMod_[0] * ops_[0].compute(
                            ampMod_[1] * ops_[1].compute(0.0f, phaseMod_[1]) , phaseMod_[0]
                        ) +
                        ampMod_[2] * ops_[2].compute(
                            ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]) , phaseMod_[2]
                        )));

            case 8: { // 4->(1,2,3) ; OUT=1+2+3
                const float m4 = ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]);
                return ((ampMod_[0] * ops_[0].compute(m4 , phaseMod_[0]) +
                        ampMod_[1] * ops_[1].compute(m4 , phaseMod_[1]) +
                        ampMod_[2] * ops_[2].compute(m4 , phaseMod_[2])));
            }

            case 9: { // 4->(2,3) ; OUT=1+2+3
                const float m4 = ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]);
                return ((ampMod_[0] * ops_[0].compute(0.0f, phaseMod_[0]) +
                        ampMod_[1] * ops_[1].compute(m4 , phaseMod_[1]) +
                        ampMod_[2] * ops_[2].compute(m4 , phaseMod_[2])));
            }

            case 10: // 4->3 ; OUT=1+2+3
                return ((ampMod_[0] * ops_[0].compute(0.0f, phaseMod_[0]) +
                        ampMod_[1] * ops_[1].compute(0.0f, phaseMod_[1]) +
                        ampMod_[2] * ops_[2].compute(
                            ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3]) , phaseMod_[2]
                        )));

            case 11: // 1||2||3||4
                return ((ampMod_[0] * ops_[0].compute(0.0f, phaseMod_[0]) +
                        ampMod_[1] * ops_[1].compute(0.0f, phaseMod_[1]) +
                        ampMod_[2] * ops_[2].compute(0.0f, phaseMod_[2]) +
                        ampMod_[3] * ops_[3].compute(0.0f, phaseMod_[3])));

            default: {
                vTaskDelay(1); // invalid RDX_State ?
                return 0.f;
            }
        }
    }

    inline void cacheParams() {
 //       ctl_.portaTimeS = patch_.common.portaTime * 0.0037f ; // 71ms at 19, 469ms at 127
        ctl_.portaTimeS = AM_DEPTH[patch_.common.portaTime] * 2.5f ; // 71ms at 19, 2500ms at 127
        algorithm_          = patch_.common.algorithm;
        pmDepth_            = PM_DEPTH[patch_.common.lfoPMD];
        lfo_.setWaveform((RDX_LFO::Waveform)patch_.common.lfoWave);
        lfo_.setRate(patch_.common.lfoSpeed);
        for (int i = 0; i < 4; ++i) {
            ops_[i].updateParams();
            pegEnable_[i]       = patch_.ops[i].pegEnable;
            lfoPMDEnable_[i]    = patch_.ops[i].lfoPMDEnable;
            lfoAMD_[i]          = patch_.ops[i].lfoAMD;
        }
    }


    inline void setHeld(bool g) { gate_ = g; }
    inline bool isHeld() const { return gate_; }

    inline void setSustained(bool s) { sustained_ = s; }
    inline bool isSustained() const { return sustained_; }


private:
    // --- Portamento ---
    float portamentoStartNote_  = 0.f;   // absolute semitone
    float portamentoTargetNote_ = 0.f;   // absolute semitone
    float portamentoPos_        = 1.f;   // 0..1
    float portamentoInc_        = 0.f;   // per-sample step (0 => no glide)
    float noteOnBaseNote_       = 0.f;   // absolute semitone that ops were set with
    float currentNoteSemitone_ = 0.0f;
    bool justAllocated_         = false; // score modifier
    float sampleRate_ = (float)SAMPLE_RATE;

    RDX_Operator ops_[4] = { // a bit of verbose so the ops would know who they are on creation, and could have a firm ref to their params structs
        RDX_Operator(0),
        RDX_Operator(1),
        RDX_Operator(2),
        RDX_Operator(3)
    };

    RDX_PEG             peg_;             // per-voice PEG
    RDX_Patch&          patch_          = RDX_State::getState().workingPatch; 
    RDX_Controls&       ctl_            = RDX_State::getState().controls;
    float               phaseMod_[4]    = {0.0f, 0.0f, 0.0f, 0.0f};         // per-operator PM input
    float               ampMod_[4]      = {1.0f, 1.0f, 1.0f, 1.0f};         // per-operator AM input
    float               score_ = 0.f;
    uint8_t             note_;
    float               velocity_ = 0.0f;
    bool                active_ = false;
    int                 gate_ = false;
    bool                sustained_ = false;   // note held by pedal
    
    // cached params
    int                 algorithm_          = 0;
    float               pmDepth_            = 0.f;
    int                 pegEnable_[4]       = {0};
    int                 lfoPMDEnable_[4]    = {0};
    int                 lfoAMD_[4]          = {0};

    // LFO
    RDX_LFO             lfo_;
    float               lfoValue_ = 0.f;
    float               lfoIncrement_ = 0.f;
    float               portaSemitoneOffset_ = 0.0f;
};


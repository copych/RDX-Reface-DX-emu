// RDX_Synth.h
#pragma once
#include <Arduino.h>
#include <atomic>
#include "config.h"
#include "RDX_Voice.h"
#include "RDX_Types.h"
#include "RDX_State.h"
#include "RDX_VoiceAlloc.h"
#include "RDX_GUI.h"


#ifdef ENABLE_GUI
extern RDX_GUI gui;
#endif
extern PresetManager pm;

inline float computePortaTime(uint8_t val) {
    return AM_DEPTH[val] * 2.5f;
}

class  RDX_Synth {
public:
    inline void init() {
        for (auto& v : voices_) {
            v.init();
        }
    }
 
    inline RDX_Patch& currentPatch() { return state_.workingPatch; }
    inline const RDX_Patch& currentPatch() const { return state_.workingPatch; }
    // Core1 only: invalidate CPU timing observations after edits.
    inline uint32_t budgetModelRevision() const { return budgetModelRevision_; }

    inline void applyPatch(const RDX_Patch& patch) {
        voiceAlloc_.allSoundOff(voices_, VOICES);

        state_.workingPatch = patch;
        ++budgetModelRevision_;

        calcFreqCoefs();
        updateGlobalDerived();

        for (int i = 0; i < MAX_VOICES; i++) {
            voices_[i].init();
            voices_[i].cacheParams(); 
            voices_[i].syncLFO();
        }

        calcOutputGain();

        state_.storedPatch = patch;

        globalDirty_ = 0;
        memset(opDirty_, 0, sizeof(opDirty_));
    #ifdef ENABLE_GUI
        gui.push();   // keep it
    #endif
    }

    // Core1 only. Keep voice slots contiguous: all slots above the new
    // limit must be retired before the render loop stops visiting them.
    // This follows the existing Core1 MIDI/voice mutation model.
    inline void setVoiceLimit(int next) {
        if (next < 1) next = 1;
        if (next > MAX_VOICES) next = MAX_VOICES;
        if (patch_.common.monoPoly != RDX_MODE_POLY) next = 1;
        const int old = VOICES;
        if (next == old) return;
        if (next < old) {
            for (int i = next; i < old; ++i) {
                voices_[i].setHeld(false);
                voices_[i].setSustained(false);
                voices_[i].noteOff();
                // The existing noteOff() releases the gate but may leave
                // oscillator envelopes sounding. Retire before hiding slot.
                voices_[i].init();
            }
        }
        VOICES = next;
    }

    inline int activeVoiceCount() const {
        int count = 0;
        for (int i = 0; i < VOICES; ++i)
            if (voices_[i].isActive()) ++count;
        return count;
    }

    inline void noteOn(uint8_t note, uint8_t vel) {
        const uint8_t mode = patch_.common.monoPoly;
        const int idx = voiceAlloc_.findVoice(voices_, VOICES, note, vel, mode);

        if (mode == RDX_MODE_MONO_LEGATO && voiceAlloc_.legatoPending()) {
            // legato -> same voice, glide or phase continue
            voices_[idx].noteOn(note, vel); // if implemented, else noteOn
        } else {
            voices_[idx].noteOn(note, vel);
        }
    }

    inline void noteOff(uint8_t note) {
        voiceAlloc_.noteOff(voices_, VOICES, note, patch_.common.monoPoly);
    }


    // Select the render specialization once per DMA block, not per sample or voice.
    inline IRAM_ATTR __attribute__((always_inline, hot)) void renderAudioBlock(
        float* outL, float* outR, uint32_t len = DMA_BUFFER_LEN) {
        if (patch_.common.monoPoly == RDX_MODE_POLY)
            renderAudioBlockMode<false>(outL, outR, len);
        else
            renderAudioBlockMode<true>(outL, outR, len);
    }

    template <bool WITH_PORTAMENTO>
    inline IRAM_ATTR __attribute__((always_inline)) float process() {
        float mix = 0.f;
        const float outGain = outputGain_;
        for (int i = 0; i < VOICES; i++) {
            mix += voices_[i].step<WITH_PORTAMENTO>() * outGain;
        }
        return mix;
    }

    template <bool WITH_PORTAMENTO>
    inline IRAM_ATTR __attribute__((always_inline, hot)) void renderAudioBlockMode(
        float* outL, float* outR, uint32_t len) {
        for (int i = 0; i < VOICES; i++) voices_[i].updateLfo();
        for (uint32_t i = 0; i < len; ++i) {
            const float sample = process<WITH_PORTAMENTO>();
            outL[i] = sample;
            outR[i] = sample;
        }
    }


    

	// Hardcoded DigiChord patch
	inline RDX_Patch DigiChordPatch() {
		RDX_Patch p{};

		// ------------------------
		// Common
		// ------------------------
		const char name[10] = {'D','i','g','i','C','h','o','r','d','\0'};
		memcpy(p.common.voiceName, name, 10);
		p.common.reserved1[0] = 0;
		p.common.reserved1[1] = 0;
		p.common.transpose   = 244;   //  
		p.common.monoPoly    = 1;    // Poly
		p.common.portaTime   = 0;
		p.common.pbRange     = 66;
		p.common.algorithm   = 3;
		p.common.lfoWave     = 0;    // Sine
		p.common.lfoSpeed    = 85;
		p.common.lfoDelay    = 0;
		p.common.lfoPMD      = 0;
		p.common.pegRate[0]  = 64;
		p.common.pegRate[1]  = 64;
		p.common.pegRate[2]  = 64;
		p.common.pegRate[3]  = 64;
		p.common.pegLevel[0] = 64;
		p.common.pegLevel[1] = 64;
		p.common.pegLevel[2] = 64;
		p.common.pegLevel[3] = 64;
		p.common.effects[0][0] = 4;   // Flanger
		p.common.effects[0][1] = 64;
		p.common.effects[0][2] = 64;
		p.common.effects[1][0] = 7;   // Reverb
		p.common.effects[1][1] = 59;
		p.common.effects[1][2] = 20;
		p.common.reserved2[0] = 0;
		p.common.reserved2[1] = 0;
		p.common.reserved2[2] = 0;

		// ------------------------
		// Operators
		// ------------------------
		uint8_t opEnable[4]       = {1,1,1,1};
		uint8_t egRate[4][4]      = {{127,44,46,50},{127,81,80,64},{127,44,68,72},{127,105,54,64}};
		uint8_t egLevel[4][4]     = {{127,81,0,0},{127,127,0,0},{127,81,0,0},{127,53,0,0}};
		uint8_t rateScaling[4]    = {0,0,0,0};
		uint8_t scaleLD[4]        = {0,20,0,0};
		uint8_t scaleRD[4]        = {20,0,0,0};
		uint8_t scaleLC[4]        = {0,3,0,0}; // -LIN=0, +LIN=3
		uint8_t scaleRC[4]        = {0,0,0,0};
		uint8_t lfoAMD[4]         = {0,0,0,0};
		uint8_t lfoPMDEnable[4]   = {1,1,1,1};
		uint8_t pegEnable[4]      = {1,1,1,1};
		uint8_t velSens[4]        = {40,40,57,65};
		uint8_t outLevel[4]       = {109,70,105,100};
		uint8_t feedback[4]       = {0,97,0,27};
		uint8_t fbType[4]         = {0,0,0,1}; // 0=Saw,1=Square
		uint8_t freqMode[4]       = {0,0,0,0}; // 0=Ratio
		uint8_t freqCoarse[4]     = {1,14,1,1};
		uint8_t freqFine[4]       = {0,0,0,0};
		uint8_t freqDetune[4]     = {0,0,0,0};
		uint8_t reserved[4][3]    = {{0,0,0},{0,0,0},{0,0,0},{0,0,0}};

		for (int i = 0; i < 4; i++) {
            // Assign each value explicitly for the current operator 'i'
            p.ops[i].enable = opEnable[i];

            // Replace memcpy for egRate and egLevel
            p.ops[i].egRate[0] = egRate[i][0];
            p.ops[i].egRate[1] = egRate[i][1];
            p.ops[i].egRate[2] = egRate[i][2];
            p.ops[i].egRate[3] = egRate[i][3];

            p.ops[i].egLevel[0] = egLevel[i][0];
            p.ops[i].egLevel[1] = egLevel[i][1];
            p.ops[i].egLevel[2] = egLevel[i][2];
            p.ops[i].egLevel[3] = egLevel[i][3];

            p.ops[i].rateScaling = rateScaling[i];
            p.ops[i].scaleLD = scaleLD[i];
            p.ops[i].scaleRD = scaleRD[i];
            p.ops[i].scaleLC = scaleLC[i];
            p.ops[i].scaleRC = scaleRC[i];
            p.ops[i].lfoAMD = lfoAMD[i];
            p.ops[i].lfoPMDEnable = lfoPMDEnable[i];
            p.ops[i].pegEnable = pegEnable[i];
            p.ops[i].velSens = velSens[i];
            p.ops[i].outLevel = outLevel[i];
            p.ops[i].feedback = feedback[i];
            p.ops[i].fbType = fbType[i];
            p.ops[i].freqMode = freqMode[i];
            p.ops[i].freqCoarse = freqCoarse[i];
            p.ops[i].freqFine = freqFine[i];
            p.ops[i].freqDetune = freqDetune[i];

            // Replace memcpy for reserved
            p.ops[i].reserved[0] = 0;
            p.ops[i].reserved[1] = 0;
            p.ops[i].reserved[2] = 0;
        }
		return p;
	}

    void processCC(int channel, uint8_t cc, uint8_t val) {
        switch (cc) {
            case 0:  // Bank Select MSB
                ctl_.wantBankMSB = val & 0x7F;
                break;
            case 32: // Bank Select LSB
                ctl_.wantBankLSB = val & 0x7F;
                break;
            case 1:
                ctl_.modWheel = val & 0x7F;
                ctl_.modWheelFactor = val * MIDI_NORM;
                markGlobalDirty(GDIRTY_LFO);
                markGlobalDirty(GDIRTY_MOD);
                break;
            case 5:
                patch_.common.portaTime = val;
                markGlobalDirty(GDIRTY_MOD);   // reuse existing
                break;
            case 7:
                ctl_.mainVolume = val & 0x7F;
                ctl_.mainVolumeFactor = val * MIDI_NORM;
                calcOutputGain();
                break;
            case 64: 
                ctl_.sustain = val > 63;
                if (!ctl_.sustain) {
                    // Pedal lifted → release all deferred notes
                    for (int i = 0; i < VOICES; ++i) {
                        RDX_Voice& v = voices_[i];
                        if (!v.isHeld() && v.isSustained()) {
                            v.setSustained(false);
                            v.noteOff();
                        }
                    }
                }
                break;

            case 65:
                ctl_.portamento = val>63 ? true : false ;
                break;
            // ========= PATCH COMMON ===============
            case 80:
                if (patch_.common.algorithm != val * 12 / 128) {
                    patch_.common.algorithm = val * 12 / 128;
                    globalDirty_ |= GDIRTY_ALGO;
                    ESP_LOGI("CC","set      algo to %d", patch_.common.algorithm);
                    ESP_LOGI("ALGO", "globalDirty now = %02X", globalDirty_);
                }
                break;


            // ========= OP 1 =======================    
            case 85:
                patch_.ops[0].outLevel = val;
                markOpDirty(0, DIRTY_GAIN);
                break;
            case 86:
                patch_.ops[0].feedback = val;
                markOpDirty(0, DIRTY_FEEDBACK);
                break;
            case 87:
                patch_.ops[0].fbType = val;
                markOpDirty(0, DIRTY_FEEDBACK);
                break;
            case 88:
                patch_.ops[0].freqMode = val; 
                markOpDirty(0, DIRTY_FREQ);
                break;
            case 89:
                patch_.ops[0].freqCoarse = val;
                markOpDirty(0, DIRTY_FREQ);
                break;
            case 90:
                patch_.ops[0].freqFine = val; 
                markOpDirty(0, DIRTY_FREQ);
                break;

            // ========= OP 2 =======================    
            case 102:
                patch_.ops[1].outLevel = val;
                markOpDirty(1, DIRTY_GAIN);
                break;
            case 103:
                patch_.ops[1].feedback = val;
                markOpDirty(1, DIRTY_FEEDBACK);
                break;
            case 104:
                patch_.ops[1].fbType = val;
                markOpDirty(1, DIRTY_FEEDBACK);
                break;
            case 105:
                patch_.ops[1].freqMode = val;
                markOpDirty(1, DIRTY_FREQ);
                break;
            case 106:
                patch_.ops[1].freqCoarse = val;
                markOpDirty(1, DIRTY_FREQ);
                break;
            case 107:
                patch_.ops[1].freqFine = val;
                markOpDirty(1, DIRTY_FREQ);
                break;
                
            // ========= OP 1 =======================    
            case 108:
                patch_.ops[2].outLevel = val;
                markOpDirty(2, DIRTY_GAIN);
                break;
            case 109:
                patch_.ops[2].feedback = val;
                markOpDirty(2, DIRTY_FEEDBACK);
                break;
            case 110:
                patch_.ops[2].fbType = val;
                markOpDirty(2, DIRTY_FEEDBACK);
                break;
            case 111:
                patch_.ops[2].freqMode = val; 
                markOpDirty(2, DIRTY_FREQ);
                break;
            case 112:
                patch_.ops[2].freqCoarse = val; 
                markOpDirty(2, DIRTY_FREQ);
                break;
            case 113:
                patch_.ops[2].freqFine = val;
                markOpDirty(2, DIRTY_FREQ);
                break;
                
            // ========= OP 1 =======================    
            case 114:
                patch_.ops[3].outLevel = val; 
                markOpDirty(3, DIRTY_GAIN);
                break;
            case 115:
                patch_.ops[3].feedback = val;
                markOpDirty(3, DIRTY_FEEDBACK);
                break;
            case 116:
                patch_.ops[3].fbType = val; 
                markOpDirty(3, DIRTY_FEEDBACK);
                break;
            case 117:
                patch_.ops[3].freqMode = val;
                markOpDirty(3, DIRTY_FREQ);
                break;
            case 118:
                patch_.ops[3].freqCoarse = val;
                markOpDirty(3, DIRTY_FREQ);
                break;
            case 119:
                patch_.ops[3].freqFine = val;
                markOpDirty(3, DIRTY_FREQ);
                break;
            case 120:
                voiceAlloc_.allSoundOff(voices_, VOICES);
                break;
            case 123:
                voiceAlloc_.allNotesOff(voices_, VOICES);
                break;
        }
    }

    inline void updateGlobalDerived() {
        ctl_.portaTimeS = computePortaTime(patch_.common.portaTime);
    }

    inline void flushUpdates() {
        uint8_t gd = globalDirty_;
        if (gd) {
            updateGlobalDerived();

            if (gd & GDIRTY_LFO) {
                for (auto& v : voices_) {
                    v.updateLfoParams();
                }
            }

            if (gd & GDIRTY_MOD) {
                for (auto& v : voices_) {
                    v.updateModParams();
                }
            }

            if (gd & GDIRTY_ALGO) {
                for (auto& v : voices_) {
                    v.setAlgorithm(patch_.common.algorithm);
                }
            }
            globalDirty_ = 0;
        #ifdef ENABLE_GUI
            gui.push();   // keep it
        #endif
        }
        for (int op = 0; op < 4; ++op) {
            uint8_t m = opDirty_[op];
            if (!m) continue;

            if (m & DIRTY_FREQ) {
                calcFreqCoef(op);
            }


            for (auto& v : voices_) {
                v.applyDirty(op, m);
            }

            opDirty_[op] = 0;
        #ifdef ENABLE_GUI
            gui.push();   // keep it
        #endif
        }
    }

    void updatePB(int channel, int val) {
        ctl_.pitchbend = val;
        float pbNorm = val / 8192.0f;
        ctl_.pitchbendSemitones = pbNorm * ((float)(patch_.common.pbRange - 64));
    }


    void programChange(uint8_t ch, uint8_t program) {
        if (ch >= 16) return;

        ctl_.wantProgram = program & 0x7F;
        applyBankProgram(ch);
    }

    inline void applyBankProgram(uint8_t ch) {
        (void)ch;
        const uint8_t program = ctl_.wantProgram;
        const uint16_t bank   = ctl_.getWantBank();
        RDX_Patch patch{};

        if (bank == 0) {
            if (pm.open(FS_Type::LITTLEFS, "/patches")) {
                if (!pm.openByIndex(program + 1, patch)) {
                    return;
                }
            } else {
                patch = DigiChordPatch(); // hardcoded patch
            }
        } else {
            return;
        }

        requestProgramPatch(patch);
    }

    // Program changes are committed only while Core0 is holding a silent block.
    // This keeps patch/voice mutation off the render path and prevents both
    // waveform discontinuities and cross-core partial-patch reads.
    inline void requestProgramPatch(const RDX_Patch& patch) {
        pendingProgramPatch_ = patch;

        const ProgramTransition st =
            (ProgramTransition)programTransition_.load(std::memory_order_acquire);

        if (st == ProgramTransition::IDLE) {
            programFade_ = 1.0f;
        }

        // A new PC received during fade-in simply reverses the ramp from its
        // current gain. During fade-out it replaces the pending destination.
        if (st != ProgramTransition::MUTED) {
            programTransition_.store((uint8_t)ProgramTransition::FADE_OUT,
                                     std::memory_order_release);
        }
    }

    inline bool programChangeMuted() const {
        return programTransition_.load(std::memory_order_acquire) ==
               (uint8_t)ProgramTransition::MUTED;
    }

    // Core1 only. Call while programChangeMuted() is true. Core0 does not
    // touch synth or FX state in that phase. Returns true exactly when a patch
    // was committed and the caller may safely synchronize the FX host.
    inline bool serviceProgramChange() {
        if (!programChangeMuted()) return false;

        voiceAlloc_.allSoundOff(voices_, VOICES);
        voiceAlloc_.reset();
        applyPatch(pendingProgramPatch_);
        return true;
    }

    // Core1 only, after FX has been synchronized to the newly applied patch.
    inline void finishProgramChange() {
        programFade_ = 0.0f;
        programTransition_.store((uint8_t)ProgramTransition::FADE_IN,
                                 std::memory_order_release);
    }

    // Core0 only, after FX processing and immediately before I2S output.
    // Normal audio pays only one atomic state check per block. The per-sample
    // multiplies run only during the short program-change ramps.
    inline IRAM_ATTR __attribute__((always_inline, hot))
    void applyProgramChangeFade(float* left, float* right, uint32_t len) {
        ProgramTransition st =
            (ProgramTransition)programTransition_.load(std::memory_order_acquire);

        if (st == ProgramTransition::IDLE) return;

        if (st == ProgramTransition::MUTED) {
            for (uint32_t i = 0; i < len; ++i) {
                left[i] = 0.0f;
                right[i] = 0.0f;
            }
            return;
        }

        float gain = programFade_;

        if (st == ProgramTransition::FADE_OUT) {
            for (uint32_t i = 0; i < len; ++i) {
                left[i] *= gain;
                right[i] *= gain;

                gain -= PROGRAM_FADE_STEP;
                if (gain <= 0.0f) {
                    gain = 0.0f;
                    for (uint32_t j = i + 1; j < len; ++j) {
                        left[j] = 0.0f;
                        right[j] = 0.0f;
                    }
                    break;
                }
            }

            programFade_ = gain;
            if (gain <= 0.0f) {
                // This is the last Core0 access to synth/FX state before the
                // MUTED handoff. The next audio block will bypass both.
                programTransition_.store((uint8_t)ProgramTransition::MUTED,
                                         std::memory_order_release);
            }
            return;
        }

        // FADE_IN
        for (uint32_t i = 0; i < len; ++i) {
            left[i] *= gain;
            right[i] *= gain;

            gain += PROGRAM_FADE_STEP;
            if (gain >= 1.0f) {
                gain = 1.0f;
                break;
            }
        }

        programFade_ = gain;
        if (gain >= 1.0f) {
            programTransition_.store((uint8_t)ProgramTransition::IDLE,
                                     std::memory_order_release);
        }
    }

    inline void calcFreqCoefs() {
        if (!RDX_State::isInitialized()) {
            return; 
        }

        for (int i = 0; i < 4 ; i++) {
            calcFreqCoef(i);
        }
    }

    inline void calcFreqCoef(int op_id) {
        if (patch_.ops[op_id].freqMode == 0) {
            if (patch_.ops[op_id].freqCoarse > 0)
                ctl_.freqCoef[op_id] = (patch_.ops[op_id].freqCoarse + patch_.ops[op_id].freqFine * 0.01f);
            else
                ctl_.freqCoef[op_id] = (0.5f + patch_.ops[op_id].freqFine * 0.005f);
        } else {
            float c = powf(10.0f, fclamp(patch_.ops[op_id].freqCoarse >> 3, 0.0f, 3.0f));
            float step = powf(9.772f, patch_.ops[op_id].freqFine * 0.01010101f );
            ctl_.freqCoef[op_id] = c * step;
        }
        // --- Yamaha detune law (Reface DX) 
		int dt = patch_.ops[op_id].freqDetune - 64; // [0..127], 64 = center
		if (dt != 0) {
			ctl_.detuneCoef[op_id] = powf(1.00033913f, float(dt));
		} else {
            ctl_.detuneCoef[op_id] = 1.0f;
        }
    }

    
    inline void markGlobalDirty(uint8_t mask) {
        globalDirty_ |= mask;
        ++budgetModelRevision_;
    }

    inline void markOpDirty(uint8_t op, uint8_t mask) {
        opDirty_[op] |= mask;
        ++budgetModelRevision_;
    }


    void calcOutputGain() {
        // depending on number of carrier ops

        if (!RDX_State::isInitialized()) {
            return; 
        }

        polyMixCoeff_ = 0.2f / sqrtf((float)MAX_VOICES);
        int algo = state_.workingPatch.common.algorithm;
        
        switch (algo) {
            case 5:
            case 6:
            case 7:
                algoMixCoeff_ = ONE_DIV_SQRT2  ; break;
            case 8:
            case 9:
            case 10:
                algoMixCoeff_ = ONE_DIV_SQRT3  ; break;
            case 11:
                algoMixCoeff_ = 0.5f  ; break;
            default:
                algoMixCoeff_ = 1.0f  ; break;
        }
        outputGain_ = algoMixCoeff_ * ctl_.mainVolumeFactor * polyMixCoeff_ ;
    }
    
    RDX_Voice& getVoice(int idx)  {return voices_[idx];}
    
private:
    enum class ProgramTransition : uint8_t {
        IDLE = 0,
        FADE_OUT,
        MUTED,
        FADE_IN
    };

    // 128 samples at 44.1 kHz = 2.90 ms. This is short enough to feel
    // instantaneous while long enough to remove arbitrary waveform cuts.
    static constexpr uint32_t PROGRAM_FADE_SAMPLES = 128;
    static constexpr float PROGRAM_FADE_STEP = 1.0f / PROGRAM_FADE_SAMPLES;

    RDX_Voice           voices_[MAX_VOICES];
    RDX_VoiceAllocator  voiceAlloc_;
    SynthState&         state_  = RDX_State::getState(); 
    RDX_Controls&       ctl_    = RDX_State::getState().controls;
    RDX_Patch&          patch_  = RDX_State::getState().workingPatch;
 
    float algoMixCoeff_ = 1.0f;
    float polyMixCoeff_ = 1.0f;
    float outputGain_ = 1.0f;

    RDX_Patch pendingProgramPatch_{};
    std::atomic<uint8_t> programTransition_{(uint8_t)ProgramTransition::IDLE};
    float programFade_ = 1.0f;

    int voiceUpdateIdx_ = 0;
        
    uint8_t opDirty_[4] = {0};
    uint32_t budgetModelRevision_ = 0; // Core1 only
    uint8_t globalDirty_ = 0;
};




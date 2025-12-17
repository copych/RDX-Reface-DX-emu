// RDX_VoiceAlloc.h

#pragma once
#include "RDX_Voice.h"
#include "RDX_Types.h"

// ======================================================
// RDX_VoiceAllocator
// Intelligent score-based allocator + mono note stack
// ======================================================
class RDX_VoiceAllocator {
public:


    inline IRAM_ATTR __attribute__((always_inline)) int findVoice(RDX_Voice* voices, int count, uint8_t note, uint8_t vel, uint8_t mode) {
        switch (mode) {
            case RDX_MODE_MONO_FULL:
            case RDX_MODE_MONO_LEGATO:
                pushNote(note);
                monoActive_ = true;
                return 0; // always single voice
            case RDX_MODE_POLY:
            default:
                break;
        }

        // --- polyphonic ---
        for (int i = 0; i < count; ++i) {
            if (!voices[i].isActive()) {
                ESP_LOGD("VA", "Using inactive voice %d", i);
                return i;
            }
        }

        float minScore = 1e9f;
        int victim = 0;
        for (int i = 0; i < count; ++i) {
            float s = voices[i].calcScore();
            if (s < minScore) { 
                minScore = s;
                victim = i;
            }
            ESP_LOGD("VA", "  ? voice %d score %f", i, s );
        }
        ESP_LOGD("VA", "Using victim voice %d score %f", victim, minScore );
        voices[victim].setJustAllocated();
        return victim;
    }


    inline IRAM_ATTR __attribute__((always_inline)) void noteOff(RDX_Voice* voices, int /*count*/, uint8_t note, uint8_t mode) {

        switch (mode) {
            case RDX_MODE_MONO_FULL:
            case RDX_MODE_MONO_LEGATO:
                popNote(note);
                if (stackSize_ == 0) {
                    monoActive_ = false;
                    voices[0].noteOff();
                } else {
                    // return (glide) to previous note
                    uint8_t prev = stack_[stackSize_ - 1];
                    voices[0].noteOn(prev, 100);
                }
                return;

            case RDX_MODE_POLY:
            default:
            {
                for (int i = 0; i < VOICES; ++i) {
                    RDX_Voice& v = voices[i];

                    if (v.note() == note && v.isActive()) {
                        // Key physically released
                        v.setHeld(false);

                        if (ctl_.sustain) {
                            // Pedal down → defer release
                            v.setSustained(true);
                        } else {
                            // Pedal up → release immediately if not held
                            if (!v.isHeld()) {
                                v.setSustained(false);
                                v.noteOff();
                            }
                        }
                    }
                }
                return;
            }
        }
    }

    inline void allSoundOff(RDX_Voice* voices, int count) {
        for (int i = 0; i < count; ++i) {
            voices[i].setHeld(false);
            voices[i].setSustained(false);
            voices[i].noteOff();
            //voices[i].reset();   // optional: hard reset envelopes/LFOs
        }
        clearStack();
        monoActive_ = false;
    }


    inline void allNotesOff(RDX_Voice* voices, int count) {
        for (int i = 0; i < count; ++i) {
            if (!ctl_.sustain) {
                // no sustain — kill everything
                voices[i].setSustained(false);
                voices[i].setHeld(false);
                voices[i].noteOff();
            } else if (!voices[i].isHeld()) {
                // sustain active — kill only non-held notes
                voices[i].setSustained(true);
                voices[i].noteOff();
            }
        }

        clearStack();
        monoActive_ = false;
    }


    inline bool legatoPending() const { return legatoPending_; }
    inline uint8_t monoNote() const { return monoNote_; }

    inline void clearStack() {
        stackSize_ = 0;
    }
    
private:
    RDX_Controls& ctl_ = RDX_State::getState().controls;
    // --- mono note stack ---
    static constexpr int MAX_STACK = 12;
    uint8_t stack_[MAX_STACK];
    uint8_t stackSize_ = 0;

    uint8_t monoNote_ = 0;
    bool monoActive_ = false;
    bool legatoPending_ = false;

    inline void pushNote(uint8_t note) {
        if (stackSize_ < MAX_STACK) stack_[stackSize_++] = note;
        monoNote_ = note;
    }

    inline void popNote(uint8_t note) {
        for (int i = 0; i < stackSize_; ++i) {
            if (stack_[i] == note) {
                for (int j = i; j < stackSize_ - 1; ++j)
                    stack_[j] = stack_[j + 1];
                --stackSize_;
                break;
            }
        }
    }

};

// RDX_MidiClock.h
#pragma once
#include "MidiClock.h"
#include "MIDI.h"   // fortyseveneffects MIDI lib

extern midi::MidiInterface<midi::SerialMIDI<HardwareSerial>> MIDI;

class RDX_MidiClock : public MidiClock {
public:
    void enableOut(bool enabled) { sendOut_ = enabled; }

protected:
    void produceTick() override {
        MidiClock::produceTick();
        if (sendOut_) {
            MIDI.sendRealTime(midi::Clock); // 0xF8
        }
    }

public:
    void sendStart() {
        if (sendOut_) MIDI.sendRealTime(midi::Start);
    }

    void sendStop() {
        if (sendOut_) MIDI.sendRealTime(midi::Stop);
    }

    void sendContinue() {
        if (sendOut_) MIDI.sendRealTime(midi::Continue);
    }

private:
    bool sendOut_ = true;
};

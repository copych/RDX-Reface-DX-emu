// RDX_State.h
#pragma once

// #include <atomic>
#include "RDX_Types.h"

// The Singleton Accessor Class

class RDX_State {
public:
    // Delete copy constructor and assignment operator
    RDX_State(const RDX_State&) = delete;
    RDX_State& operator=(const RDX_State&) = delete;
    static SynthState& getState() {
        static SynthState instance;
        return instance;
    }
    
    static bool isInitialized() {
        // Since we're using static local, it's always initialized
        // when called. This is safe for header-only.
        return true;
    }
};
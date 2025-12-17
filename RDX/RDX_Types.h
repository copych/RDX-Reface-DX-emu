// RDX_Types.h
#pragma once
#include <cstdint>

// ===============================
// Reface DX data types & structs
// ===============================

// ---------------------------------
// Enums
// ---------------------------------

// Frequency mode
enum RDX_FreqMode : uint8_t {
    RDX_FREQ_RATIO = 0,
    RDX_FREQ_FIXED = 1
};

// Feedback type
enum RDX_FeedbackType : uint8_t {
    RDX_FB_SAW    = 0,
    RDX_FB_SQUARE = 1
};

// Scaling curve types
enum RDX_ScaleCurve : uint8_t {
    RDX_SCALE_NEG_LIN = 0,
    RDX_SCALE_NEG_EXP = 1,
    RDX_SCALE_POS_EXP = 2,
    RDX_SCALE_POS_LIN = 3
};

// poly / mono mode
enum RDX_Mode : uint8_t {
    RDX_MODE_POLY = 0,
    RDX_MODE_MONO_FULL = 1,
    RDX_MODE_MONO_LEGATO = 2
};


// ---------------------------------------------------------
// Yamaha Reface DX checksum helper
// ---------------------------------------------------------
inline uint8_t rdxSyxChecksum(const uint8_t* data, uint32_t len) {
    // Sum all bytes, take lower 7 bits, subtract from 128, mask 7 bits
    uint32_t sum = 0;
    for (uint32_t i = 0; i < len; ++i) sum += data[i];
    return (128 - (sum & 0x7F)) & 0x7F;
}

// Convenience overload for direct brace-init usage
inline uint8_t rdxSyxChecksum(std::initializer_list<uint8_t> list) {
    uint32_t sum = 0;
    for (auto v : list) {
        sum += v;
    }
    return (128 - (sum & 0x7F)) & 0x7F;
}

struct RDX_Controls {
    int pitchbend = 0 ; // midi library gives it 0-centered
    float pitchbendSemitones = 0.0f;
    
    int mainVolume = 100; // cc#7
    float mainVolumeFactor = 1.0f; // 0.0 .. 1.0

    // Tuning
    float tuningSemitones = 0.0f;

    int modWheel = 0;
    float modWheelFactor = 0.0f;
    
    bool sustain = false; // sustain pedal CC#64
    
    bool portamento = false; // portamento CC#65
    
    float portaTimeS = 0.06f; // 60ms

	// bank / program
    uint32_t  bankMSB = 0;     // CC#0
    uint32_t  bankLSB = 0;     // CC#32
    uint32_t  program = 0;     // Program Change

    uint32_t  wantBankMSB = 0;     // CC#0
    uint32_t  wantBankLSB = 0;     // CC#32
    uint32_t  wantProgram = 0;     // Program Change desired
    
	// NRPN
    struct ParamPair { uint8_t msb = 0x7F, lsb = 0x7F; };
    ParamPair rpn;
    ParamPair nrpn;

/*
    std::array<uint8_t, 8> noteStack = {};  
    uint8_t stackSize = 0;

    inline void pushNote(uint8_t note) {
        if (stackSize < noteStack.size()) {
            noteStack[stackSize++] = note;
        }
    }

    inline void removeNote(uint8_t note) {
        for (uint8_t i = 0; i < stackSize; ++i) {
            if (noteStack[i] == note) {
                for (uint8_t j = i; j < stackSize - 1; ++j)
                    noteStack[j] = noteStack[j + 1];
                --stackSize;
                break;
            }
        }
    }

    inline uint8_t topNote() const {
        return stackSize > 0 ? noteStack[stackSize - 1] : 0xFF;
    }

    inline bool hasNotes() const {
        return stackSize > 0;
    }

    inline void clearNoteStack() {
        stackSize = 0;
    }
*/
    // Utility
    inline uint16_t getBank() const {
        return (bankMSB << 7) | bankLSB;
    }
    inline uint16_t getWantBank() const {
        return (wantBankMSB << 7) | wantBankLSB;
    }

    inline void setBank(uint16_t bank) {
        bankMSB = 0b01111111 & (bank >> 7);
        bankLSB = 0b01111111 & bank;
    }


};

// ---------------------------------------------------------
// System parameters (32 bytes)
// ---------------------------------------------------------
struct __attribute__((packed)) RDX_System {
    uint8_t txMidiChan = 0;     // 00-0F, 7F : 1-15, off
    uint8_t rxMidiChan = 0;  // 00-0F, 0x10 : 1-15, all
    uint8_t masterTune[4] = {0};// 4 lower bits of each element MSB to LSB: 15-12, 11-8, 7-4, 3-0 : -102.4 .. 102.3
    uint8_t localControl = 1;   // 00-01 : off/ON 
    uint8_t masterTranspose = 0x40;    // 34-4C : -12..+12 semitones 
    uint8_t tempo[2] = {0, 0x78};   // 7 bits of each MSB to LSB: 13-7, 6-0: tempo 30..300  
    uint8_t lcdContrast = 20;   // 00-3F
    uint8_t pedalModel = 1;     // 00-01 : FC3 or FC4/FC5 
    uint8_t autoPowerOff = 1;   // 00-01 : off/ON
    uint8_t speakerOn = 1;      // 00-01 : off/ON 
    uint8_t midiControl = 1;    // 00-01 : off/ON
    uint8_t reserved[17];
};

// ---------------------------------------------------------
// Common patch parameters (38 bytes)
// ---------------------------------------------------------
struct __attribute__((packed)) RDX_Common {
    uint8_t voiceName[10];   // 0..9  : VOICENAME (10 chars)
    uint8_t reserved1[2];    // 10..11: reserved
    uint8_t transpose ;  // 12    : TRANSPOSE
    uint8_t monoPoly;        // 13    : MONO/POLY
    uint8_t portaTime;       // 14    : PORTATIME
    uint8_t pbRange ;     // 15    : PB RANGE
    uint8_t algorithm;       // 16    : ALGORITHM
    uint8_t lfoWave;         // 17    : LFO WAVE
    uint8_t lfoSpeed;        // 18    : LFO SPEED
    uint8_t lfoDelay;        // 19    : LFO DELAY
    uint8_t lfoPMD;          // 20    : LFO PMD
    uint8_t pegRate[4];      // 21..24: PEG RATE1..4
    uint8_t pegLevel[4];     // 25..28: PEG LEVEL1..4
    uint8_t effects[2][3];  // 29    0: FX1 TYPE
                            // 30    1: FX1 PARAM1
                            // 31    2: FX1 PARAM2
                            // 32    0: FX2 TYPE
                            // 33    1: FX2 PARAM1
                            // 34    2: FX2 PARAM2
    uint8_t reserved2[3];    // 35..37: reserved
};

// ---------------------------------------------------------
// Operator parameters (28 bytes each, x4)
// ---------------------------------------------------------
struct __attribute__((packed)) RDX_OpParams {
    uint8_t enable;        // 0   : OP On/Off
    uint8_t egRate[4];     // 1-4 : EG RATE1..4
    uint8_t egLevel[4];    // 5-8 : EG LEVEL1..4
    uint8_t rateScaling;   // 9   : RATE SCALING
    uint8_t scaleLD;       // 10  : SCALING LD
    uint8_t scaleRD;       // 11  : SCALING RD
    uint8_t scaleLC;       // 12  : SCALING LC
    uint8_t scaleRC;       // 13  : SCALING RC
    uint8_t lfoAMD;        // 14  : LFO AMD
    uint8_t lfoPMDEnable;  // 15  : LFO PMD On/Off
    uint8_t pegEnable;     // 16  : PEG On/Off
    uint8_t velSens;       // 17  : VELO SENS
    uint8_t outLevel;      // 18  : OUT LEVEL
    uint8_t feedback;      // 19  : FEEDBACK
    uint8_t fbType;        // 20  : FB TYPE
    uint8_t freqMode;      // 21  : FREQ MODE
    uint8_t freqCoarse;    // 22  : FREQ COARSE
    uint8_t freqFine;      // 23  : FREQ FINE
    uint8_t freqDetune;    // 24  : FREQ DETUNE
    uint8_t reserved[3];   // 25-27: reserved
};

// ---------------------------------------------------------
// Full patch (38 + 4*28 = 150 bytes)
// ---------------------------------------------------------
struct __attribute__((packed)) RDX_Patch {
    RDX_Common common;       // 38 bytes
    RDX_OpParams ops[4];     // 112 bytes
};

// ---------------------------------
// Bank Parameters (future)
// ---------------------------------
struct RDX_Bank {
    RDX_Patch patches[32];              // 32 patch slots
};


// The main state structure.
struct SynthState {
    RDX_System      system;
    RDX_Patch       storedPatch;
    RDX_Patch       workingPatch;
    RDX_Controls    controls;
};

static inline bool patchToSyx( RDX_Patch& patch, uint8_t* out, uint32_t& outLen, uint8_t midiCh=0, uint8_t patchNum=0) {
    // yamaha header
    const uint8_t head[] = {0xF0, 0x43, midiCh, 0x7F, 0x1C, 0x00};
    uint8_t* w = out;
    auto writeBlock = [&](uint8_t cmd, const uint8_t* data, uint32_t len) {
        memcpy(w, head, sizeof(head)); w += sizeof(head);
        *w++ = cmd;
        memcpy(w, data, len); w += len;
        *w++ = rdxSyxChecksum(data, len);
        *w++ = 0xF7;
    };

    // HEADER block
    uint8_t a = (patchNum==0) ? 0x0F : 0x00;
    uint8_t p = (patchNum==0) ? 0x00 : (uint8_t)((patchNum-1)%32);
    uint8_t headerBlk[] = {0x05, 0x0E, a, p};
    writeBlock(0x04, headerBlk, sizeof(headerBlk));

    // COMMON block (38 bytes)
    uint8_t commonBlk[4+38];
    commonBlk[0]=0x05; commonBlk[1]=0x30; commonBlk[2]=0x00; commonBlk[3]=0x00;
    memcpy(commonBlk+4, &patch.common, 38);
    writeBlock(0x2A, commonBlk, sizeof(commonBlk));

    // OPS
    for (int i=0;i<4;i++) {
        uint8_t opBlk[4+28];
        opBlk[0]=0x05; opBlk[1]=0x31; opBlk[2]=i; opBlk[3]=0x00;
        memcpy(opBlk+4, &patch.ops[i], 28);
        writeBlock(0x20, opBlk, sizeof(opBlk));
    }

    // FOOTER
    uint8_t footerBlk[] = {0x05, 0x0F, a, p};
    writeBlock(0x04, footerBlk, sizeof(footerBlk));

    outLen = (uint32_t)(w - out);
 

    return true;
}

static inline bool syxToPatch(const uint8_t* syx, uint32_t len, RDX_Patch& patch) {
    // naive parser: find common/op blocks, validate checksum, copy
    uint32_t i=0;
    while (i+10 < len) {
        if (syx[i]!=0xF0 || syx[i+1]!=0x43) { i++; continue; } // not yamaha syx
        uint8_t cmd = syx[i+6];
        const uint8_t* data = syx+i+7;
        uint32_t dlen = 0;
        // common
        if (cmd==0x2A && data[1]==0x30) {
            dlen = 42; // 4 header + 38 data
            if (rdxSyxChecksum(data, dlen)!=syx[i+7+dlen]) return false;
            memcpy(&patch.common, data+4, 38);
        }
        // ops
        else if (cmd==0x20 && data[1]==0x31) {
            dlen = 32; // 4 header + 28 data
            int op = data[2];
            if (op<0 || op>3) return false;
            if (rdxSyxChecksum(data, dlen)!=syx[i+7+dlen]) return false;
            memcpy(&patch.ops[op], data+4, 28);
        }
        // advance
        while (i < len && syx[i]!=0xF7) i++;
        i++;
    } 
    return true;

}



inline void printPatchParams(const RDX_Patch& patch) {
    // --- Common ---
    char name[11] = {0};
    memcpy(name, patch.common.voiceName, 10);
    ESP_LOGI("Voice", "VoiceName: %s", name);
    ESP_LOGI("Voice", "Transpose: %d, Mono/Poly: %d, PortaTime: %d, PBRange: %d",
                patch.common.transpose - 64,
                patch.common.monoPoly,
                patch.common.portaTime,
                patch.common.pbRange - 64);
    ESP_LOGI("Voice", "Algorithm: %d, LFO Wave: %d, LFO Speed: %d, LFO Delay: %d, LFO PMD: %d",
                patch.common.algorithm,
                patch.common.lfoWave,
                patch.common.lfoSpeed,
                patch.common.lfoDelay,
                patch.common.lfoPMD);
    ESP_LOGI("Voice", "PEG Rates: %d,%d,%d,%d  Levels: %d,%d,%d,%d",
                patch.common.pegRate[0], patch.common.pegRate[1],
                patch.common.pegRate[2], patch.common.pegRate[3],
                patch.common.pegLevel[0], patch.common.pegLevel[1],
                patch.common.pegLevel[2], patch.common.pegLevel[3]);
    ESP_LOGI("Voice", "FX1: type=%d p1=%d p2=%d  FX2: type=%d p1=%d p2=%d",
                patch.common.effects[0][0], patch.common.effects[0][1], patch.common.effects[0][2],
                patch.common.effects[1][0], patch.common.effects[1][1], patch.common.effects[1][2]);

    // --- Operators ---
    for (int i=0; i<4; i++) {
        const auto& op = patch.ops[i];
        ESP_LOGI("Voice", "---- OP%d ----", i+1);
        ESP_LOGI("Voice", "Enable=%d OutLevel=%d VelSens=%d Feedback=%d FbType=%d",
                    op.enable, op.outLevel, op.velSens, op.feedback, op.fbType);
        ESP_LOGI("Voice", "EG Rates: %d,%d,%d,%d  Levels: %d,%d,%d,%d",
                    op.egRate[0], op.egRate[1], op.egRate[2], op.egRate[3],
                    op.egLevel[0], op.egLevel[1], op.egLevel[2], op.egLevel[3]);
        ESP_LOGI("Voice", "RateScaling=%d ScaleLD=%d ScaleRD=%d ScaleLC=%d ScaleRC=%d",
                    op.rateScaling, op.scaleLD, op.scaleRD, op.scaleLC, op.scaleRC);
        ESP_LOGI("Voice", "LFO AMD=%d PMDOn=%d PEGOn=%d",
                    op.lfoAMD, op.lfoPMDEnable, op.pegEnable);
        ESP_LOGI("Voice", "FreqMode=%d Coarse=%d Fine=%d Detune=%d",
                    op.freqMode, op.freqCoarse, op.freqFine, op.freqDetune);
    }
}

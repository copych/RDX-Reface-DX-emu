// RDX_Midi.h
#pragma once
#include <Arduino.h>
#include <MIDI.h> 
#include "RDX_State.h" // for RDX_State::getState()
#include "RDX_Synth.h"


extern RDX_Synth synth;   // defined in RDX.ino

#if MIDI_IN_DEV == USE_USB_MIDI_DEVICE
    #include "src/usbmidi/src/USB-MIDI.h"
#endif

#if MIDI_IN_DEV == USE_MIDI_STANDARD
    MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);
#elif MIDI_IN_DEV == USE_USB_MIDI_DEVICE
    USBMIDI_CREATE_INSTANCE(0, MIDI); // reface DX
#endif

constexpr uint32_t SYSEX_BUF_SIZE = 512;
uint8_t syxBuf[SYSEX_BUF_SIZE];
uint32_t syxLen = 0;

// -----------------------------
// Bulk dump helpers
// -----------------------------
inline void dumpSysex(const uint8_t* buf, uint32_t len, const char* tag = "SYSEX") {
    ESP_LOGD(tag, "SysEx (%u bytes):", len);
    char line[128];
    uint32_t pos = 0;
    for (uint32_t i = 0; i < len; i++) {
        int n = snprintf(line + pos, sizeof(line) - pos, "%02X ", buf[i]);
        pos += n;
        if ((i + 1) % 16 == 0 || i + 1 == len) {
            ESP_LOGD(tag, "%s", line);
            pos = 0;
        }
    }
}





// ==========================
// Helper: Send a single block (Model ID + Address + Data)
// ==========================
inline void sendBlock(uint8_t device, uint8_t addrH, uint8_t addrM, uint8_t addrL,
                      const uint8_t* data, uint32_t dataLen)
{
    uint8_t byteCount = 1 + 3 + dataLen; // ModelID + AddrH/M/L + data
    uint8_t syx[512];
    uint8_t* w = syx;

    *w++ = 0xF0;
    *w++ = 0x43;
    *w++ = device;
    *w++ = 0x7F;
    *w++ = 0x1C;
    *w++ = (byteCount >> 7) & 0x7F;   // MSB
    *w++ = byteCount & 0x7F;          // LSB
    *w++ = 0x05;                       // Model ID
    *w++ = addrH;
    *w++ = addrM;
    *w++ = addrL;
    if (dataLen>0) memcpy(w, data, dataLen);
    w += dataLen;
    uint8_t chk = rdxSyxChecksum(syx + 7, 1 + 3 + dataLen); // ModelID+Addr+Data
    *w++ = chk;
    *w++ = 0xF7;

    uint32_t len = w - syx;
    dumpSysex(syx, len, "OUT");
    MIDI.sendSysEx(len, syx, true);
}

// ==========================
// Send Common Block
// ==========================
inline void sendCommonBlock(const RDX_Patch& patch, uint8_t device=0x00) {
    sendBlock(device, 0x30, 0x00, 0x00, reinterpret_cast<const uint8_t*>(&patch.common), sizeof(RDX_Common));
}

// ==========================
// Send Operator Block
// ==========================
inline void sendOperatorBlock(const RDX_Patch& patch, int opNum, uint8_t device=0x00) {
    if (opNum < 0 || opNum > 3) return;
    sendBlock(device, 0x31, opNum, 0x00, reinterpret_cast<const uint8_t*>(&patch.ops[opNum]), sizeof(RDX_OpParams));
}

// ==========================
// Send Full Patch (Common + 4 Ops)
// ==========================
inline void sendFullPatch(const RDX_Patch& patch, uint8_t device=0x00) {
    // Bulk Header 
    sendBlock(device, 0x0e, 0x0f, 0x00, nullptr, 0);

    // Common + Ops
    sendCommonBlock(patch, device);
    for (int i = 0; i < 4; ++i) sendOperatorBlock(patch, i, device);

    // Bulk Footer
    sendBlock(device, 0x0f, 0x0f, 0x00, nullptr, 0);
}

inline void sendSystemBulk(uint8_t device = 0x00) {
    SynthState& state = RDX_State::getState();
    sendBlock(device, 0x00, 0x00, 0x00, reinterpret_cast<const uint8_t*>(&state.system), sizeof(state.system));
}

inline void sendBulkDump(uint8_t device, const RDX_Patch& patch) {
    // 1) send SYSTEM bulk message
    sendSystemBulk(0x00);

    // 2) send VOICE blocks (header, common, ops, footer)
    sendFullPatch(patch, 0x00);
}

inline void sendIdentityReply(uint8_t deviceId = 0x7F) {
    // SysEx Identity Reply for Yamaha Reface DX 
    // F0 7E 7F 06 02 43 00 41 53 06 00 00 00 7F F7
    const uint8_t reply[] = {
        0xF0,       // SysEx start
        0x7E,       // Non-realtime
        deviceId,   // Device ID (0x7F = all-call)
        0x06, 0x02, // Sub-ID #1/#2: Identity Reply
        0x43,       // Yamaha (0x43)
        0x00, 0x41, // Family code
        0x53, 0x06, // Model ID = Reface DX
        0x03, 0x00, 0x00, 0x7F, // version = 1.0 + data[10] / 10.0
        0xF7        // SysEx end
    };
    uint32_t delayMs = esp_random() % 64 ;
    delay(delayMs);
    dumpSysex(reply, sizeof(reply), "OUT ID");
    MIDI.sendSysEx(sizeof(reply), reply, true);
}


inline bool unpackCommonBlock(const uint8_t* data, uint32_t len, RDX_Patch& patch) {
    if (len < 43) return false;
    if (rdxSyxChecksum(data + 4, 38) != data[42]) return false;
    memcpy(&patch.common, data + 4, 38);
    return true;
}

inline bool unpackOperatorBlock(const uint8_t* data, uint32_t len, RDX_Patch& patch) {
    if (len < 33) return false;
    int opNum = data[2];
    if (opNum < 0 || opNum > 3) return false;
    if (rdxSyxChecksum(data + 4, 28) != data[32]) return false;
    memcpy(&patch.ops[opNum], data + 4, 28);
    return true;
}



// -----------------------------
//  apply single param
// -----------------------------
inline void applyCommonParam(RDX_Patch& patch, uint8_t addr, uint8_t val) {
    if (addr >= sizeof(RDX_Common)) return;
    reinterpret_cast<uint8_t*>(&patch.common)[addr] = val;
}

inline void applyOperatorParam(RDX_Patch& patch, int opNum, uint8_t addr, uint8_t val) {
    if (opNum < 0 || opNum > 3) return;
    if (addr >= sizeof(RDX_OpParams)) return;
    reinterpret_cast<uint8_t*>(&patch.ops[opNum])[addr] = val;
}
 
// ------------------- MIDI callbacks -------------------

void handleAll(const midi::MidiInterface<usbMidi::usbMidiTransport>::MidiMessage& msg) {
    ESP_LOGI("MIDI", "Type=%02X Chan=%d Data1=%02X Data2=%02X",
             msg.type, msg.channel, msg.data1, msg.data2);
}

inline void handleSysEx(byte* data, unsigned length) {
    dumpSysex( data, length, "SYSEX IN");
    if (length < 6 || data[0] != 0xF0 || data[length - 1] != 0xF7) return;

    // ---------------- Identity Request ----------------
    // F0 7E <device> 06 01 F7
    if (data[1] == 0x7E && data[3] == 0x06 && data[4] == 0x01) {
        ESP_LOGD("MIDI", "SysEx: Identity Request");
        sendIdentityReply(0x7F);
        return;
    }

    // ---------------- Yamaha header check ----------------
    if (data[1] != 0x43) return;
    uint8_t device = data[2];
    if (data[3] != 0x7F || data[4] != 0x1C) return; // group mismatch

    uint8_t typeNibble = (device >> 4) & 0x0F;
    uint8_t addrH = (length > 6) ? data[6] : 0;
    uint8_t addrM = (length > 7) ? data[7] : 0;
    uint8_t addrL = (length > 8) ? data[8] : 0;
    uint32_t addr = (addrH << 16) | (addrM << 8) | addrL;
    
    switch (typeNibble) {

    // ===================================================
    case 0x1: { // PARAMETER CHANGE (editor → us)
    // ===================================================
        if (length < 10) return;
        uint8_t val = data[9];
        if (addrH == 0x30) {
            ESP_LOGD("IN", "Common param change: offset=0x%02X val=%d", addrL, val);
            applyCommonParam(synth.currentPatch(), addrL, val);
        } else if (addrH == 0x31) {
            ESP_LOGD("IN", "Operator %d param change: offset=0x%02X val=%d", addrM, addrL, val);
            applyOperatorParam(synth.currentPatch(), addrM, addrL, val);
        } else {
            ESP_LOGI("IN", "Unknown param change at addr=%02X%02X%02X", addrH, addrM, addrL);
        }
        // echo
       // MIDI.sendSysEx(sizeof(data), data, true);
        break;
    }

    // ===================================================
    case 0x0: { // BULK DUMP (editor → us)
    // ===================================================
        ESP_LOGI("IN", "BulkDump received (%u bytes)", length);
        // ModelID is data[7], Address is data[8..10], Data starts at [11]
        // Here you’d call unpackCommonBlock/unpackOperatorBlock depending on addrH
        break;
    }

    // ===================================================
    case 0x2: { // DUMP REQUEST (editor → request us → reply with bulk)
    // ===================================================
        if (length < 10) return;
        ESP_LOGI("IN", "DumpRequest 0x02 addr=%02X%02X%02X", addrH, addrM, addrL);
        if (addr == 0x00000000) sendBulkDump(0x00, synth.currentPatch());  // 0x00000000 is a SYSTEM block request addr
        if (addr == 0x000e0f00) sendFullPatch( synth.currentPatch()); // 0x0e0f00 is a VOICE block request addr
        break;
    }

    // ===================================================
    case 0x3: { // PARAMETER REQUEST (editor → request us → reply single block)
    // ===================================================
        if (length < 10) return;
        ESP_LOGD("IN", "ParamRequest addr=%02X%02X%02X", addrH, addrM, addrL);
        if (addrH == 0x30) {
            sendCommonBlock(synth.currentPatch());
        } else if (addrH == 0x31) {
            sendOperatorBlock(synth.currentPatch(), addrM);
        } else if (addrH == 0x0E) {
            sendFullPatch(synth.currentPatch());
        } else {
            ESP_LOGI("IN", "Unhandled param request at addr=%02X%02X%02X", addrH, addrM, addrL);
        }
        break;
    }

    default:
        ESP_LOGI("IN", "Unhandled SysEx device=0x%02X", device);
        break;
    }
}


 
void handleNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
#ifdef ENABLE_GUI
    gui.pause(20);
#endif
    ESP_LOGD("MIDI", "Note on %d %d", note, velocity);
    synth.noteOn(note, velocity);

    // Forward to Soundmondo / external MIDI
//    MIDI.sendNoteOn(note, velocity, channel);
}

void handleNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
#ifdef ENABLE_GUI
    gui.pause(20);
#endif
    synth.noteOff(note);
 
}

void handleCC(uint8_t channel, uint8_t cc, uint8_t val) {
#ifdef ENABLE_GUI
    gui.pause(20);
#endif
    synth.processCC(channel, cc, val);
}

void handlePB(uint8_t channel, int pb) {
#ifdef ENABLE_GUI
    gui.pause(20);
#endif
    synth.updatePB(channel, pb);
}

void handleProgChange(uint8_t channel, uint8_t pr) {
#ifdef ENABLE_GUI
    gui.pause(20);
#endif
    synth.programChange(channel, pr);
}

void setupMidi() {
#if MIDI_IN_DEV == USE_USB_MIDI_DEVICE
  // Change USB Device Descriptor Parameter
    USB.VID(0x0499);
    USB.PID(0x1624); // A guess. Most likely 0x1622..0x1625, cause they were added in 2015. As of identity range 0x51..0x54 DX has 0x53, I choose 3rd: 0x1624 
 
   // USB.usbVersion(0x0000);
    USB.productName("reface DX");
    USB.manufacturerName("Yamaha Corp.");
    USB.usbClass(TUSB_CLASS_AUDIO);
    USB.usbSubClass(0x00);
    USB.usbProtocol(0x00);
    USB.usbAttributes(0x80);
#endif
    MIDI.setHandleNoteOn(handleNoteOn);
    MIDI.setHandleNoteOff(handleNoteOff);
    MIDI.setHandleControlChange(handleCC);
    MIDI.setHandlePitchBend(handlePB);
    MIDI.setHandleProgramChange(handleProgChange);    
    MIDI.setHandleSystemExclusive(handleSysEx);
  //  MIDI.setHandleMessage(handleAll);
    MIDI.begin(MIDI_CHANNEL_OMNI);
    delay(800);
}

void keepAlive() {
    static uint32_t lastActiveSensingMicros = 0;
    uint32_t now = micros();

    if (now - lastActiveSensingMicros >= 200000) { // 200 ms
        lastActiveSensingMicros = now;
        MIDI.sendActiveSensing();
    }
}


inline void processMidi() {
    MIDI.read();
    //keepAlive();
}




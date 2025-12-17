// config.h
#pragma once

// ===================== AUDIO ======================================================================================
#define   DMA_BUFFER_NUM        2     // number of internal DMA buffers
#define   DMA_BUFFER_LEN        128    // length of each buffer in samples
#define   CHANNEL_SAMPLE_BYTES  2     // can be 1, 2, 3 or 4 (2 and 4 only supported yet)
#define   SAMPLE_RATE           44100

// ===================== MIDI =======================================================================================
#define   USE_USB_MIDI_DEVICE   1     // definition: the synth appears as a USB MIDI Device "S3 SF2 Synth"
#define   USE_MIDI_STANDARD     2     // definition: the synth receives MIDI messages via serial 31250 bps
#define   MIDI_IN_DEV           USE_USB_MIDI_DEVICE     // select the appropriate (one of the above) 
#define   NUM_MIDI_CHANNELS		16

// ===================== SYNTHESIZER ================================================================================
#define MAX_VOICES 8
#define MAX_VOICES_PER_NOTE 2

// ===================== MIDI PINS ==================================================================================
#define MIDI_IN         4      // if USE_MIDI_STANDARD is selected as MIDI_IN, this pin receives MIDI messages

// ===================== I2S PINS ===================================================================================
#define I2S_BCLK_PIN    5       // I2S BIT CLOCK pin (BCL BCK CLK)
#define I2S_DOUT_PIN    6       // MCU Data Out: connect to periph. DATA IN (DIN D DAT)
#define I2S_WCLK_PIN    7       // I2S WORD CLOCK pin (WCK WCL LCK)
#define I2S_DIN_PIN     -1      // MCU Data In: connect to periph. DATA OUT (DOUT D SD)

// ===================== SD MMC PINS ================================================================================
// ESP32S3 allows almost any GPIOs for any particular needs
#define SDMMC_CMD 38
#define SDMMC_CLK 39
#define SDMMC_D0  10
#define SDMMC_D1  11
#define SDMMC_D2  12
#define SDMMC_D3  13


// ===================== GUI SETTINGS ==========================================================================

  #define ACTIVE_STATE  LOW   // LOW = switch connects to GND, HIGH = switch connects to 3V3

  #define BTN0_PIN 	14
  #define ENC0_A_PIN 	15
  #define ENC0_B_PIN 	16

  #define BTN1_PIN 	41
  #define BTN2_PIN 	42
  #define BTN3_PIN 	2
  #define BTN4_PIN  1
  #define BTN5_PIN 	0

// #define ENABLE_GUI

#define USE_SD // where to store patches and configs: comment out to use LittleFS on internal flash

#ifdef ENABLE_GUI
  #define DISPLAY_CONTROLLER SH1106
  // #define DISPLAY_CONTROLLER SSD1306

  #define DISPLAY_SDA 8 // SDA GPIO
  #define DISPLAY_SCL 9 // SCL GPIO

  #define DISPLAY_W 128
  #define DISPLAY_H 64
  #define DISPLAY_ROTATE 0 // can be 0, 90, 180 or 270
#endif


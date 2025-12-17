# RDX
This is an ESP32S3 approximation of a known FM synth. I don't have the real one, so it's a kind of a self-set challenge. It has a lot of 'AS IS', and mostly consists of compromises.
Yet, by default it's a USB-MIDI device that one can use with some midi host. It understands almost every documented midi feature, though I don't know how the original behaves.
It has 7-8 notes polyphony, 4 operators and 2 effects in a chain.
The code is written from scratch, but has a lot of AI generated pieces. Otherwise it'd take me forever to get at this point.

##  TODO 
*  Looper
*  GUI
*  hardware controls
*  saving patches
*  effects improving

##  BUILD
refer to `config.h` to see pins and edit settings

```
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
  
```

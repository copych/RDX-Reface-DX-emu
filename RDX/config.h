// config.h
#pragma once


// ===================== AUDIO ==================================
#define   DMA_BUFFER_NUM        2     // number of internal DMA buffers
#define   DMA_BUFFER_LEN        128    // length of each buffer in samples
#define   CHANNEL_SAMPLE_BYTES  2     // can be 1, 2, 3 or 4 (2 and 4 only supported yet)
#define   SAMPLE_RATE           44100

// ===================== MIDI ===================================
#define   USE_USB_MIDI_DEVICE   1     // definition: the synth appears as a USB MIDI Device "S3 SF2 Synth"
#define   USE_MIDI_STANDARD     2     // definition: the synth receives MIDI messages via serial 31250 bps
#define   MIDI_IN_DEV           USE_USB_MIDI_DEVICE     // select the appropriate (one of the above) 
#define   NUM_MIDI_CHANNELS		16


#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ===================== SYNTHESIZER ============================
  #define TIMING_CORRECTION 1.0f // to estimate polyphony
  #define MAX_VOICES 8
  #define MAX_VOICES_PER_NOTE 2

  // ===================== MIDI PINS ==============================
  #define MIDI_IN         4      // if USE_MIDI_STANDARD is selected as MIDI_IN, this pin receives MIDI messages
  // ===================== I2S PINS ===============================
  #define I2S_BCLK_PIN    5       // I2S BIT CLOCK pin (BCL BCK CLK)
  #define I2S_DOUT_PIN    6       // MCU Data Out: connect to periph. DATA IN (DIN D DAT)
  #define I2S_WCLK_PIN    7       // I2S WORD CLOCK pin (WCK WCL LCK)
  #define I2S_DIN_PIN     -1      // MCU Data In: connect to periph. DATA OUT (DOUT D SD)
  // ===================== SD MMC PINS ============================
  // ESP32S3 allows almost any GPIOs for any particular needs
  #define SDMMC_CMD 38
  #define SDMMC_CLK 39
  #define SDMMC_D0  10
  #define SDMMC_D1  11
  #define SDMMC_D2  12
  #define SDMMC_D3  13

  #define BTN0_PIN 	14
  #define ENC0_A_PIN 	15
  #define ENC0_B_PIN 	16

  #define BTN1_PIN 	41
  #define BTN2_PIN 	42
  #define BTN3_PIN 	2
  #define BTN4_PIN  1
  #define BTN5_PIN 	0
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
  // ===================== SYNTHESIZER ============================
  #define MAX_VOICES 11
  #define MAX_VOICES_PER_NOTE 2
  #define TIMING_CORRECTION 0.71f // to estimate polyphony, as P4 is faster than S3

  // ===================== MIDI PINS ==============================
  #define MIDI_IN         50      // if USE_MIDI_STANDARD is selected as MIDI_IN, this pin receives MIDI messages
  // ===================== I2S PINS ===============================
  #define I2S_BCLK_PIN    48      // I2S BIT CLOCK pin (BCL BCK CLK)
  #define I2S_DOUT_PIN    47      // MCU Data Out: connect to periph. DATA IN (DIN D DAT)
  #define I2S_WCLK_PIN    46      // I2S WORD CLOCK pin (WCK WCL LCK)
  #define I2S_DIN_PIN     49      // MCU Data In: connect to periph. DATA OUT (DOUT D SD)  
  // ===================== SD MMC PINS ============================
  // ESP32P4 SPI slot 0 is LOCKED to hardware pins, DON't change it unless you switch to SLOT 1
  // slot 0 defaults:
  #define SDMMC_D0  39
  #define SDMMC_D1  40
  #define SDMMC_D2  41
  #define SDMMC_D3  42
  #define SDMMC_CLK 43
  #define SDMMC_CMD 44 
  
  #define BTN0_PIN 	35
  #define ENC0_A_PIN 	23
  #define ENC0_B_PIN 	22

  #define BTN1_PIN 	6
  #define BTN2_PIN 	9
  #define BTN3_PIN 	10
  #define BTN4_PIN  11
  #define BTN5_PIN  11 

#endif
 


// ===================== GUI SETTINGS ===========================

#define ENABLE_GUI

#define ACTIVE_STATE  LOW   // LOW = switch connects to GND, HIGH = switch connects to 3V3


//#define USE_SD // where to store patches and configs: comment out to use LittleFS on internal flash

#ifdef ENABLE_GUI

  // only one of SPI and I2C can be set to 1
  #define OLED_USE_SPI  1 // 7 pins displays
  #define OLED_USE_I2C  0 // 4 pins displays

  // display signal pins
  #if (OLED_USE_I2C==1)
    #define OLED_I2C_SCL 9 // -1 = autodetect
    #define OLED_I2C_SDA 8 // -1 = autodetect
  #elif (OLED_USE_SPI==1) 
    #define OLED_PIN_CS 4 
    #define OLED_PIN_DC 5 
    #define OLED_PIN_RST 3   // -1 = unused 
    #define OLED_SPI_SCK 18 // SCL (SCK) GPIO
    #define OLED_SPI_MOSI 19 // SDA (MOSI) GPIO
  #endif

  #define OLED_WIDTH    128
  #define OLED_HEIGHT   64

  // choose ONE display driver
  // #define OLED_SSD1306  1
  #define OLED_SH1106   1
  // #define OLED_SH1107   1
  // #define LCD_ST7565    1

#endif

// ===================== COMPILE GUARDS =========================
#ifndef BOARD_HAS_PSRAM
#error "OPI-PSRAM or better is required, enable it in the [Tools] -> [PSRAM] menu"
#endif

#if MIDI_IN_DEV == USE_USB_MIDI_DEVICE 
  #if ARDUINO_USB_MODE != 0
  #error "[Tools] -> [USB Mode] should be set to [USB-OTG (TinyUSB)] if you want to use USB MIDI Device"
  #endif
#endif

#pragma once



#include "esp_heap_caps.h"



inline  IRAM_ATTR __attribute__((always_inline)) float fclamp(float smp, float a, float b) {
   if (smp>b) return b;
   if (smp>=a) return smp;
   return a; 
}

inline  IRAM_ATTR __attribute__((always_inline))  float midiNoteToHz(uint8_t note) {
    return 440.0f * powf(2.0f, (int(note) - 69) / 12.0f);
}

inline  IRAM_ATTR __attribute__((always_inline)) float saturate_cubic(float x) {
 // Soft clipping cubic saturator
    if (x > 1.0f) return 1.0f;
    if (x < -1.0f) return -1.0f;
    return  1.5f * (x - (x * x * x) * 0.3334f);
}

inline float __attribute__((always_inline)) IRAM_ATTR fast_floorf(float x) {
    int i = (int)x - (int)(i>x);
    return (float)i;
}

inline float __attribute__((always_inline)) IRAM_ATTR wrap01(float x)  {
    return x - fast_floorf(x);
}


void logMemoryStats(const char* tag = "MEM") {
    uint32_t dram_free  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t dram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    ESP_LOGI(tag, "Internal RAM free: %d bytes (largest block: %d)", dram_free, dram_largest);
    ESP_LOGI(tag, "PSRAM free: %d bytes (largest block: %d)", psram_free, psram_largest);
}


inline uint32_t lfsr_ = 0x12345678;

inline float randomFloat() {
    lfsr_ ^= lfsr_ << 13;
    lfsr_ ^= lfsr_ >> 17;
    lfsr_ ^= lfsr_ << 5;
    return ((lfsr_ & 0xFFFFFF) / float(0x800000) - 1.f);
}

/*
inline IRAM_ATTR __attribute__((always_inline)) float fastJitter(float amt) {
    static uint32_t rngState_ = 22222; // some seed
    rngState_ = rngState_ * 1664525 + 1013904223; // classic LCG
    return ((rngState_ & 0xFFFF) / 65536.f - 0.5f) * amt; // ±0.001 jitter
}

inline IRAM_ATTR __attribute__((always_inline)) float fastJitterLP(float amt) {
    static uint32_t rngState_ = 22222; // some seed
    rngState_ = rngState_ * 1664525 + 1013904223;
    float raw = ((rngState_ & 0xFFFF) / 65536.f - 0.5f) * amt; // ±0.001
    float jitter += (raw - jitter) * 0.01f; // smooth factor, tweak 0.005..0.02
    return jitter;
}
*/
// MidiClock.h

#pragma once
#include <stdint.h>
#include <functional>
#include "esp_timer.h"

class MidiClock {
public:
    enum SyncSource : uint8_t {
        SYNC_EXTERNAL,
        SYNC_MICROS,
        SYNC_ISR,
        SYNC_ESP_TIMER
    };

    using TickCallback = std::function<void(uint32_t tick)>;

    MidiClock();
    ~MidiClock();

    void begin(SyncSource source = SYNC_ESP_TIMER, uint8_t ppqn = 96, float bpm = 120.0f);
    void setBPM(float bpm);
    float getBPM() const;
    void setPPQN(uint8_t ppqn);
    uint8_t getPPQN() const;
    void setSyncSource(SyncSource source);
    void onTick(TickCallback cb);
    void start();
    void stop();
    void reset();
    void process();
    void externalTick();
    void IRAM_ATTR isrTick();
    bool isRunning() const;
    uint32_t getTickCount() const;
	
	// external midi sync
	void startSync();
	void stopSync();
	void continueSync();
	void resetSync();


private:
    volatile bool tickFlag_ = false;
    SyncSource syncSource_ = SYNC_MICROS;
    uint8_t ppqn_ = 96;
    uint8_t external_ppqn_ = 24;
    uint32_t ticksPerBpmUpdate_ = 12;
    uint32_t tickAccumulated_ = 0;
    uint32_t tickCountForBpm_ = 0;
    float bpm_ = 120.0f;
    uint32_t tickCount_ = 0;
    bool running_ = false;
    uint32_t tickIntervalMicros_ = 0;
    uint32_t lastMicros_ = 0;
    TickCallback tickCallback_ = nullptr;

    esp_timer_handle_t espTimerHandle_ = nullptr;

    static void IRAM_ATTR espTimerCallback(void* arg);

    void produceTick();
    void calculateTickInterval();
};

inline MidiClock::MidiClock()
    : syncSource_(SYNC_MICROS),
      ppqn_(96),
      bpm_(120.0f),
      tickCount_(0),
      running_(false),
      tickIntervalMicros_(0),
      lastMicros_(0),
      tickCallback_(nullptr),
      espTimerHandle_(nullptr)
{}

inline MidiClock::~MidiClock() {
    if (espTimerHandle_) {
        esp_timer_stop(espTimerHandle_);
        esp_timer_delete(espTimerHandle_);
        espTimerHandle_ = nullptr;
    }
}

inline void MidiClock::calculateTickInterval() {
    if (bpm_ <= 0 || ppqn_ == 0) {
        tickIntervalMicros_ = 0;
        return;
    }
    tickIntervalMicros_ = static_cast<uint32_t>(60000000.0f / (bpm_ * ppqn_));
}

inline void MidiClock::begin(SyncSource source, uint8_t newPpqn, float newBpm) {
    syncSource_ = source;
    ppqn_ = newPpqn;
    bpm_ = newBpm;
    calculateTickInterval();
    tickCount_ = 0;
    running_ = false;
    lastMicros_ = 0;

    if (espTimerHandle_) {
        esp_timer_stop(espTimerHandle_);
        esp_timer_delete(espTimerHandle_);
        espTimerHandle_ = nullptr;
    }

    if (syncSource_ == SYNC_ESP_TIMER) {
        esp_timer_create_args_t timerArgs = {};
        timerArgs.callback = &MidiClock::espTimerCallback;
        timerArgs.arg = this;
        timerArgs.dispatch_method = ESP_TIMER_TASK; // safe for long callbacks

        esp_timer_create(&timerArgs, &espTimerHandle_);
    }
}

inline void MidiClock::setBPM(float newBpm) {
    bpm_ = newBpm;
    calculateTickInterval();

    if (syncSource_ == SYNC_ESP_TIMER && espTimerHandle_) {
        esp_timer_stop(espTimerHandle_);
        uint64_t period_us = 60000000ULL / static_cast<uint64_t>(bpm_ * ppqn_);
        esp_timer_start_periodic(espTimerHandle_, period_us);
    }
}

inline void MidiClock::setPPQN(uint8_t newPpqn) {
    ppqn_ = newPpqn;
    calculateTickInterval();

    if (syncSource_ == SYNC_ESP_TIMER && espTimerHandle_) {
        esp_timer_stop(espTimerHandle_);
        uint64_t period_us = 60000000ULL / static_cast<uint64_t>(bpm_ * ppqn_);
        esp_timer_start_periodic(espTimerHandle_, period_us);
    }
}

inline void MidiClock::start() {
    if (running_) return;
    running_ = true;
    lastMicros_ = micros();

    if (syncSource_ == SYNC_ESP_TIMER && espTimerHandle_) {
        uint64_t period_us = 60000000ULL / static_cast<uint64_t>(bpm_ * ppqn_);
        esp_timer_start_periodic(espTimerHandle_, period_us);
    }
}

inline void MidiClock::stop() {
    running_ = false;

    if (syncSource_ == SYNC_ESP_TIMER && espTimerHandle_) {
        esp_timer_stop(espTimerHandle_);
    }
}

inline void MidiClock::reset() {
    tickCount_ = 0;
    lastMicros_ = micros();
}

inline void MidiClock::produceTick() {
    ++tickCount_;
    if (tickCallback_) {
        tickCallback_(tickCount_);
    }
}

inline void MidiClock::externalTick() {
    uint32_t now = micros();
    if (lastMicros_ == 0) {
        lastMicros_ = now;
        tickAccumulated_ = 0;
        tickCountForBpm_ = 0;
        return;
    }

    uint32_t delta = now - lastMicros_;
    lastMicros_ = now;

    tickAccumulated_ += delta;
    tickCountForBpm_++;

    if (tickCountForBpm_ >= ticksPerBpmUpdate_) {
        float new_bpm = 60.0f * 1000000.0f * tickCountForBpm_ / (tickAccumulated_ * external_ppqn_);
        bpm_ = new_bpm;
        calculateTickInterval();
        tickAccumulated_ = 0;
        tickCountForBpm_ = 0;
    }

    produceTick();
}

inline void IRAM_ATTR MidiClock::isrTick() {
    tickFlag_ = true;  // atomic write
}

inline void MidiClock::process() {
    if (!running_) return;

    switch (syncSource_) {
        case SYNC_MICROS: {
            uint32_t now = micros();
            if (tickIntervalMicros_ == 0) return;
            while ((now - lastMicros_) >= tickIntervalMicros_) {
                lastMicros_ += tickIntervalMicros_;
                produceTick();
            }
            break;
        }
        case SYNC_EXTERNAL:
            // externalTick() called externally
            break;
        case SYNC_ISR:
            if (tickFlag_) {
                tickFlag_ = false;
                produceTick();
            }
            break;
        case SYNC_ESP_TIMER:
            // esp_timer calls produceTick directly, no process() needed here
            break;
    }
}

inline uint32_t MidiClock::getTickCount() const {
    return tickCount_;
}

inline bool MidiClock::isRunning() const {
    return running_;
}

inline float MidiClock::getBPM() const {
    return bpm_;
}

inline uint8_t MidiClock::getPPQN() const {
    return ppqn_;
}

inline void MidiClock::setSyncSource(SyncSource source) {
    if (syncSource_ == source) return;

    // stop current timer if any
    if (espTimerHandle_) {
        esp_timer_stop(espTimerHandle_);
        esp_timer_delete(espTimerHandle_);
        espTimerHandle_ = nullptr;
    }

    syncSource_ = source;

    if (syncSource_ == SYNC_ESP_TIMER) {
        esp_timer_create_args_t timerArgs = {};
        timerArgs.callback = &MidiClock::espTimerCallback;
        timerArgs.arg = this;
        timerArgs.dispatch_method = ESP_TIMER_TASK;
        esp_timer_create(&timerArgs, &espTimerHandle_);
    }
}

inline void MidiClock::onTick(TickCallback cb) {
    tickCallback_ = std::move(cb);
}

inline void IRAM_ATTR MidiClock::espTimerCallback(void* arg) {
    auto* self = static_cast<MidiClock*>(arg);
    if (self && self->running_) {
        self->produceTick();
    }
}

inline void MidiClock::startSync() {
    resetSync();
    running_ = true;
    lastMicros_ = micros();
}

inline void MidiClock::stopSync() {
    running_ = false;
}

inline void MidiClock::continueSync() {
    running_ = true;
    lastMicros_ = micros();
}

inline void MidiClock::resetSync() {
    tickCount_ = 0;
    lastMicros_ = micros();
}

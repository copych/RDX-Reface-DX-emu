// RDX_CPU_Budget.h – patch-local, measured active-voice prediction (Core1 only).
#pragma once
#include <Arduino.h>
#include <math.h>
#include "config.h"

class RDX_CPUBudget {
public:
    static constexpr uint32_t PERIOD_MS = 12;
    static constexpr uint32_t RECOVERY_MS = 1500;
    static constexpr uint32_t RESTORE_STEP_MS = 250;
    static constexpr float SAFETY_US = 20.0f;
    static constexpr float RESTORE_MARGIN_US = 15.0f;
    static constexpr uint8_t RESTORE_CONFIRMATIONS = 3;

    bool due(uint32_t now) const { return (uint32_t)(now - lastUpdate_) >= PERIOD_MS; }
    // Debug: incremental activation cost, NOT the cost of a complete voice slot.
    float voiceCostUs() const { return haveActivationCost_ ? activationCostUs_ : 0.0f; }
    float renderBaselineUs() const { return haveIdle_[modelSlots_] ? idleUs_[modelSlots_] : 0.0f; }

    // Call on Core1 on every patch change / DSP-affecting parameter revision.
    void resetModel() {
        for (int i = 0; i <= MAX_VOICES; ++i) {
            idleUs_[i] = 0.0f;
            haveIdle_[i] = false;
        }
        activationCostUs_ = 0.0f;
        slotCostUs_ = 0.0f;
        haveActivationCost_ = haveSlotCost_ = false;
        havePrevious_ = false;
        restoreCount_ = 0;
        haveReduced_ = false;
        lastReduction_ = lastChange_ = 0;
        modelSlots_ = MAX_VOICES;
    }

    template<class Synth>
    void update(uint32_t now, int renderUs, int fixedUs, int fxUs,
                int active, int slots, bool polyMode, uint32_t overrunRecord,
                Synth& synth) {
        lastUpdate_ = now;
        if (polyMode && !haveReduced_ && !haveActivationCost_ && slots < MAX_VOICES) {
            synth.setVoiceLimit(MAX_VOICES);
            return;
        }
        if (renderUs <= 0 || fixedUs < 0 || fxUs < 0 ||
            slots < 1 || slots > MAX_VOICES || active < 0 || active > slots) return;
        modelSlots_ = slots;

        const float deadline = 1000000.0f * DMA_BUFFER_LEN / SAMPLE_RATE;
        const float target = deadline - SAFETY_US;
        const float measured = (float)renderUs + fixedUs + fxUs;

        // Single completed-block emergency record, packed by Core0.
        // Ignore events from a retired slot configuration.
        const float overrunUs = (float)(overrunRecord >> 8);
        const bool emergency = overrunRecord != 0 &&
            (int)(overrunRecord & 0xFFU) == slots && overrunUs >= target;

        // Learn ACTIVE voice increment from TWO measurements with the same
        // rendered-slot count. Neither render / slots nor render / active is
        // an estimate of the incremental cost of activating a voice.
        if (havePrevious_ && prevSlots_ == slots &&
            (uint32_t)(now - prevTime_) <= 120 && active != prevActive_) {
            const float delta = (float)(renderUs - prevRenderUs_) /
                                (float)(active - prevActive_);
            // Reject unrelated transients and unmatched snapshots; do not
            // pollute the model with a negative or implausibly large slope.
            if (delta >= 8.0f && delta <= 160.0f) {
                if (!haveActivationCost_) activationCostUs_ = delta;
                else activationCostUs_ = 0.75f * activationCostUs_ + 0.25f * delta;
                haveActivationCost_ = true;
            }
        }

        // Silent observations give the idle cost for THIS exact slot count.
        // Once activation cost is known, active observations may also estimate
        // the intercept, but only when compatible with an existing baseline.
        if (active == 0) {
            const float idle = (float)renderUs;
            if (!haveIdle_[slots]) idleUs_[slots] = idle;
            else idleUs_[slots] = 0.875f * idleUs_[slots] + 0.125f * idle;
            haveIdle_[slots] = true;
        } else if (haveActivationCost_) {
            const float idle = (float)renderUs - activationCostUs_ * active;
            if (idle > 0.0f && (!haveIdle_[slots] ||
                fabsf(idle - idleUs_[slots]) <= 140.0f)) {
                if (!haveIdle_[slots]) idleUs_[slots] = idle;
                else idleUs_[slots] = 0.9375f * idleUs_[slots] + 0.0625f * idle;
                haveIdle_[slots] = true;
            }
        }

        // Adjacent-slot samples are useful only at identical activity. Keep
        // the previous observation across an intentional slot change, but
        // only when the controller knows that it did not retire an active slot.
        if (havePrevious_ && prevSlots_ != slots &&
            (prevSlots_ + 1 == slots || slots + 1 == prevSlots_) &&
            active == prevActive_ && (uint32_t)(now - prevTime_) <= 120) {
            float delta = (float)(renderUs - prevRenderUs_);
            if (slots < prevSlots_) delta = -delta;
            if (delta >= 10.0f && delta <= 500.0f) {
                if (!haveSlotCost_) slotCostUs_ = delta;
                else slotCostUs_ = 0.75f * slotCostUs_ + 0.25f * delta;
                haveSlotCost_ = true;
            }
        }
        prevRenderUs_ = renderUs;
        prevActive_ = active;
        prevSlots_ = slots;
        prevTime_ = now;
        havePrevious_ = true;

        // The slot estimate is ONLY used to project a never-measured slot
        // count. A measured baseline for that count always takes precedence.
        const float idleSlot = haveSlotCost_ ? slotCostUs_ :
                               (haveIdle_[slots] ? idleUs_[slots] / slots :
                                (float)renderUs / slots);

        // Predict FULL occupancy in the current configuration, not merely
        // the next note. Predict only after real same-slot activation data;
        // an empty patch must not lose polyphony just because render / slots
        // is large. The snapshot-based prediction is advisory; the coherent
        // block-overrun event remains authoritative.
        bool haveFullPrediction = haveActivationCost_ && haveIdle_[slots];
        float fullCurrent = 0.0f;
        if (haveFullPrediction)
            fullCurrent = idleUs_[slots] + activationCostUs_ * slots + fixedUs + fxUs;

        if (emergency || measured >= target ||
            (slots > 1 && haveFullPrediction && fullCurrent > target)) {
            restoreCount_ = 0;
            if (slots > 1) {
                // One slot per predictive adjustment. A genuine excess can
                // request more than one, but the model cannot veto action.
                const float worst = emergency && overrunUs > measured ? overrunUs : measured;
                int cut = 1;
                if (emergency || measured >= target) {
                    cut = (int)ceilf((worst - target) / (idleSlot > 1.0f ? idleSlot : 1.0f));
                    if (cut < 1) cut = 1;
                }
                int next = slots - cut;
                if (next < 1) next = 1;
                synth.setVoiceLimit(next);
                lastReduction_ = lastChange_ = now;
                haveReduced_ = true;
                // Preserve model from this patch, but don't compare adjacent
                // observations if a retired voice was sounding.
                if (active > next) havePrevious_ = false;
                ESP_LOGW("CPU_BUDGET", "%s slots=%d->%d active=%d measured=%.0f target=%.0f full=%.0f actCost=%.1f idle=%.0f overrun=%.0f",
                         emergency || measured >= target ? "emergency" : "predict",
                         slots, next, active, measured, target, fullCurrent,
                         activationCostUs_, idleUs_[slots], overrunUs);
            }
            return;
        }

        if (slots >= MAX_VOICES ||
            (haveReduced_ && (uint32_t)(now - lastReduction_) < RECOVERY_MS) ||
            (uint32_t)(now - lastChange_) < RESTORE_STEP_MS) {
            restoreCount_ = 0;
            return;
        }

        // Restoration is tested at FULL occupancy of the proposed limit.
        // This prevents 7->8->7 oscillation on an expensive patch when the
        // CURRENT seven-voice workload happens to be light.
        const int next = slots + 1;
        const float nextIdle = haveIdle_[next] ? idleUs_[next] :
                               (haveIdle_[slots] ? idleUs_[slots] + idleSlot :
                                (float)renderUs - activationCostUs_ * active + idleSlot);
        const float predictedNext = haveActivationCost_ ?
            nextIdle + activationCostUs_ * next + fixedUs + fxUs : 0.0f;
        if (haveActivationCost_ && predictedNext + RESTORE_MARGIN_US < target &&
            measured + RESTORE_MARGIN_US < target) {
            if (restoreCount_ < RESTORE_CONFIRMATIONS) ++restoreCount_;
            if (restoreCount_ >= RESTORE_CONFIRMATIONS) {
                synth.setVoiceLimit(next);
                lastChange_ = now;
                restoreCount_ = 0;
                ESP_LOGI("CPU_BUDGET", "restore slots=%d->%d active=%d measured=%.0f nextFull=%.0f target=%.0f actCost=%.1f",
                         slots, next, active, measured, predictedNext, target, activationCostUs_);
            }
        } else restoreCount_ = 0;
    }

private:
    uint32_t lastUpdate_ = 0, lastReduction_ = 0, lastChange_ = 0, prevTime_ = 0;
    int prevRenderUs_ = 0, prevActive_ = 0, prevSlots_ = 0, modelSlots_ = MAX_VOICES;
    uint8_t restoreCount_ = 0;
    float idleUs_[MAX_VOICES + 1] = {};
    bool haveIdle_[MAX_VOICES + 1] = {};
    float activationCostUs_ = 0.0f, slotCostUs_ = 0.0f;
    bool haveActivationCost_ = false, haveSlotCost_ = false;
    bool haveReduced_ = false, havePrevious_ = false;
};

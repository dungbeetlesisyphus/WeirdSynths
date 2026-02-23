#pragma once

#include <rack.hpp>

namespace nerve {

class SlewSmoother {
public:
    void reset() { value = 0.f; }
    void reset(float v) { value = v; }

    float process(float target, float smoothTime, float sampleTime) {
        if (smoothTime < 0.001f) {
            value = target;
        } else {
            float alpha = sampleTime / (smoothTime * 0.5f + sampleTime);
            value += alpha * (target - value);
        }
        return value;
    }

    float getValue() const { return value; }

private:
    float value = 0.f;
};

class TimeoutTracker {
public:
    void setTimeoutSeconds(float seconds) { timeoutSeconds = seconds; }

    void tick(float sampleTime) { elapsed += sampleTime; }

    void reset() { elapsed = 0.f; }

    bool isTimedOut() const { return elapsed > timeoutSeconds; }

    float getElapsed() const { return elapsed; }

private:
    float elapsed = 999.f;
    float timeoutSeconds = 0.5f;
};

}  // namespace nerve

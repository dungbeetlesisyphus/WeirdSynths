#pragma once

#include <rack.hpp>
#include <cmath>

namespace skull {

// ─────────────────────────────────────────────────────────
// Simple noise generator (xorshift32)
// ─────────────────────────────────────────────────────────
struct NoiseGen {
    uint32_t state = 123456789;
    float next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float)(int32_t)state / (float)INT32_MAX;
    }
};

// ─────────────────────────────────────────────────────────
// Envelope — exponential decay with adjustable time
// ─────────────────────────────────────────────────────────
struct DecayEnvelope {
    float value = 0.f;
    float decayRate = 0.f;

    void trigger(float level = 1.f) {
        value = level;
    }

    void setDecay(float decaySeconds, float sampleRate) {
        if (decaySeconds < 0.001f) {
            decayRate = 0.f;
        } else {
            // Time constant for exponential decay to ~0.001
            decayRate = std::exp(-6.9f / (decaySeconds * sampleRate));
        }
    }

    float process() {
        value *= decayRate;
        if (value < 1e-6f) value = 0.f;
        return value;
    }

    bool isActive() const { return value > 1e-6f; }
};

// ─────────────────────────────────────────────────────────
// Schmitt Trigger for face gesture detection
// ─────────────────────────────────────────────────────────
struct GestureTrigger {
    bool state = false;

    // Returns true on rising edge (gesture start)
    bool process(float input, float threshold) {
        float hiThresh = threshold;
        float loThresh = threshold * 0.6f;
        if (!state && input > hiThresh) {
            state = true;
            return true;
        }
        if (state && input < loThresh) {
            state = false;
        }
        return false;
    }
};

// ─────────────────────────────────────────────────────────
// One-pole lowpass filter
// ─────────────────────────────────────────────────────────
struct OnePole {
    float y = 0.f;

    float process(float x, float cutoff, float sampleRate) {
        float w = 2.f * M_PI * cutoff / sampleRate;
        float a = w / (1.f + w);
        y += a * (x - y);
        return y;
    }

    void reset() { y = 0.f; }
};

// ─────────────────────────────────────────────────────────
// Simple SVF (State Variable Filter) for hat filtering
// ─────────────────────────────────────────────────────────
struct SVFilter {
    float low = 0.f, band = 0.f, high = 0.f;

    void process(float input, float cutoff, float resonance, float sampleRate) {
        float f = 2.f * std::sin(M_PI * rack::math::clamp(cutoff / sampleRate, 0.f, 0.49f));
        float q = 1.f / rack::math::clamp(resonance, 0.5f, 20.f);
        high = input - low - q * band;
        band += f * high;
        low += f * band;
        // Prevent NaN/Inf runaway
        if (!std::isfinite(low)) low = 0.f;
        if (!std::isfinite(band)) band = 0.f;
        if (!std::isfinite(high)) high = 0.f;
    }

    void reset() { low = band = high = 0.f; }
};

// ─────────────────────────────────────────────────────────
// Bit crusher for digital kit
// ─────────────────────────────────────────────────────────
inline float bitCrush(float input, float bits) {
    float steps = std::pow(2.f, bits);
    return std::round(input * steps) / steps;
}

// ─────────────────────────────────────────────────────────
// Soft clipper for warmth
// ─────────────────────────────────────────────────────────
inline float softClip(float x) {
    if (x > 1.f) return 1.f;
    if (x < -1.f) return -1.f;
    return 1.5f * x - 0.5f * x * x * x;
}

// ═════════════════════════════════════════════════════════
// KICK VOICE
// ═════════════════════════════════════════════════════════
struct KickVoice {
    float phase = 0.f;
    DecayEnvelope ampEnv;
    DecayEnvelope pitchEnv;
    float velocity = 1.f;

    void trigger(float vel = 1.f) {
        velocity = vel;
        phase = 0.f;
        ampEnv.trigger(vel);
        pitchEnv.trigger(1.f);
    }

    // kit: 0=analog, 0.5=digital, 1=physical
    float process(float kit, float decay, float tone, float sampleRate) {
        ampEnv.setDecay(decay * 0.6f + 0.1f, sampleRate);

        float pitchDecayTime;
        if (kit < 0.33f) {
            // 808 analog: slow pitch sweep
            pitchDecayTime = 0.08f;
        } else if (kit < 0.66f) {
            // Digital: faster, snappier
            pitchDecayTime = 0.04f;
        } else {
            // Physical: medium pitch, more harmonics
            pitchDecayTime = 0.06f;
        }
        pitchEnv.setDecay(pitchDecayTime, sampleRate);

        float pitchMod = pitchEnv.process();
        float amp = ampEnv.process();

        // Base frequency: 40-80Hz, pitch sweep from 200-400Hz
        float basePitch = 40.f + tone * 40.f;
        float startPitch = 200.f + tone * 200.f;
        float freq = basePitch + pitchMod * (startPitch - basePitch);

        phase += freq / sampleRate;
        if (phase > 1.f) phase -= 1.f;

        float out = std::sin(2.f * M_PI * phase);

        // Kit-specific character
        if (kit >= 0.33f && kit < 0.66f) {
            // Digital: add bit crushing
            out = bitCrush(out, 6.f + tone * 4.f);
        } else if (kit >= 0.66f) {
            // Physical: add odd harmonics for membrane feel
            out += 0.3f * std::sin(2.f * M_PI * phase * 2.3f) * pitchMod;
            out += 0.15f * std::sin(2.f * M_PI * phase * 3.7f) * pitchMod * pitchMod;
        }

        return softClip(out * amp * 5.f);
    }

    bool isActive() const { return ampEnv.isActive(); }
};

// ═════════════════════════════════════════════════════════
// SNARE VOICE
// ═════════════════════════════════════════════════════════
struct SnareVoice {
    float phase = 0.f;
    DecayEnvelope toneEnv;
    DecayEnvelope noiseEnv;
    NoiseGen noise;
    SVFilter noiseFilt;
    float velocity = 1.f;

    void trigger(float vel = 1.f) {
        velocity = vel;
        phase = 0.f;
        toneEnv.trigger(vel * 0.6f);
        noiseEnv.trigger(vel);
        noiseFilt.reset();
    }

    float process(float kit, float decay, float tone, float sampleRate) {
        toneEnv.setDecay(0.08f + decay * 0.12f, sampleRate);
        noiseEnv.setDecay(0.1f + decay * 0.3f, sampleRate);

        float toneAmp = toneEnv.process();
        float noiseAmp = noiseEnv.process();

        // Tone layer: ~180-250Hz
        float freq = 180.f + tone * 70.f;
        phase += freq / sampleRate;
        if (phase > 1.f) phase -= 1.f;
        float toneOut = std::sin(2.f * M_PI * phase) * toneAmp;

        // Noise layer: filtered
        float n = noise.next();
        float filterCutoff = 3000.f + tone * 5000.f;
        noiseFilt.process(n, filterCutoff, 2.f, sampleRate);
        float noiseOut = noiseFilt.band * noiseAmp;

        // Kit character
        float mix;
        if (kit < 0.33f) {
            // Analog: warm, balanced
            mix = toneOut * 0.4f + noiseOut * 0.6f;
        } else if (kit < 0.66f) {
            // Digital: crushed noise, less tone
            noiseOut = bitCrush(noiseOut, 5.f + tone * 3.f);
            mix = toneOut * 0.2f + noiseOut * 0.8f;
        } else {
            // Physical: more tone, body
            mix = toneOut * 0.55f + noiseOut * 0.45f;
            // Add second harmonic for body
            mix += 0.15f * std::sin(2.f * M_PI * phase * 2.f) * toneAmp;
        }

        return softClip(mix * 5.f);
    }

    bool isActive() const { return toneEnv.isActive() || noiseEnv.isActive(); }
};

// ═════════════════════════════════════════════════════════
// HI-HAT VOICE (used for both closed and open)
// ═════════════════════════════════════════════════════════
struct HiHatVoice {
    DecayEnvelope ampEnv;
    NoiseGen noise;
    SVFilter filt;
    // For 808-style: square wave oscillators
    float phases[6] = {};
    float velocity = 1.f;

    void trigger(float vel = 1.f) {
        velocity = vel;
        ampEnv.trigger(vel);
        filt.reset();
    }

    // isOpen: false = closed hat, true = open hat
    float process(float kit, float decay, float tone, bool isOpen, float sampleRate) {
        float decayTime = isOpen ? (0.15f + decay * 0.5f) : (0.02f + decay * 0.08f);
        ampEnv.setDecay(decayTime, sampleRate);

        float amp = ampEnv.process();
        float out;

        if (kit < 0.33f) {
            // Analog 808: metallic square wave oscillators
            const float freqs[6] = {204.f, 270.f, 330.f, 390.f, 510.f, 540.f};
            float sum = 0.f;
            for (int i = 0; i < 6; i++) {
                phases[i] += freqs[i] * (0.8f + tone * 0.4f) / sampleRate;
                if (phases[i] > 1.f) phases[i] -= 1.f;
                sum += (phases[i] < 0.5f ? 1.f : -1.f);
            }
            out = sum / 6.f;
            // Highpass the result
            float cutoff = 7000.f + tone * 5000.f;
            filt.process(out, cutoff, 3.f, sampleRate);
            out = filt.high;
        } else if (kit < 0.66f) {
            // Digital: crushed noise
            float n = noise.next();
            float cutoff = 8000.f + tone * 6000.f;
            filt.process(n, cutoff, 2.5f, sampleRate);
            out = bitCrush(filt.high, 4.f + tone * 4.f);
        } else {
            // Physical: filtered noise with resonance
            float n = noise.next();
            float cutoff = 6000.f + tone * 8000.f;
            float reso = 4.f + tone * 6.f;
            filt.process(n, cutoff, reso, sampleRate);
            out = filt.band * 0.5f + filt.high * 0.5f;
        }

        return softClip(out * amp * 5.f);
    }

    bool isActive() const { return ampEnv.isActive(); }
};

// ═════════════════════════════════════════════════════════
// CRASH CYMBAL — triggered by tongue
// ═════════════════════════════════════════════════════════
struct CrashVoice {
    DecayEnvelope ampEnv;
    NoiseGen noise;
    SVFilter filt;
    SVFilter filt2;
    float phases[4] = {};
    float velocity = 1.f;

    void trigger(float vel = 1.f) {
        velocity = vel;
        ampEnv.trigger(vel);
        filt.reset();
        filt2.reset();
    }

    float process(float kit, float decay, float tone, float sampleRate) {
        // Crash has a long decay
        float decayTime = 0.5f + decay * 2.f;
        ampEnv.setDecay(decayTime, sampleRate);

        float amp = ampEnv.process();
        float out;

        if (kit < 0.33f) {
            // Analog: dense metallic partials
            const float freqs[4] = {340.f, 460.f, 587.f, 720.f};
            float sum = 0.f;
            for (int i = 0; i < 4; i++) {
                phases[i] += freqs[i] * (0.9f + tone * 0.2f) / sampleRate;
                if (phases[i] > 1.f) phases[i] -= 1.f;
                sum += (phases[i] < 0.5f ? 1.f : -1.f);
            }
            // Mix metallic + noise
            float n = noise.next();
            float cutoff = 5000.f + tone * 7000.f;
            filt.process(sum / 4.f + n * 0.5f, cutoff, 1.5f, sampleRate);
            out = filt.band * 0.4f + filt.high * 0.6f;
        } else if (kit < 0.66f) {
            // Digital: washed out bit-crushed shimmer
            float n = noise.next();
            float cutoff = 4000.f + tone * 8000.f;
            filt.process(n, cutoff, 2.f, sampleRate);
            out = bitCrush(filt.high, 6.f + tone * 3.f);
        } else {
            // Physical: resonant plate-like
            float n = noise.next();
            float cutoff = 3000.f + tone * 9000.f;
            filt.process(n, cutoff, 6.f + tone * 4.f, sampleRate);
            filt2.process(filt.band, cutoff * 1.5f, 3.f, sampleRate);
            out = filt.band * 0.3f + filt2.band * 0.3f + filt.high * 0.4f;
        }

        return softClip(out * amp * 5.f);
    }

    bool isActive() const { return ampEnv.isActive(); }
};

// ═════════════════════════════════════════════════════════
// SKULL DRUM ENGINE — all 5 voices + stereo mix
// ═════════════════════════════════════════════════════════
struct DrumEngine {
    KickVoice kick;
    SnareVoice snare;
    HiHatVoice closedHat;
    HiHatVoice openHat;
    CrashVoice crash;

    GestureTrigger kickTrig;
    GestureTrigger snareTrig;
    GestureTrigger chTrig;
    GestureTrigger ohTrig;
    GestureTrigger crashTrig;

    float mixL = 0.f;
    float mixR = 0.f;
    float kickOut = 0.f;
    float snareOut = 0.f;
    float chOut = 0.f;
    float ohOut = 0.f;
    float crashOut = 0.f;

    // Process one sample
    // faceData values are 0-1 normalized
    void process(
        float blinkL, float blinkR, float jaw, float browL, float browR,
        float mouthW, float headX, float headY, float expression,
        float tongue,
        float kit, float sensitivity, float decay, float tone, float pan, float level,
        float sampleRate
    ) {
        float thresh = 1.f - sensitivity;

        // Detect triggers from face gestures
        float blinkVal = std::max(blinkL, blinkR);
        bool kickFired = kickTrig.process(blinkVal, thresh);
        bool snareFired = snareTrig.process(jaw, thresh);
        bool chFired = chTrig.process(browL, thresh);
        bool ohFired = ohTrig.process(browR, thresh);
        // Tongue blendshape is much weaker — use half threshold
        bool crashFired = crashTrig.process(tongue, thresh * 0.3f);

        float vel = 0.5f + expression * 0.5f;

        if (kickFired) kick.trigger(vel);
        if (snareFired) snare.trigger(vel);
        if (chFired) closedHat.trigger(vel);
        if (ohFired) openHat.trigger(vel);
        if (crashFired) crash.trigger(vel);

        // Snare decay modulated by mouth width
        float snareDec = decay + mouthW * 0.5f;

        // Process voices — skip inactive voices for CPU savings
        kickOut = kick.isActive() ? kick.process(kit, decay, tone, sampleRate) : 0.f;
        snareOut = snare.isActive() ? snare.process(kit, snareDec, tone, sampleRate) : 0.f;
        chOut = closedHat.isActive() ? closedHat.process(kit, decay, tone, false, sampleRate) : 0.f;
        ohOut = openHat.isActive() ? openHat.process(kit, decay, tone, true, sampleRate) : 0.f;
        crashOut = crash.isActive() ? crash.process(kit, decay, tone, sampleRate) : 0.f;

        // Stereo mix with head-controlled panning
        float panAmount = rack::math::clamp(headX * 0.5f + pan, -1.f, 1.f);
        float panL = std::cos((panAmount + 1.f) * 0.25f * M_PI);
        float panR = std::sin((panAmount + 1.f) * 0.25f * M_PI);

        // Kick center, snare slightly right, hats spread, crash wide
        mixL = (kickOut * 0.5f + snareOut * 0.45f * panL + chOut * 0.7f * panL + ohOut * 0.3f * panL + crashOut * 0.6f * panL) * level;
        mixR = (kickOut * 0.5f + snareOut * 0.55f * panR + chOut * 0.3f * panR + ohOut * 0.7f * panR + crashOut * 0.4f * panR) * level;

        mixL = softClip(mixL);
        mixR = softClip(mixR);
    }
};


}  // namespace skull

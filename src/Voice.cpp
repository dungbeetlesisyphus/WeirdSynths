#include "plugin.hpp"
#include <cmath>
#include <cstring>
#include <algorithm>


// ═══════════════════════════════════════════════════════════
// YIN Pitch Detector
// ═══════════════════════════════════════════════════════════

struct YINDetector {
    static constexpr int MAX_BUFFER = 2048;

    float buffer[MAX_BUFFER] = {};
    int writePos = 0;
    int bufferSize = 1024; // default balanced

    float detectedFreq = 0.f;
    float confidence = 0.f;
    float sampleRate = 44100.f;

    int sampleCount = 0;
    int hopSize = 512;

    void setBufferSize(int size) {
        bufferSize = std::min(size, MAX_BUFFER);
        hopSize = bufferSize / 2;
    }

    void setSampleRate(float sr) { sampleRate = sr; }

    // Returns true when a new analysis is ready
    bool pushSample(float sample) {
        buffer[writePos] = sample;
        writePos = (writePos + 1) % bufferSize;
        sampleCount++;

        if (sampleCount >= hopSize) {
            sampleCount = 0;
            analyze();
            return true;
        }
        return false;
    }

    void analyze() {
        int halfW = bufferSize / 2;

        // Step 1: Difference function d(tau)
        float diff[MAX_BUFFER / 2];
        diff[0] = 0.f;

        for (int tau = 1; tau < halfW; tau++) {
            float sum = 0.f;
            for (int j = 0; j < halfW; j++) {
                int idx1 = (writePos - bufferSize + j + MAX_BUFFER * 2) % bufferSize;
                int idx2 = (writePos - bufferSize + j + tau + MAX_BUFFER * 2) % bufferSize;
                float delta = buffer[idx1] - buffer[idx2];
                sum += delta * delta;
            }
            diff[tau] = sum;
        }

        // Step 2: Cumulative mean normalized difference (CMND)
        float cmnd[MAX_BUFFER / 2];
        cmnd[0] = 1.f;
        float runningSum = 0.f;

        for (int tau = 1; tau < halfW; tau++) {
            runningSum += diff[tau];
            cmnd[tau] = (runningSum > 0.f) ? diff[tau] * tau / runningSum : 1.f;
        }

        // Step 3: Absolute threshold — find first dip below threshold
        float threshold = 0.15f;
        int bestTau = -1;

        // Min lag for highest detectable pitch (~C8 = 4186 Hz)
        int minLag = std::max(2, (int)(sampleRate / 5000.f));
        // Max lag for lowest detectable pitch (~C1 = 32.7 Hz)
        int maxLag = std::min(halfW - 1, (int)(sampleRate / 30.f));

        for (int tau = minLag; tau < maxLag; tau++) {
            if (cmnd[tau] < threshold) {
                // Find local minimum after crossing threshold
                while (tau + 1 < maxLag && cmnd[tau + 1] < cmnd[tau]) {
                    tau++;
                }
                bestTau = tau;
                break;
            }
        }

        // Fallback: if no dip below threshold, find global minimum
        if (bestTau < 0) {
            float minVal = 1.f;
            for (int tau = minLag; tau < maxLag; tau++) {
                if (cmnd[tau] < minVal) {
                    minVal = cmnd[tau];
                    bestTau = tau;
                }
            }
            // Only accept if reasonably confident
            if (minVal > 0.5f) {
                confidence = 0.f;
                return;
            }
        }

        if (bestTau < minLag) {
            confidence = 0.f;
            return;
        }

        // Step 4: Parabolic interpolation for sub-sample accuracy
        float tauEstimate = (float)bestTau;
        if (bestTau > 0 && bestTau < halfW - 1) {
            float s0 = cmnd[bestTau - 1];
            float s1 = cmnd[bestTau];
            float s2 = cmnd[bestTau + 1];
            float denom = 2.f * (2.f * s1 - s0 - s2);
            if (std::abs(denom) > 1e-6f) {
                tauEstimate = bestTau + (s0 - s2) / denom;
            }
        }

        detectedFreq = sampleRate / tauEstimate;
        confidence = 1.f - cmnd[bestTau];
        confidence = std::max(0.f, std::min(1.f, confidence));
    }

    float getFreq() const { return detectedFreq; }
    float getConfidence() const { return confidence; }
};


// ═══════════════════════════════════════════════════════════
// Envelope Follower (RMS-based)
// ═══════════════════════════════════════════════════════════

struct EnvelopeFollower {
    float envelope = 0.f;
    float attackCoeff = 0.f;
    float releaseCoeff = 0.f;

    void setCoeffs(float attackMs, float releaseMs, float sampleRate) {
        if (sampleRate <= 0.f) return;
        attackCoeff = std::exp(-1.f / (attackMs * 0.001f * sampleRate));
        releaseCoeff = std::exp(-1.f / (releaseMs * 0.001f * sampleRate));
    }

    float process(float sample) {
        float rectified = std::abs(sample);
        float coeff = (rectified > envelope) ? attackCoeff : releaseCoeff;
        envelope = coeff * envelope + (1.f - coeff) * rectified;
        return envelope;
    }
};


// ═══════════════════════════════════════════════════════════
// Onset Detector (energy-based with adaptive threshold)
// ═══════════════════════════════════════════════════════════

struct OnsetDetector {
    float prevEnergy = 0.f;
    float currentEnergy = 0.f;
    float energyAcc = 0.f;
    int sampleCount = 0;
    int windowSize = 512;
    float adaptiveThreshold = 0.f;

    bool triggered = false;
    int cooldownSamples = 0;
    int cooldownMax = 2205; // ~50ms at 44.1kHz

    void setCooldown(float ms, float sampleRate) {
        cooldownMax = (int)(ms * 0.001f * sampleRate);
    }

    // Returns true on onset
    bool pushSample(float sample) {
        energyAcc += sample * sample;
        sampleCount++;

        if (cooldownSamples > 0) cooldownSamples--;

        if (sampleCount >= windowSize) {
            sampleCount = 0;
            prevEnergy = currentEnergy;
            currentEnergy = std::sqrt(energyAcc / windowSize);
            energyAcc = 0.f;

            // Adaptive threshold (slow-moving average)
            adaptiveThreshold = adaptiveThreshold * 0.95f + currentEnergy * 0.05f;

            // Onset = significant increase above adaptive threshold
            if (prevEnergy > 0.001f && cooldownSamples <= 0) {
                float ratio = currentEnergy / prevEnergy;
                if (ratio > 1.5f && currentEnergy > adaptiveThreshold * 1.2f) {
                    cooldownSamples = cooldownMax;
                    return true;
                }
            }
        }
        return false;
    }
};


// ═══════════════════════════════════════════════════════════
// Spectral Brightness (zero-crossing rate as proxy)
// Uses ZCR as a lightweight spectral centroid approximation
// ═══════════════════════════════════════════════════════════

struct BrightnessTracker {
    float prevSample = 0.f;
    int crossings = 0;
    int sampleCount = 0;
    int windowSize = 1024;
    float brightness = 0.f;
    float smoothBrightness = 0.f;

    float process(float sample) {
        if ((sample > 0.f && prevSample <= 0.f) || (sample < 0.f && prevSample >= 0.f)) {
            crossings++;
        }
        prevSample = sample;
        sampleCount++;

        if (sampleCount >= windowSize) {
            // ZCR normalized to [0, 1] — higher = brighter
            float zcr = (float)crossings / (float)windowSize;
            // Scale: typical voice ZCR range is 0.01-0.3
            brightness = std::min(1.f, zcr * 5.f);
            crossings = 0;
            sampleCount = 0;
        }

        // Smooth
        smoothBrightness += (brightness - smoothBrightness) * 0.001f;
        return smoothBrightness;
    }
};


// ═══════════════════════════════════════════════════════════
// Harmonic Analyzer
// Detects harmonics from fundamental and outputs poly V/Oct
// ═══════════════════════════════════════════════════════════

struct HarmonicAnalyzer {
    static constexpr int MAX_HARMONICS = 8;
    float harmonicFreqs[MAX_HARMONICS] = {};
    float harmonicVolts[MAX_HARMONICS] = {};
    int numHarmonics = 0;

    void analyze(float fundamental, float confidence) {
        if (fundamental < 20.f || confidence < 0.3f) {
            numHarmonics = 0;
            return;
        }

        // Generate harmonic series from detected fundamental
        // In a full implementation we'd verify each harmonic exists in the spectrum
        // For now, output the theoretical harmonic series with decreasing confidence
        numHarmonics = MAX_HARMONICS;
        for (int i = 0; i < MAX_HARMONICS; i++) {
            float freq = fundamental * (i + 1);
            if (freq > 16000.f) {
                numHarmonics = i;
                break;
            }
            harmonicFreqs[i] = freq;
            // V/Oct: C4 (261.626 Hz) = 0V
            harmonicVolts[i] = std::log2(freq / 261.626f);
        }
    }
};


// ═══════════════════════════════════════════════════════════
// Note name lookup
// ═══════════════════════════════════════════════════════════

static const char* NOTE_NAMES[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

struct NoteInfo {
    char name[4] = {};
    int octave = 4;
    float cents = 0.f;

    void fromFreq(float freq) {
        if (freq < 10.f) {
            std::strcpy(name, "---");
            octave = 0;
            cents = 0.f;
            return;
        }
        float midi = 69.f + 12.f * std::log2(freq / 440.f);
        int midiNote = (int)std::round(midi);
        cents = (midi - midiNote) * 100.f;
        int noteIndex = ((midiNote % 12) + 12) % 12;
        octave = (midiNote / 12) - 1;
        std::strcpy(name, NOTE_NAMES[noteIndex]);
    }
};


// ═══════════════════════════════════════════════════════════
// VOICE Module
// ═══════════════════════════════════════════════════════════

struct Voice : Module {

    enum ParamId {
        SENS_PARAM,
        SMOOTH_PARAM,
        TONE_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        AUDIO_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        THRU_OUTPUT,
        VOCT_OUTPUT,
        GATE_OUTPUT,
        ENV_OUTPUT,
        ONSET_OUTPUT,
        BRIGHT_OUTPUT,
        HARM_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId {
        GATE_LIGHT,
        ONSET_LIGHT,
        LIGHTS_LEN
    };

    // DSP engines
    YINDetector yin;
    EnvelopeFollower envFollower;
    OnsetDetector onsetDetector;
    BrightnessTracker brightnessTracker;
    HarmonicAnalyzer harmonicAnalyzer;

    // State
    float currentPitchV = 0.f;
    float smoothedPitchV = 0.f;
    float currentEnvV = 0.f;
    float currentBrightV = 0.f;
    bool gateHigh = false;
    float gateHysteresis = 0.f; // smoothed gate

    dsp::PulseGenerator onsetPulse;
    float onsetLightVal = 0.f;

    // Display state (read by widget)
    NoteInfo displayNote;
    float displayConfidence = 0.f;
    float displayEnv = 0.f;
    bool displayActive = false;

    // Quality mode
    enum QualityMode { LIGHT, BALANCED, PREMIUM };
    QualityMode qualityMode = BALANCED;

    dsp::ClockDivider lightDivider;

    Voice() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(SENS_PARAM, 0.f, 1.f, 0.5f, "Sensitivity", " %", 0.f, 100.f);
        configParam(SMOOTH_PARAM, 0.f, 1.f, 0.4f, "Smoothing", " %", 0.f, 100.f);
        configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone / Brightness", " %", 0.f, 100.f);

        configInput(AUDIO_INPUT, "Audio");
        configOutput(THRU_OUTPUT, "Audio thru");
        configOutput(VOCT_OUTPUT, "V/Oct pitch");
        configOutput(GATE_OUTPUT, "Voiced gate");
        configOutput(ENV_OUTPUT, "Envelope");
        configOutput(ONSET_OUTPUT, "Onset trigger");
        configOutput(BRIGHT_OUTPUT, "Brightness");
        configOutput(HARM_OUTPUT, "Harmonics (poly V/Oct)");

        lightDivider.setDivision(256);
        applyQualityMode();
    }

    void applyQualityMode() {
        switch (qualityMode) {
            case LIGHT:
                yin.setBufferSize(512);
                break;
            case BALANCED:
                yin.setBufferSize(1024);
                break;
            case PREMIUM:
                yin.setBufferSize(2048);
                break;
        }
    }

    void process(const ProcessArgs& args) override {
        // Get audio input
        float audio = inputs[AUDIO_INPUT].getVoltage() / 5.f; // normalize to ±1

        // Pass through
        outputs[THRU_OUTPUT].setVoltage(inputs[AUDIO_INPUT].getVoltage());

        if (!inputs[AUDIO_INPUT].isConnected()) {
            outputs[VOCT_OUTPUT].setVoltage(0.f);
            outputs[GATE_OUTPUT].setVoltage(0.f);
            outputs[ENV_OUTPUT].setVoltage(0.f);
            outputs[ONSET_OUTPUT].setVoltage(0.f);
            outputs[BRIGHT_OUTPUT].setVoltage(0.f);
            outputs[HARM_OUTPUT].setVoltage(0.f);
            outputs[HARM_OUTPUT].setChannels(1);
            displayActive = false;
            return;
        }

        displayActive = true;

        float sens = params[SENS_PARAM].getValue();
        float smooth = params[SMOOTH_PARAM].getValue();
        float tone = params[TONE_PARAM].getValue();

        // Set sample rate on DSP engines
        yin.setSampleRate(args.sampleRate);
        onsetDetector.setCooldown(50.f, args.sampleRate);

        // Envelope follower — attack/release scaled by smooth knob
        float attackMs = 1.f + smooth * 49.f;   // 1-50ms
        float releaseMs = 10.f + smooth * 490.f; // 10-500ms
        envFollower.setCoeffs(attackMs, releaseMs, args.sampleRate);
        float envRaw = envFollower.process(audio);
        currentEnvV = std::min(10.f, envRaw * 20.f); // scale to 0-10V range

        // Sensitivity threshold for gate
        float gateThreshold = 0.01f + (1.f - sens) * 0.15f;

        // Gate with hysteresis
        if (!gateHigh && envRaw > gateThreshold * 1.2f) {
            gateHigh = true;
        } else if (gateHigh && envRaw < gateThreshold * 0.8f) {
            gateHigh = false;
        }

        // Smooth gate for display
        float gateTarget = gateHigh ? 1.f : 0.f;
        gateHysteresis += (gateTarget - gateHysteresis) * 0.01f;

        // Pitch detection
        bool newPitch = yin.pushSample(audio);

        if (newPitch && yin.getConfidence() > 0.3f && gateHigh) {
            float freq = yin.getFreq();
            if (freq > 20.f && freq < 5000.f) {
                // V/Oct: C4 (261.626 Hz) = 0V
                currentPitchV = std::log2(freq / 261.626f);

                // Update display
                displayNote.fromFreq(freq);
                displayConfidence = yin.getConfidence();

                // Harmonics
                harmonicAnalyzer.analyze(freq, yin.getConfidence());
            }
        }

        // Pitch smoothing — one-pole lowpass
        float smoothCoeff = 1.f - std::exp(-1.f / (std::max(0.001f, smooth * 0.05f) * args.sampleRate));
        if (gateHigh) {
            smoothedPitchV += (currentPitchV - smoothedPitchV) * smoothCoeff;
        }

        // Onset detection
        bool onset = onsetDetector.pushSample(audio);
        if (onset && sens > 0.1f) {
            onsetPulse.trigger(1e-3f); // 1ms pulse
            onsetLightVal = 1.f;
        }

        // Brightness
        brightnessTracker.windowSize = (int)(512 + tone * 1536); // 512-2048 window
        float bright = brightnessTracker.process(audio);
        currentBrightV = bright * 10.f;

        // ── Set outputs ──

        outputs[VOCT_OUTPUT].setVoltage(smoothedPitchV);
        outputs[GATE_OUTPUT].setVoltage(gateHigh ? 10.f : 0.f);
        outputs[ENV_OUTPUT].setVoltage(currentEnvV);
        outputs[BRIGHT_OUTPUT].setVoltage(currentBrightV);

        // Onset trigger
        float onsetV = onsetPulse.process(args.sampleTime) ? 10.f : 0.f;
        outputs[ONSET_OUTPUT].setVoltage(onsetV);

        // Harmonics — polyphonic V/Oct
        int numHarm = harmonicAnalyzer.numHarmonics;
        if (numHarm > 0 && gateHigh) {
            outputs[HARM_OUTPUT].setChannels(numHarm);
            for (int i = 0; i < numHarm; i++) {
                outputs[HARM_OUTPUT].setVoltage(harmonicAnalyzer.harmonicVolts[i], i);
            }
        } else {
            outputs[HARM_OUTPUT].setChannels(1);
            outputs[HARM_OUTPUT].setVoltage(0.f);
        }

        // Display envelope
        displayEnv = currentEnvV / 10.f;

        // Lights
        if (lightDivider.process()) {
            lights[GATE_LIGHT].setSmoothBrightness(gateHigh ? 1.f : 0.f, args.sampleTime * 256);
            onsetLightVal *= 0.9f;
            lights[ONSET_LIGHT].setSmoothBrightness(onsetLightVal, args.sampleTime * 256);
        }
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "qualityMode", json_integer((int)qualityMode));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* qJ = json_object_get(root, "qualityMode");
        if (qJ) {
            qualityMode = (QualityMode)json_integer_value(qJ);
            applyQualityMode();
        }
    }
};


// ═══════════════════════════════════════════════════════════
// CRT Pitch Display Widget
// ═══════════════════════════════════════════════════════════

struct PitchCRTDisplay : widget::TransparentWidget {
    Voice* module = nullptr;

    // Phosphor colors (matching MIRROR)
    static constexpr int P_R = 40;
    static constexpr int P_G = 255;
    static constexpr int P_B = 90;

    // Display bounds (relative to widget)
    static constexpr float DX = 2.f;
    static constexpr float DY = 2.f;
    static constexpr float DW = 108.f;
    static constexpr float DH = 64.f;

    int frameCount = 0;

    void draw(const DrawArgs& args) override {
        // CRT housing
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, DX - 1.f, DY - 1.f, DW + 2.f, DH + 2.f, 3.f);
        nvgFillColor(args.vg, nvgRGBA(15, 15, 12, 255));
        nvgFill(args.vg);

        // Screen
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, DX, DY, DW, DH, 2.f);
        nvgFillColor(args.vg, nvgRGBA(2, 6, 2, 255));
        nvgFill(args.vg);

        // Bezel
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, DX, DY, DW, DH, 2.f);
        nvgStrokeColor(args.vg, nvgRGBA(30, 50, 30, 100));
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStroke(args.vg);

        TransparentWidget::draw(args);
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) {
            TransparentWidget::drawLayer(args, layer);
            return;
        }

        frameCount++;

        nvgSave(args.vg);
        nvgScissor(args.vg, DX, DY, DW, DH);

        // Background glow
        {
            float cx = DX + DW / 2.f;
            float cy = DY + DH / 2.f;
            NVGpaint glow = nvgRadialGradient(args.vg, cx, cy, 5.f, 60.f,
                nvgRGBA(P_R / 8, P_G / 8, P_B / 8, 12),
                nvgRGBA(0, 0, 0, 0));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY, DW, DH);
            nvgFillPaint(args.vg, glow);
            nvgFill(args.vg);
        }

        if (module && module->displayActive && module->displayConfidence > 0.2f) {
            drawPitchInfo(args);
        } else {
            drawNoInput(args);
        }

        // Scanlines
        for (float y = DY; y < DY + DH; y += 2.5f) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, y, DW, 0.8f);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 15));
            nvgFill(args.vg);
        }

        // Moving scan beam
        float scanY = DY + std::fmod(frameCount * 0.8f, DH);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, DX, scanY - 0.5f, DW, 1.5f);
        nvgFillColor(args.vg, nvgRGBA(P_R / 3, P_G / 3, P_B / 3, 15));
        nvgFill(args.vg);

        // Vignette
        {
            NVGpaint vig = nvgLinearGradient(args.vg, DX, DY, DX, DY + 12.f,
                nvgRGBA(0, 0, 0, 60), nvgRGBA(0, 0, 0, 0));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY, DW, 12.f);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);

            vig = nvgLinearGradient(args.vg, DX, DY + DH - 12.f, DX, DY + DH,
                nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 60));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY + DH - 12.f, DW, 12.f);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);
        }

        // Screen reflection
        {
            NVGpaint refl = nvgLinearGradient(args.vg,
                DX + DW * 0.2f, DY + 2.f,
                DX + DW * 0.8f, DY + DH * 0.25f,
                nvgRGBA(255, 255, 255, 3),
                nvgRGBA(255, 255, 255, 0));
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, DX + 3.f, DY + 2.f, DW - 6.f, DH * 0.2f, 1.f);
            nvgFillPaint(args.vg, refl);
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
        TransparentWidget::drawLayer(args, layer);
    }

    void drawPitchInfo(const DrawArgs& args) {
        if (!module) return;

        float cx = DX + DW / 2.f;
        float conf = module->displayConfidence;
        float envLevel = module->displayEnv;

        // Phosphor intensity based on confidence
        int alpha = (int)(120 + conf * 135.f);

        // Note name — big
        nvgFontSize(args.vg, 28.f);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, alpha));
        nvgText(args.vg, cx - 8.f, DY + DH * 0.4f, module->displayNote.name, NULL);

        // Octave number
        char octStr[4];
        snprintf(octStr, sizeof(octStr), "%d", module->displayNote.octave);
        nvgFontSize(args.vg, 16.f);
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
        nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, alpha * 3 / 4));
        nvgText(args.vg, cx + 14.f, DY + DH * 0.45f, octStr, NULL);

        // Cents deviation
        float cents = module->displayNote.cents;
        char centStr[16];
        snprintf(centStr, sizeof(centStr), "%+.0f\xC2\xA2", cents); // ¢ symbol
        nvgFontSize(args.vg, 10.f);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        int centAlpha = (int)(40 + std::abs(cents) * 2.f);
        nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, std::min(200, centAlpha)));
        nvgText(args.vg, cx, DY + DH * 0.62f, centStr, NULL);

        // Tuning indicator bar — horizontal line showing cents deviation
        float barW = 60.f;
        float barX = cx - barW / 2.f;
        float barY = DY + DH * 0.78f;

        // Center line
        nvgBeginPath(args.vg);
        nvgRect(args.vg, barX, barY, barW, 1.f);
        nvgFillColor(args.vg, nvgRGBA(P_R / 3, P_G / 3, P_B / 3, 40));
        nvgFill(args.vg);

        // Center tick
        nvgBeginPath(args.vg);
        nvgRect(args.vg, cx - 0.5f, barY - 2.f, 1.f, 5.f);
        nvgFillColor(args.vg, nvgRGBA(P_R / 2, P_G / 2, P_B / 2, 60));
        nvgFill(args.vg);

        // Pitch indicator dot
        float dotX = cx + (cents / 50.f) * (barW / 2.f);
        dotX = std::max(barX + 2.f, std::min(barX + barW - 2.f, dotX));
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, dotX, barY + 0.5f, 2.5f);
        nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, alpha));
        nvgFill(args.vg);

        // Glow around dot
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, dotX, barY + 0.5f, 5.f);
        nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, 20));
        nvgFill(args.vg);

        // Envelope level bar (right edge, vertical)
        float meterH = DH - 10.f;
        float meterX = DX + DW - 8.f;
        float meterY = DY + 5.f;

        nvgBeginPath(args.vg);
        nvgRect(args.vg, meterX, meterY, 3.f, meterH);
        nvgFillColor(args.vg, nvgRGBA(P_R / 6, P_G / 6, P_B / 6, 30));
        nvgFill(args.vg);

        float fillH = envLevel * meterH;
        nvgBeginPath(args.vg);
        nvgRect(args.vg, meterX, meterY + meterH - fillH, 3.f, fillH);
        nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, (int)(60 + envLevel * 100.f)));
        nvgFill(args.vg);
    }

    void drawNoInput(const DrawArgs& args) {
        float cx = DX + DW / 2.f;
        float cy = DY + DH / 2.f;

        float flicker = 0.3f + 0.15f * std::sin(frameCount * 0.04f);
        int alpha = (int)(flicker * 80.f);

        nvgFontSize(args.vg, 10.f);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, alpha));
        nvgText(args.vg, cx, cy - 6.f, "NO INPUT", NULL);

        nvgFontSize(args.vg, 7.f);
        nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, alpha / 2));
        nvgText(args.vg, cx, cy + 8.f, "patch audio to IN", NULL);

        // Ghost waveform
        nvgBeginPath(args.vg);
        for (int i = 0; i < (int)DW - 10; i++) {
            float x = DX + 5.f + i;
            float y = cy + 16.f + std::sin(i * 0.15f + frameCount * 0.02f) * 3.f * flicker;
            if (i == 0) nvgMoveTo(args.vg, x, y);
            else nvgLineTo(args.vg, x, y);
        }
        nvgStrokeColor(args.vg, nvgRGBA(P_R, P_G, P_B, alpha / 3));
        nvgStrokeWidth(args.vg, 0.8f);
        nvgStroke(args.vg);
    }
};


// ═══════════════════════════════════════════════════════════
// Widget
// ═══════════════════════════════════════════════════════════

struct VoiceWidget : ModuleWidget {

    VoiceWidget(Voice* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Voice.svg")));

        // Screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // CRT Pitch display
        PitchCRTDisplay* crt = new PitchCRTDisplay;
        crt->box.pos = Vec(5, 24);
        crt->box.size = Vec(112, 68);
        crt->module = module;
        addChild(crt);

        // 8HP = 40.64mm = ~121.92px
        float colL = box.size.x * 0.27f;  // ~33px
        float colR = box.size.x * 0.73f;  // ~89px
        float colC = box.size.x * 0.5f;

        // Knobs — SENS and SMOOTH top row, TONE center
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(colL, 107), module, Voice::SENS_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(colR, 107), module, Voice::SMOOTH_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(colC, 140), module, Voice::TONE_PARAM));

        // I/O Jacks — paired left/right

        // Row 1: IN / THRU
        float row1 = 178;
        addInput(createInputCentered<PJ301MPort>(Vec(colL, row1), module, Voice::AUDIO_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(colR, row1), module, Voice::THRU_OUTPUT));

        // Row 2: V/OCT / GATE
        float row2 = 218;
        addOutput(createOutputCentered<PJ301MPort>(Vec(colL, row2), module, Voice::VOCT_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(colR, row2), module, Voice::GATE_OUTPUT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(colR + 14, row2 - 10), module, Voice::GATE_LIGHT));

        // Row 3: ENV / ONSET
        float row3 = 258;
        addOutput(createOutputCentered<PJ301MPort>(Vec(colL, row3), module, Voice::ENV_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(colR, row3), module, Voice::ONSET_OUTPUT));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(colR + 14, row3 - 10), module, Voice::ONSET_LIGHT));

        // Row 4: BRIGHT / HARM
        float row4 = 298;
        addOutput(createOutputCentered<PJ301MPort>(Vec(colL, row4), module, Voice::BRIGHT_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(colR, row4), module, Voice::HARM_OUTPUT));
    }

    void appendContextMenu(Menu* menu) override {
        Voice* module = dynamic_cast<Voice*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("VOICE Settings"));

        // Quality mode
        menu->addChild(createSubmenuItem("Quality Mode", "", [=](Menu* subMenu) {
            subMenu->addChild(createCheckMenuItem("Light (~11ms latency)",  "",
                [=]() { return module->qualityMode == Voice::LIGHT; },
                [=]() { module->qualityMode = Voice::LIGHT; module->applyQualityMode(); }));
            subMenu->addChild(createCheckMenuItem("Balanced (~23ms latency)", "",
                [=]() { return module->qualityMode == Voice::BALANCED; },
                [=]() { module->qualityMode = Voice::BALANCED; module->applyQualityMode(); }));
            subMenu->addChild(createCheckMenuItem("Premium (~46ms latency)", "",
                [=]() { return module->qualityMode == Voice::PREMIUM; },
                [=]() { module->qualityMode = Voice::PREMIUM; module->applyQualityMode(); }));
        }));
    }
};


Model* modelVoice = createModel<Voice, VoiceWidget>("Voice");

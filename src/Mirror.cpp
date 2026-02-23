#include "plugin.hpp"
#include "NerveUDP.hpp"
#include "NerveSmoothing.hpp"
#include <cstdlib>
#include <cmath>
#include <cstring>


// ═══════════════════════════════════════════════════════════
// MIRROR — Dot Matrix CRT Face Display for VCV Rack
// Optimized: batched rendering, control-rate smoothing,
// anti-feedback blanking
// ═══════════════════════════════════════════════════════════

struct Mirror : Module {

    enum ParamId {
        CAM_PARAM,
        FREEZE_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        BLANK_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        OUTPUTS_LEN
    };

    enum LightId {
        CAM_GREEN_LIGHT,
        CAM_RED_LIGHT,
        FREEZE_LIGHT,
        LIGHTS_LEN
    };

    nerve::FaceDataBuffer faceDataBuffer;
    nerve::UDPListener udpListener{&faceDataBuffer};
    nerve::TimeoutTracker timeout;

    uint64_t lastSeenVersion = 0;
    int udpPort = 9002;
    float faceTimeoutSec = 0.5f;

    dsp::ClockDivider threadCheckDivider;
    dsp::ClockDivider smoothDivider;

    struct SmoothedFace {
        float headX = 0.f, headY = 0.f, headZ = 0.f;
        float leftEye = 0.f, rightEye = 0.f;
        float gazeX = 0.f, gazeY = 0.f;
        float mouthW = 0.f, mouthH = 0.f;
        float jaw = 0.f, lips = 0.f;
        float browL = 0.f, browR = 0.f;
        float blinkL = 0.f, blinkR = 0.f;
        float expression = 0.f;
        float tongue = 0.f;
        float browInnerUp = 0.f;
        float browDownL = 0.f, browDownR = 0.f;
        bool valid = false;
    };

    SmoothedFace displayFace;
    nerve::SlewSmoother smoothers[20];

    // Anti-feedback state (read by widget on UI thread)
    std::atomic<bool> blanked{false};
    std::atomic<bool> frozen{false};

    // Display rate limiting
    int displayRateDivisor = 1;  // 1=60fps, 2=30fps, 4=15fps, 6=10fps

    // Display mode: 0=Face, 1=Monitor (parameter bars)
    int displayMode = 0;

    Mirror() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(CAM_PARAM, 0.f, 1.f, 1.f, "Camera Enable");
        configParam(FREEZE_PARAM, 0.f, 1.f, 0.f, "Freeze Display");
        configInput(BLANK_INPUT, "Display Blank");

        timeout.setTimeoutSeconds(faceTimeoutSec);
        threadCheckDivider.setDivision(1024);
        smoothDivider.setDivision(256);  // ~187 Hz at 48k
    }

    void onAdd() override {
        if (params[CAM_PARAM].getValue() > 0.5f) {
            udpListener.start(udpPort);
        }
    }

    void onRemove() override {
        udpListener.stop();
    }

    void process(const ProcessArgs& args) override {
        bool camEnabled = params[CAM_PARAM].getValue() > 0.5f;

        if (threadCheckDivider.process()) {
            if (camEnabled && !udpListener.isRunning()) {
                udpListener.start(udpPort);
            } else if (!camEnabled && udpListener.isRunning()) {
                udpListener.stop();
            }
        }

        // Check face data version (cheap atomic read)
        const nerve::FaceData& face = faceDataBuffer.read();
        uint64_t currentVersion = faceDataBuffer.getVersion();
        if (currentVersion != lastSeenVersion) {
            lastSeenVersion = currentVersion;
            timeout.reset();
        }
        timeout.tick(args.sampleTime);
        bool faceValid = face.valid && !timeout.isTimedOut();

        // Anti-feedback: BLANK gate
        blanked.store(inputs[BLANK_INPUT].getVoltage() > 1.f,
                      std::memory_order_relaxed);

        // Freeze toggle
        frozen.store(params[FREEZE_PARAM].getValue() > 0.5f,
                     std::memory_order_relaxed);

        // ── Smoothing at CONTROL RATE (~187 Hz) ──
        // This is purely visual data — no need for audio-rate smoothing
        if (smoothDivider.process() && !frozen.load(std::memory_order_relaxed)) {
            float smoothTime = 0.06f;
            float dt = args.sampleTime * 256.f;

            float targets[20] = {
                faceValid ? face.headX : 0.f,
                faceValid ? face.headY : 0.f,
                faceValid ? face.headZ : 0.f,
                faceValid ? face.leftEye : 0.f,
                faceValid ? face.rightEye : 0.f,
                faceValid ? face.gazeX : 0.f,
                faceValid ? face.gazeY : 0.f,
                faceValid ? face.mouthW : 0.f,
                faceValid ? face.mouthH : 0.f,
                faceValid ? face.jaw : 0.f,
                faceValid ? face.lips : 0.f,
                faceValid ? face.browL : 0.f,
                faceValid ? face.browR : 0.f,
                faceValid ? face.blinkL : 0.f,
                faceValid ? face.blinkR : 0.f,
                faceValid ? face.expression : 0.f,
                faceValid ? face.tongue : 0.f,
                faceValid ? face.browInnerUp : 0.f,
                faceValid ? face.browDownL : 0.f,
                faceValid ? face.browDownR : 0.f,
            };

            for (int i = 0; i < 20; i++) {
                targets[i] = smoothers[i].process(targets[i], smoothTime, dt);
            }

            displayFace.headX      = targets[0];
            displayFace.headY      = targets[1];
            displayFace.headZ      = targets[2];
            displayFace.leftEye    = targets[3];
            displayFace.rightEye   = targets[4];
            displayFace.gazeX      = targets[5];
            displayFace.gazeY      = targets[6];
            displayFace.mouthW     = targets[7];
            displayFace.mouthH     = targets[8];
            displayFace.jaw        = targets[9];
            displayFace.lips       = targets[10];
            displayFace.browL      = targets[11];
            displayFace.browR      = targets[12];
            displayFace.blinkL     = targets[13];
            displayFace.blinkR     = targets[14];
            displayFace.expression = targets[15];
            displayFace.tongue     = targets[16];
            displayFace.browInnerUp = targets[17];
            displayFace.browDownL  = targets[18];
            displayFace.browDownR  = targets[19];
            displayFace.valid      = faceValid;
        }

        // Lights — keep at audio rate for smooth visual (cheap)
        lights[CAM_GREEN_LIGHT].setSmoothBrightness(
            faceValid ? 1.f : 0.f, args.sampleTime);
        lights[CAM_RED_LIGHT].setSmoothBrightness(
            (!faceValid && camEnabled) ? 1.f : 0.f, args.sampleTime);
        lights[FREEZE_LIGHT].setSmoothBrightness(
            frozen.load(std::memory_order_relaxed) ? 1.f : 0.f, args.sampleTime);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "udpPort", json_integer(udpPort));
        json_object_set_new(root, "faceTimeout", json_real(faceTimeoutSec));
        json_object_set_new(root, "displayRate", json_integer(displayRateDivisor));
        json_object_set_new(root, "displayMode", json_integer(displayMode));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* portJ = json_object_get(root, "udpPort");
        if (portJ) udpPort = json_integer_value(portJ);

        json_t* timeoutJ = json_object_get(root, "faceTimeout");
        if (timeoutJ) {
            faceTimeoutSec = json_real_value(timeoutJ);
            timeout.setTimeoutSeconds(faceTimeoutSec);
        }

        json_t* rateJ = json_object_get(root, "displayRate");
        if (rateJ) displayRateDivisor = json_integer_value(rateJ);

        json_t* modeJ = json_object_get(root, "displayMode");
        if (modeJ) displayMode = json_integer_value(modeJ);
    }
};


// ═══════════════════════════════════════════════════════════
// DOT MATRIX CRT FACE DISPLAY — Optimized
// Batched NanoVG rendering, frame-rate limiting
// ═══════════════════════════════════════════════════════════

struct DotMatrixDisplay : widget::TransparentWidget {
    Mirror* module = nullptr;

    // Grid dimensions
    static constexpr int GRID_W = 32;
    static constexpr int GRID_H = 40;

    // Display area
    static constexpr float DX = 6.f;
    static constexpr float DY = 24.f;
    static constexpr float DW = 110.f;
    static constexpr float DH = 138.f;

    // Dot spacing & size
    static constexpr float DOT_SPACING = 3.4f;
    static constexpr float DOT_RADIUS  = 1.1f;

    // Phosphor color
    static constexpr int P_R = 40;
    static constexpr int P_G = 255;
    static constexpr int P_B = 90;

    // Brightness quantization bands for batched rendering
    static constexpr int NUM_BANDS = 8;

    int frameCount = 0;
    int skipCounter = 0;

    float grid[GRID_W * GRID_H] = {};
    float persist[GRID_W * GRID_H] = {};

    // ── Grid operations ──

    void clearGrid() {
        std::memset(grid, 0, sizeof(grid));
    }

    void setDot(int x, int y, float brightness) {
        if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return;
        int idx = y * GRID_W + x;
        grid[idx] = std::min(1.f, grid[idx] + brightness);
    }

    void plotDot(float fx, float fy, float brightness) {
        int ix = (int)fx;
        int iy = (int)fy;
        float fracX = fx - ix;
        float fracY = fy - iy;
        setDot(ix,     iy,     brightness * (1.f - fracX) * (1.f - fracY));
        setDot(ix + 1, iy,     brightness * fracX * (1.f - fracY));
        setDot(ix,     iy + 1, brightness * (1.f - fracX) * fracY);
        setDot(ix + 1, iy + 1, brightness * fracX * fracY);
    }

    void plotLine(float x0, float y0, float x1, float y1, float brightness) {
        float dx = x1 - x0;
        float dy = y1 - y0;
        float dist = std::sqrt(dx * dx + dy * dy);
        int steps = std::max(1, (int)(dist * 1.5f));
        for (int i = 0; i <= steps; i++) {
            float t = (float)i / (float)steps;
            plotDot(x0 + dx * t, y0 + dy * t, brightness);
        }
    }

    void plotEllipse(float cx, float cy, float rx, float ry, float brightness) {
        int segments = std::max(12, (int)(std::max(rx, ry) * 4.f));
        for (int i = 0; i < segments; i++) {
            float a0 = 2.f * M_PI * i / segments;
            float a1 = 2.f * M_PI * (i + 1) / segments;
            plotLine(cx + std::cos(a0) * rx, cy + std::sin(a0) * ry,
                     cx + std::cos(a1) * rx, cy + std::sin(a1) * ry, brightness);
        }
    }

    void fillEllipse(float cx, float cy, float rx, float ry, float brightness) {
        int y0 = std::max(0, (int)(cy - ry));
        int y1 = std::min(GRID_H - 1, (int)(cy + ry));
        for (int y = y0; y <= y1; y++) {
            float dy = ((float)y - cy) / ry;
            if (std::abs(dy) > 1.f) continue;
            float xSpan = rx * std::sqrt(1.f - dy * dy);
            int x0 = std::max(0, (int)(cx - xSpan));
            int x1 = std::min(GRID_W - 1, (int)(cx + xSpan));
            for (int x = x0; x <= x1; x++) {
                setDot(x, y, brightness);
            }
        }
    }

    void plotQuad(float x0, float y0, float cx, float cy, float x1, float y1, float brightness) {
        int steps = 16;
        float px = x0, py = y0;
        for (int i = 1; i <= steps; i++) {
            float t = (float)i / steps;
            float it = 1.f - t;
            float nx = it * it * x0 + 2.f * it * t * cx + t * t * x1;
            float ny = it * it * y0 + 2.f * it * t * cy + t * t * y1;
            plotLine(px, py, nx, ny, brightness);
            px = nx;
            py = ny;
        }
    }

    // ── Face rasterization (grid only — no NanoVG here) ──

    void rasterizeFace(const Mirror::SmoothedFace& f) {
        clearGrid();

        float gcx = GRID_W / 2.f + f.headX * 4.f;
        float gcy = GRID_H / 2.f - f.headY * 3.f;
        float bright = 0.6f + f.expression * 0.4f;

        // Face outline
        float faceRX = 10.f;
        float faceRY = 13.f + f.jaw * 2.f;
        plotEllipse(gcx, gcy, faceRX, faceRY, bright * 0.5f);

        // Jawline
        float jawDrop = f.jaw * 2.5f;
        plotQuad(gcx - 8.f, gcy + 1.f,
                 gcx, gcy + 12.f + jawDrop,
                 gcx + 8.f, gcy + 1.f, bright * 0.35f);

        // Left eyebrow
        float browLH = gcy - 7.f - f.browL * 1.5f + f.browDownL * 1.f;
        float browInner = f.browInnerUp * 1.2f;
        plotLine(gcx - 7.f, browLH + 0.5f,
                 gcx - 1.5f, browLH - 0.5f - browInner, bright * 0.9f);

        // Right eyebrow
        float browRH = gcy - 7.f - f.browR * 1.5f + f.browDownR * 1.f;
        plotLine(gcx + 1.5f, browRH - 0.5f - browInner,
                 gcx + 7.f, browRH + 0.5f, bright * 0.9f);

        // Left eye
        float leOpen = f.leftEye * (1.f - f.blinkL);
        float leyH = 0.3f + leOpen * 1.8f;
        float lex = gcx - 4.f;
        float ley = gcy - 3.5f;
        plotEllipse(lex, ley, 2.5f, leyH, bright * 0.8f);

        if (leOpen > 0.25f) {
            float px = lex + f.gazeX * 1.f;
            float py = ley - f.gazeY * 0.6f;
            fillEllipse(px, py, 0.7f + leOpen * 0.2f, 0.7f + leOpen * 0.2f, bright);
        }

        // Right eye
        float reOpen = f.rightEye * (1.f - f.blinkR);
        float reyH = 0.3f + reOpen * 1.8f;
        float rex = gcx + 4.f;
        float rey = gcy - 3.5f;
        plotEllipse(rex, rey, 2.5f, reyH, bright * 0.8f);

        if (reOpen > 0.25f) {
            float px = rex + f.gazeX * 1.f;
            float py = rey - f.gazeY * 0.6f;
            fillEllipse(px, py, 0.7f + reOpen * 0.2f, 0.7f + reOpen * 0.2f, bright);
        }

        // Nose
        plotLine(gcx, gcy - 1.f, gcx - 1.f, gcy + 2.f, bright * 0.25f);
        plotLine(gcx - 1.f, gcy + 2.f, gcx + 1.f, gcy + 2.f, bright * 0.25f);

        // Mouth
        float mw = 2.5f + f.mouthW * 3.f;
        float mh = 0.5f + f.mouthH * 4.f;
        float my = gcy + 5.5f + f.jaw * 2.f;
        float lipsPurse = f.lips * 1.5f;
        mw = std::max(1.5f, mw - lipsPurse);

        plotQuad(gcx - mw, my, gcx, my - mh * 0.25f, gcx + mw, my, bright * 0.7f);
        plotQuad(gcx - mw, my, gcx, my + mh, gcx + mw, my, bright * 0.7f);

        if (mh > 1.5f) {
            fillEllipse(gcx, my + mh * 0.3f, mw * 0.7f, mh * 0.35f, bright * 0.15f);
        }

        // Tongue
        if (f.tongue > 0.05f) {
            float ty = my + mh * 0.5f;
            float tlen = f.tongue * 3.f;
            float tw = 1.2f + f.tongue * 0.8f;
            fillEllipse(gcx, ty + tlen * 0.4f, tw, tlen, bright * f.tongue * 0.6f);
        }
    }

    void rasterizeNoSignal() {
        clearGrid();

        static const uint8_t font[][5] = {
            {0b101, 0b111, 0b111, 0b111, 0b101}, // N
            {0b111, 0b101, 0b101, 0b101, 0b111}, // O
            {0b000, 0b000, 0b000, 0b000, 0b000}, // (space)
            {0b111, 0b100, 0b111, 0b001, 0b111}, // S
            {0b111, 0b010, 0b010, 0b010, 0b111}, // I
            {0b111, 0b100, 0b101, 0b101, 0b111}, // G
            {0b101, 0b111, 0b111, 0b111, 0b101}, // N
            {0b010, 0b101, 0b111, 0b101, 0b101}, // A
            {0b100, 0b100, 0b100, 0b100, 0b111}, // L
        };

        int numChars = 9;
        int totalW = numChars * 4 - 1;
        int startX = (GRID_W - totalW) / 2;
        int startY = GRID_H / 2 - 2;

        float flicker = 0.15f + 0.1f * std::sin(frameCount * 0.05f);

        for (int c = 0; c < numChars; c++) {
            for (int row = 0; row < 5; row++) {
                for (int col = 0; col < 3; col++) {
                    if (font[c][row] & (1 << (2 - col))) {
                        setDot(startX + c * 4 + col, startY + row, flicker);
                    }
                }
            }
        }

        // Ghost face
        float gcx = GRID_W / 2.f;
        float gcy = GRID_H / 2.f;
        plotEllipse(gcx, gcy, 10.f, 13.f, 0.04f);
        plotEllipse(gcx - 4.f, gcy - 3.5f, 2.5f, 1.5f, 0.03f);
        plotEllipse(gcx + 4.f, gcy - 3.5f, 2.5f, 1.5f, 0.03f);
    }

    // ── Monitor mode: parameter bars rasterized into grid ──

    void rasterizeMonitor(const Mirror::SmoothedFace& f) {
        clearGrid();

        // 3x5 pixel micro-font for parameter labels
        // Each char = 5 rows of 3 bits
        static const uint8_t microFont[26][5] = {
            {0b010,0b101,0b111,0b101,0b101}, // A
            {0b110,0b101,0b110,0b101,0b110}, // B
            {0b011,0b100,0b100,0b100,0b011}, // C
            {0b110,0b101,0b101,0b101,0b110}, // D
            {0b111,0b100,0b110,0b100,0b111}, // E
            {0b111,0b100,0b110,0b100,0b100}, // F
            {0b111,0b100,0b101,0b101,0b111}, // G
            {0b101,0b101,0b111,0b101,0b101}, // H
            {0b111,0b010,0b010,0b010,0b111}, // I
            {0b001,0b001,0b001,0b101,0b010}, // J
            {0b101,0b110,0b100,0b110,0b101}, // K
            {0b100,0b100,0b100,0b100,0b111}, // L
            {0b101,0b111,0b111,0b101,0b101}, // M
            {0b101,0b111,0b111,0b111,0b101}, // N
            {0b111,0b101,0b101,0b101,0b111}, // O
            {0b110,0b101,0b110,0b100,0b100}, // P
            {0b010,0b101,0b101,0b110,0b011}, // Q
            {0b110,0b101,0b110,0b101,0b101}, // R
            {0b111,0b100,0b111,0b001,0b111}, // S
            {0b111,0b010,0b010,0b010,0b010}, // T
            {0b101,0b101,0b101,0b101,0b111}, // U
            {0b101,0b101,0b101,0b101,0b010}, // V
            {0b101,0b101,0b111,0b111,0b101}, // W
            {0b101,0b101,0b010,0b101,0b101}, // X
            {0b101,0b101,0b010,0b010,0b010}, // Y
            {0b111,0b001,0b010,0b100,0b111}, // Z
        };

        auto drawChar = [&](int x, int y, char c, float brightness) {
            int idx = -1;
            if (c >= 'A' && c <= 'Z') idx = c - 'A';
            else if (c >= 'a' && c <= 'z') idx = c - 'a';
            if (idx < 0) return;
            for (int row = 0; row < 5; row++) {
                for (int col = 0; col < 3; col++) {
                    if (microFont[idx][row] & (1 << (2 - col))) {
                        setDot(x + col, y + row, brightness);
                    }
                }
            }
        };

        auto drawLabel = [&](int x, int y, const char* str, float brightness) {
            for (int i = 0; str[i]; i++) {
                if (str[i] == ' ') { x += 2; continue; }
                drawChar(x, y, str[i], brightness);
                x += 4;
            }
        };

        auto drawBar = [&](int x, int y, int maxW, float value, float brightness) {
            int filled = (int)(value * maxW + 0.5f);
            for (int i = 0; i < maxW; i++) {
                setDot(x + i, y, i < filled ? brightness : brightness * 0.08f);
                setDot(x + i, y + 1, i < filled ? brightness * 0.7f : brightness * 0.05f);
            }
        };

        // Layout: label (8 cols) + bar (22 cols) = 30, left margin 1
        struct ParamRow {
            const char* label;
            float value;
            bool bipolar;  // -1 to 1 range
        };

        ParamRow rows[] = {
            {"EL", f.leftEye, false},
            {"ER", f.rightEye, false},
            {"GX", (f.gazeX + 1.f) * 0.5f, false},
            {"GY", (f.gazeY + 1.f) * 0.5f, false},
            {"MW", f.mouthW, false},
            {"MH", f.mouthH, false},
            {"JW", f.jaw, false},
            {"LP", f.lips, false},
            {"BL", f.browL, false},
            {"BR", f.browR, false},
            {"TG", f.tongue, false},
            {"EX", f.expression, false},
            {"HX", (f.headX + 1.f) * 0.5f, false},
            {"HY", (f.headY + 1.f) * 0.5f, false},
        };

        int numRows = 14;
        float labelBright = 0.5f;
        float barBright = 0.8f;

        for (int i = 0; i < numRows && i * 3 + 1 < GRID_H; i++) {
            int y = 1 + i * 3;
            drawLabel(1, y - 1, rows[i].label, labelBright);
            drawBar(9, y - 1, 22, rack::math::clamp(rows[i].value, 0.f, 1.f), barBright);
        }
    }

    // ═══ DRAWING — OPTIMIZED ═══

    void draw(const DrawArgs& args) override {
        // CRT housing
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, DX - 2.f, DY - 2.f, DW + 4.f, DH + 4.f, 4.f);
        nvgFillColor(args.vg, nvgRGBA(15, 15, 12, 255));
        nvgFill(args.vg);

        // Screen bezel
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, DX, DY, DW, DH, 3.f);
        nvgFillColor(args.vg, nvgRGBA(2, 6, 2, 255));
        nvgFill(args.vg);

        // Inner bezel highlight
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, DX, DY, DW, DH, 3.f);
        nvgStrokeColor(args.vg, nvgRGBA(30, 50, 30, 100));
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);

        TransparentWidget::draw(args);
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) {
            TransparentWidget::drawLayer(args, layer);
            return;
        }

        frameCount++;

        // ── Display rate limiter ──
        int rateDivisor = module ? module->displayRateDivisor : 1;
        bool shouldRasterize = true;
        if (rateDivisor > 1) {
            skipCounter++;
            if (skipCounter < rateDivisor) {
                shouldRasterize = false;
            } else {
                skipCounter = 0;
            }
        }

        // ── Anti-feedback: blank check ──
        bool isBlanked = module && module->blanked.load(std::memory_order_relaxed);
        bool isFrozen  = module && module->frozen.load(std::memory_order_relaxed);

        nvgSave(args.vg);
        nvgScissor(args.vg, DX, DY, DW, DH);

        if (isBlanked) {
            // Display is blanked — just draw dark screen + dim "BLANK" text
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY, DW, DH);
            nvgFillColor(args.vg, nvgRGBA(2, 6, 2, 255));
            nvgFill(args.vg);

            float pulse = 0.3f + 0.15f * std::sin(frameCount * 0.03f);
            nvgFontSize(args.vg, 10.f);
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(args.vg, nvgRGBA(
                (int)(P_R * pulse), (int)(P_G * pulse), (int)(P_B * pulse),
                (int)(pulse * 120.f)));
            nvgText(args.vg, DX + DW / 2.f, DY + DH / 2.f, "BLANK", NULL);

            nvgRestore(args.vg);
            TransparentWidget::drawLayer(args, layer);
            return;
        }

        // ── Rasterize face to grid ──
        int dMode = module ? module->displayMode : 0;
        if (shouldRasterize && !isFrozen) {
            if (module && module->displayFace.valid) {
                if (dMode == 1) {
                    rasterizeMonitor(module->displayFace);
                } else {
                    rasterizeFace(module->displayFace);
                }
            } else {
                rasterizeNoSignal();
            }

            // Phosphor persistence decay
            for (int i = 0; i < GRID_W * GRID_H; i++) {
                persist[i] = std::max(grid[i], persist[i] * 0.88f);
            }
        }

        // ── CRT background glow ──
        {
            float cx = DX + DW / 2.f;
            float cy = DY + DH / 2.f;
            NVGpaint glow = nvgRadialGradient(args.vg, cx, cy,
                10.f, 80.f,
                nvgRGBA(P_R / 8, P_G / 8, P_B / 8, 15),
                nvgRGBA(0, 0, 0, 0));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY, DW, DH);
            nvgFillPaint(args.vg, glow);
            nvgFill(args.vg);
        }

        // ═══════════════════════════════════════════════════
        // BATCHED DOT RENDERING
        // Quantize brightness to NUM_BANDS levels, batch all
        // dots in each band into a single NanoVG path.
        // ~16 draw calls instead of ~2560.
        // ═══════════════════════════════════════════════════

        float gridOffX = DX + (DW - GRID_W * DOT_SPACING) / 2.f;
        float gridOffY = DY + (DH - GRID_H * DOT_SPACING) / 2.f;

        for (int band = NUM_BANDS - 1; band >= 0; band--) {
            float lo = (float)band / NUM_BANDS;
            float hi = (float)(band + 1) / NUM_BANDS;
            float midVal = (lo + hi) * 0.5f;

            if (midVal < 0.02f) continue;

            // ── Glow pass (larger, dimmer halos) ──
            if (midVal > 0.15f) {
                bool hasGlow = false;
                nvgBeginPath(args.vg);
                for (int gy = 0; gy < GRID_H; gy++) {
                    for (int gx = 0; gx < GRID_W; gx++) {
                        float val = persist[gy * GRID_W + gx];
                        if (val >= lo && val < hi) {
                            float px = gridOffX + gx * DOT_SPACING;
                            float py = gridOffY + gy * DOT_SPACING;
                            nvgCircle(args.vg, px, py, DOT_RADIUS * 2.f);
                            hasGlow = true;
                        }
                    }
                }
                if (hasGlow) {
                    nvgFillColor(args.vg, nvgRGBA(
                        (int)(P_R * midVal * 0.3f),
                        (int)(P_G * midVal * 0.3f),
                        (int)(P_B * midVal * 0.3f),
                        (int)(midVal * 35.f)));
                    nvgFill(args.vg);
                }
            }

            // ── Core dot pass ──
            bool hasDots = false;
            nvgBeginPath(args.vg);
            for (int gy = 0; gy < GRID_H; gy++) {
                for (int gx = 0; gx < GRID_W; gx++) {
                    float val = persist[gy * GRID_W + gx];
                    if (val >= lo && val < hi) {
                        float px = gridOffX + gx * DOT_SPACING;
                        float py = gridOffY + gy * DOT_SPACING;
                        nvgCircle(args.vg, px, py, DOT_RADIUS);
                        hasDots = true;
                    }
                }
            }
            if (hasDots) {
                nvgFillColor(args.vg, nvgRGBA(
                    (int)(P_R * midVal),
                    (int)(P_G * midVal),
                    (int)(P_B * midVal),
                    (int)(40 + midVal * 215.f)));
                nvgFill(args.vg);
            }
        }

        // Catch max-brightness dots (val == 1.0 falls outside bands)
        {
            bool hasMax = false;
            nvgBeginPath(args.vg);
            for (int gy = 0; gy < GRID_H; gy++) {
                for (int gx = 0; gx < GRID_W; gx++) {
                    float val = persist[gy * GRID_W + gx];
                    if (val >= 1.f) {
                        float px = gridOffX + gx * DOT_SPACING;
                        float py = gridOffY + gy * DOT_SPACING;
                        nvgCircle(args.vg, px, py, DOT_RADIUS * 2.f);
                        hasMax = true;
                    }
                }
            }
            if (hasMax) {
                nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, 30));
                nvgFill(args.vg);
            }

            hasMax = false;
            nvgBeginPath(args.vg);
            for (int gy = 0; gy < GRID_H; gy++) {
                for (int gx = 0; gx < GRID_W; gx++) {
                    float val = persist[gy * GRID_W + gx];
                    if (val >= 1.f) {
                        float px = gridOffX + gx * DOT_SPACING;
                        float py = gridOffY + gy * DOT_SPACING;
                        nvgCircle(args.vg, px, py, DOT_RADIUS);
                        hasMax = true;
                    }
                }
            }
            if (hasMax) {
                nvgFillColor(args.vg, nvgRGBA(P_R, P_G, P_B, 255));
                nvgFill(args.vg);
            }
        }

        // ── Scanlines — single batched path ──
        nvgBeginPath(args.vg);
        for (float y = DY; y < DY + DH; y += 3.f) {
            nvgRect(args.vg, DX, y, DW, 1.f);
        }
        nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 16));
        nvgFill(args.vg);

        // Moving beam
        float scanY = DY + std::fmod(frameCount * 1.2f, DH);
        nvgBeginPath(args.vg);
        nvgRect(args.vg, DX, scanY - 1.f, DW, 2.f);
        nvgFillColor(args.vg, nvgRGBA(P_R / 3, P_G / 3, P_B / 3, 18));
        nvgFill(args.vg);

        // ── CRT vignette — two gradient passes instead of four ──
        {
            // Vertical vignette (top + bottom combined)
            NVGpaint vig = nvgLinearGradient(args.vg, DX, DY, DX, DY + 20.f,
                nvgRGBA(0, 0, 0, 80), nvgRGBA(0, 0, 0, 0));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY, DW, 20.f);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);

            vig = nvgLinearGradient(args.vg, DX, DY + DH - 20.f, DX, DY + DH,
                nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 80));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY + DH - 20.f, DW, 20.f);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);

            // Horizontal vignette (left + right combined)
            vig = nvgLinearGradient(args.vg, DX, DY, DX + 15.f, DY,
                nvgRGBA(0, 0, 0, 60), nvgRGBA(0, 0, 0, 0));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY, 15.f, DH);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);

            vig = nvgLinearGradient(args.vg, DX + DW - 15.f, DY, DX + DW, DY,
                nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 60));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX + DW - 15.f, DY, 15.f, DH);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);
        }

        // ── FPS counter ──
        if (module) {
            float fps = module->udpListener.currentFPS.load();
            if (fps > 0.f) {
                char fpsStr[16];
                snprintf(fpsStr, sizeof(fpsStr), "%.0f", fps);
                nvgFontSize(args.vg, 8.f);
                nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
                nvgFillColor(args.vg, nvgRGBA(P_R / 2, P_G / 2, P_B / 2, 40));
                nvgText(args.vg, DX + DW - 4.f, DY + DH - 3.f, fpsStr, NULL);
            }
        }

        // ── Frozen indicator ──
        if (module && module->frozen.load(std::memory_order_relaxed)) {
            float pulse = 0.5f + 0.3f * std::sin(frameCount * 0.04f);
            nvgFontSize(args.vg, 8.f);
            nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
            nvgFillColor(args.vg, nvgRGBA(
                (int)(P_R * pulse), (int)(P_G * pulse), (int)(P_B * pulse),
                (int)(pulse * 80.f)));
            nvgText(args.vg, DX + 4.f, DY + DH - 3.f, "FRZ", NULL);
        }

        // ── Screen reflection ──
        {
            NVGpaint refl = nvgLinearGradient(args.vg,
                DX + DW * 0.2f, DY + 5.f,
                DX + DW * 0.8f, DY + DH * 0.3f,
                nvgRGBA(255, 255, 255, 4),
                nvgRGBA(255, 255, 255, 0));
            nvgBeginPath(args.vg);
            nvgRoundedRect(args.vg, DX + 5.f, DY + 3.f, DW - 10.f, DH * 0.25f, 2.f);
            nvgFillPaint(args.vg, refl);
            nvgFill(args.vg);
        }

        nvgRestore(args.vg);
        TransparentWidget::drawLayer(args, layer);
    }
};


// ═══════════════════════════════════════════════════════════
// Widget
// ═══════════════════════════════════════════════════════════

struct MirrorWidget : ModuleWidget {

    MirrorWidget(Mirror* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Mirror.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // CRT face display
        DotMatrixDisplay* crt = new DotMatrixDisplay;
        crt->box.pos = Vec(0, 0);
        crt->box.size = Vec(box.size.x, box.size.y);
        crt->module = module;
        addChild(crt);

        // Bottom controls — BLANK input, FREEZE, CAM
        float y = 355.f;
        float cx = box.size.x / 2.f;

        // BLANK input (left)
        addInput(createInputCentered<PJ301MPort>(Vec(cx - 28.f, y), module, Mirror::BLANK_INPUT));

        // FREEZE toggle (center)
        addParam(createParamCentered<VCVButton>(Vec(cx, y), module, Mirror::FREEZE_PARAM));
        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(cx, y - 12.f), module, Mirror::FREEZE_LIGHT));

        // CAM toggle (right)
        addParam(createParamCentered<VCVButton>(Vec(cx + 28.f, y), module, Mirror::CAM_PARAM));
        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            Vec(cx + 28.f, y - 12.f), module, Mirror::CAM_GREEN_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        Mirror* module = dynamic_cast<Mirror*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("MIRROR Settings"));

        // UDP Port
        struct PortField : ui::TextField {
            Mirror* module;
            void onSelectKey(const SelectKeyEvent& e) override {
                if (e.key == GLFW_KEY_ENTER && e.action == GLFW_PRESS) {
                    int port = std::atoi(getText().c_str());
                    if (port >= 1024 && port <= 65535) {
                        module->udpPort = port;
                        if (module->udpListener.isRunning()) {
                            module->udpListener.stop();
                            module->udpListener.start(port);
                        }
                    }
                    e.consume(this);
                }
                ui::TextField::onSelectKey(e);
            }
        };

        PortField* portField = new PortField;
        portField->box.size.x = 80;
        portField->module = module;
        portField->setText(std::to_string(module->udpPort));
        portField->placeholder = "9002";

        menu->addChild(createMenuLabel("UDP Port"));
        menu->addChild(portField);

        // Face timeout
        menu->addChild(createSubmenuItem("Face Timeout", "", [=](Menu* subMenu) {
            const float timeouts[] = {0.25f, 0.5f, 1.f, 2.f};
            const std::string labels[] = {"250ms", "500ms (default)", "1 second", "2 seconds"};
            for (int i = 0; i < 4; i++) {
                float t = timeouts[i];
                subMenu->addChild(createCheckMenuItem(labels[i], "",
                    [=]() { return module->faceTimeoutSec == t; },
                    [=]() {
                        module->faceTimeoutSec = t;
                        module->timeout.setTimeoutSeconds(t);
                    }
                ));
            }
        }));

        // Display mode
        menu->addChild(createSubmenuItem("Display Mode", "", [=](Menu* subMenu) {
            subMenu->addChild(createCheckMenuItem("Face", "",
                [=]() { return module->displayMode == 0; },
                [=]() { module->displayMode = 0; }
            ));
            subMenu->addChild(createCheckMenuItem("Monitor (parameter bars)", "",
                [=]() { return module->displayMode == 1; },
                [=]() { module->displayMode = 1; }
            ));
        }));

        // Display rate
        menu->addChild(createSubmenuItem("Display Rate", "", [=](Menu* subMenu) {
            const int divisors[] = {1, 2, 4, 6};
            const std::string labels[] = {"60 fps (default)", "30 fps", "15 fps", "10 fps"};
            for (int i = 0; i < 4; i++) {
                int d = divisors[i];
                subMenu->addChild(createCheckMenuItem(labels[i], "",
                    [=]() { return module->displayRateDivisor == d; },
                    [=]() { module->displayRateDivisor = d; }
                ));
            }
        }));

        // Status
        menu->addChild(new MenuSeparator);
        float fps = module->udpListener.currentFPS.load();
        std::string status;
        if (!module->udpListener.isRunning()) {
            status = "Camera disabled";
        } else if (fps > 0.f) {
            status = "Connected (" + std::to_string((int)fps) + " fps)";
        } else {
            status = "No data";
        }
        menu->addChild(createMenuLabel(status));
    }
};


Model* modelMirror = createModel<Mirror, MirrorWidget>("Mirror");

#include "plugin.hpp"
#include "NerveUDP.hpp"
#include "NerveSmoothing.hpp"
#include <cstdlib>
#include <cmath>


// ═══════════════════════════════════════════════════════════
// MIRROR — Dot Matrix CRT Face Display for VCV Rack
// Green phosphor, scanlines, pixel grid
// ═══════════════════════════════════════════════════════════

struct Mirror : Module {

    enum ParamId {
        CAM_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        INPUTS_LEN
    };

    enum OutputId {
        OUTPUTS_LEN
    };

    enum LightId {
        CAM_GREEN_LIGHT,
        CAM_RED_LIGHT,
        LIGHTS_LEN
    };

    nerve::FaceDataBuffer faceDataBuffer;
    nerve::UDPListener udpListener{&faceDataBuffer};
    nerve::TimeoutTracker timeout;

    uint64_t lastSeenVersion = 0;
    int udpPort = 9002;
    float faceTimeoutSec = 0.5f;

    dsp::ClockDivider threadCheckDivider;

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
    nerve::SlewSmoother smoothers[21];

    Mirror() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(CAM_PARAM, 0.f, 1.f, 1.f, "Camera Enable");

        timeout.setTimeoutSeconds(faceTimeoutSec);
        threadCheckDivider.setDivision(1024);
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

        const nerve::FaceData& face = faceDataBuffer.read();

        uint64_t currentVersion = faceDataBuffer.getVersion();
        if (currentVersion != lastSeenVersion) {
            lastSeenVersion = currentVersion;
            timeout.reset();
        }
        timeout.tick(args.sampleTime);
        bool faceValid = face.valid && !timeout.isTimedOut();

        float smoothTime = 0.06f;

        float targets[21] = {
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
            0.f
        };

        for (int i = 0; i < 20; i++) {
            targets[i] = smoothers[i].process(targets[i], smoothTime, args.sampleTime);
        }

        displayFace.headX = targets[0];
        displayFace.headY = targets[1];
        displayFace.headZ = targets[2];
        displayFace.leftEye = targets[3];
        displayFace.rightEye = targets[4];
        displayFace.gazeX = targets[5];
        displayFace.gazeY = targets[6];
        displayFace.mouthW = targets[7];
        displayFace.mouthH = targets[8];
        displayFace.jaw = targets[9];
        displayFace.lips = targets[10];
        displayFace.browL = targets[11];
        displayFace.browR = targets[12];
        displayFace.blinkL = targets[13];
        displayFace.blinkR = targets[14];
        displayFace.expression = targets[15];
        displayFace.tongue = targets[16];
        displayFace.browInnerUp = targets[17];
        displayFace.browDownL = targets[18];
        displayFace.browDownR = targets[19];
        displayFace.valid = faceValid;

        lights[CAM_GREEN_LIGHT].setSmoothBrightness(
            faceValid ? 1.f : 0.f, args.sampleTime);
        lights[CAM_RED_LIGHT].setSmoothBrightness(
            (!faceValid && camEnabled) ? 1.f : 0.f, args.sampleTime);
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "udpPort", json_integer(udpPort));
        json_object_set_new(root, "faceTimeout", json_real(faceTimeoutSec));
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
    }
};


// ═══════════════════════════════════════════════════════════
// DOT MATRIX CRT FACE DISPLAY
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

    // Dot spacing
    static constexpr float DOT_SPACING = 3.4f;
    static constexpr float DOT_RADIUS = 1.1f;

    // Phosphor colors
    static constexpr int P_R = 40;
    static constexpr int P_G = 255;
    static constexpr int P_B = 90;

    // Frame counter for scanline animation
    int frameCount = 0;

    // The grid buffer — brightness 0.0 to 1.0 per pixel
    float grid[GRID_W * GRID_H] = {};

    // Persistence / phosphor decay buffer
    float persist[GRID_W * GRID_H] = {};

    void clearGrid() {
        for (int i = 0; i < GRID_W * GRID_H; i++) {
            grid[i] = 0.f;
        }
    }

    // Set a dot with additive blending
    void setDot(int x, int y, float brightness) {
        if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return;
        grid[y * GRID_W + x] = std::min(1.f, grid[y * GRID_W + x] + brightness);
    }

    // Draw a dot at floating-point position with anti-aliasing to nearby pixels
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

    // Draw a line between two points using Bresenham-like approach
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

    // Draw an ellipse outline
    void plotEllipse(float cx, float cy, float rx, float ry, float brightness) {
        int segments = std::max(12, (int)(std::max(rx, ry) * 4.f));
        for (int i = 0; i < segments; i++) {
            float a0 = 2.f * M_PI * i / segments;
            float a1 = 2.f * M_PI * (i + 1) / segments;
            float x0 = cx + std::cos(a0) * rx;
            float y0 = cy + std::sin(a0) * ry;
            float x1 = cx + std::cos(a1) * rx;
            float y1 = cy + std::sin(a1) * ry;
            plotLine(x0, y0, x1, y1, brightness);
        }
    }

    // Fill an ellipse
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

    // Draw a quadratic curve
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

    // ── Rasterize the face into the grid ──
    void rasterizeFace(const Mirror::SmoothedFace& f) {
        clearGrid();

        // Grid center and head offset
        float gcx = GRID_W / 2.f + f.headX * 4.f;
        float gcy = GRID_H / 2.f - f.headY * 3.f;

        float bright = 0.6f + f.expression * 0.4f;

        // ── Face outline ──
        float faceRX = 10.f;
        float faceRY = 13.f + f.jaw * 2.f;
        plotEllipse(gcx, gcy, faceRX, faceRY, bright * 0.5f);

        // ── Jawline ──
        float jawDrop = f.jaw * 2.5f;
        plotQuad(gcx - 8.f, gcy + 1.f,
                 gcx, gcy + 12.f + jawDrop,
                 gcx + 8.f, gcy + 1.f, bright * 0.35f);

        // ── Left eyebrow ──
        float browLH = gcy - 7.f - f.browL * 1.5f + f.browDownL * 1.f;
        float browInner = f.browInnerUp * 1.2f;
        plotLine(gcx - 7.f, browLH + 0.5f,
                 gcx - 1.5f, browLH - 0.5f - browInner, bright * 0.9f);

        // ── Right eyebrow ──
        float browRH = gcy - 7.f - f.browR * 1.5f + f.browDownR * 1.f;
        plotLine(gcx + 1.5f, browRH - 0.5f - browInner,
                 gcx + 7.f, browRH + 0.5f, bright * 0.9f);

        // ── Left eye ──
        float leOpen = f.leftEye * (1.f - f.blinkL);
        float leyH = 0.3f + leOpen * 1.8f;
        float lex = gcx - 4.f;
        float ley = gcy - 3.5f;
        plotEllipse(lex, ley, 2.5f, leyH, bright * 0.8f);

        // Left pupil
        if (leOpen > 0.25f) {
            float px = lex + f.gazeX * 1.f;
            float py = ley - f.gazeY * 0.6f;
            fillEllipse(px, py, 0.7f + leOpen * 0.2f, 0.7f + leOpen * 0.2f, bright);
        }

        // ── Right eye ──
        float reOpen = f.rightEye * (1.f - f.blinkR);
        float reyH = 0.3f + reOpen * 1.8f;
        float rex = gcx + 4.f;
        float rey = gcy - 3.5f;
        plotEllipse(rex, rey, 2.5f, reyH, bright * 0.8f);

        // Right pupil
        if (reOpen > 0.25f) {
            float px = rex + f.gazeX * 1.f;
            float py = rey - f.gazeY * 0.6f;
            fillEllipse(px, py, 0.7f + reOpen * 0.2f, 0.7f + reOpen * 0.2f, bright);
        }

        // ── Nose (subtle) ──
        plotLine(gcx, gcy - 1.f, gcx - 1.f, gcy + 2.f, bright * 0.25f);
        plotLine(gcx - 1.f, gcy + 2.f, gcx + 1.f, gcy + 2.f, bright * 0.25f);

        // ── Mouth ──
        float mw = 2.5f + f.mouthW * 3.f;
        float mh = 0.5f + f.mouthH * 4.f;
        float my = gcy + 5.5f + f.jaw * 2.f;
        float lipsPurse = f.lips * 1.5f;
        mw = std::max(1.5f, mw - lipsPurse);

        // Upper lip curve
        plotQuad(gcx - mw, my,
                 gcx, my - mh * 0.25f,
                 gcx + mw, my, bright * 0.7f);

        // Lower lip curve
        plotQuad(gcx - mw, my,
                 gcx, my + mh,
                 gcx + mw, my, bright * 0.7f);

        // Mouth interior fill when open
        if (mh > 1.5f) {
            fillEllipse(gcx, my + mh * 0.3f, mw * 0.7f, mh * 0.35f, bright * 0.15f);
        }

        // ── Tongue ──
        if (f.tongue > 0.05f) {
            float ty = my + mh * 0.5f;
            float tlen = f.tongue * 3.f;
            float tw = 1.2f + f.tongue * 0.8f;
            fillEllipse(gcx, ty + tlen * 0.4f, tw, tlen, bright * f.tongue * 0.6f);
        }
    }

    // ── Rasterize "NO SIGNAL" text into grid ──
    void rasterizeNoSignal() {
        clearGrid();

        // Simple 3x5 pixel font for "NO SIGNAL"
        // Each char is stored as 5 rows of 3-bit patterns
        static const uint8_t font[][5] = {
            // N
            {0b101, 0b111, 0b111, 0b111, 0b101},
            // O
            {0b111, 0b101, 0b101, 0b101, 0b111},
            // (space)
            {0b000, 0b000, 0b000, 0b000, 0b000},
            // S
            {0b111, 0b100, 0b111, 0b001, 0b111},
            // I
            {0b111, 0b010, 0b010, 0b010, 0b111},
            // G
            {0b111, 0b100, 0b101, 0b101, 0b111},
            // N
            {0b101, 0b111, 0b111, 0b111, 0b101},
            // A
            {0b010, 0b101, 0b111, 0b101, 0b101},
            // L
            {0b100, 0b100, 0b100, 0b100, 0b111},
        };

        int numChars = 9;
        int totalW = numChars * 4 - 1; // 3 wide + 1 gap each
        int startX = (GRID_W - totalW) / 2;
        int startY = GRID_H / 2 - 2;

        // Flicker based on frame
        float flicker = 0.15f + 0.1f * std::sin(frameCount * 0.05f);

        for (int c = 0; c < numChars; c++) {
            for (int row = 0; row < 5; row++) {
                for (int col = 0; col < 3; col++) {
                    if (font[c][row] & (1 << (2 - col))) {
                        int gx = startX + c * 4 + col;
                        int gy = startY + row;
                        setDot(gx, gy, flicker);
                    }
                }
            }
        }

        // Draw ghost face outline (very dim)
        float gcx = GRID_W / 2.f;
        float gcy = GRID_H / 2.f;
        plotEllipse(gcx, gcy, 10.f, 13.f, 0.04f);
        plotEllipse(gcx - 4.f, gcy - 3.5f, 2.5f, 1.5f, 0.03f);
        plotEllipse(gcx + 4.f, gcy - 3.5f, 2.5f, 1.5f, 0.03f);
    }

    // ═══ DRAWING ═══

    void draw(const DrawArgs& args) override {
        // CRT housing background
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

        nvgSave(args.vg);
        nvgScissor(args.vg, DX, DY, DW, DH);

        // Rasterize face to grid
        if (module && module->displayFace.valid) {
            rasterizeFace(module->displayFace);
        } else {
            rasterizeNoSignal();
        }

        // Apply phosphor persistence (slow decay)
        for (int i = 0; i < GRID_W * GRID_H; i++) {
            persist[i] = std::max(grid[i], persist[i] * 0.88f);
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

        // ── Render dot matrix ──
        float gridOffX = DX + (DW - GRID_W * DOT_SPACING) / 2.f;
        float gridOffY = DY + (DH - GRID_H * DOT_SPACING) / 2.f;

        for (int gy = 0; gy < GRID_H; gy++) {
            for (int gx = 0; gx < GRID_W; gx++) {
                float val = persist[gy * GRID_W + gx];
                if (val < 0.01f) continue;

                float px = gridOffX + gx * DOT_SPACING;
                float py = gridOffY + gy * DOT_SPACING;

                // Phosphor glow (larger, dimmer)
                if (val > 0.15f) {
                    nvgBeginPath(args.vg);
                    nvgCircle(args.vg, px, py, DOT_RADIUS * 2.5f);
                    nvgFillColor(args.vg, nvgRGBA(
                        (int)(P_R * val * 0.3f),
                        (int)(P_G * val * 0.3f),
                        (int)(P_B * val * 0.3f),
                        (int)(val * 40.f)));
                    nvgFill(args.vg);
                }

                // Core dot
                nvgBeginPath(args.vg);
                nvgCircle(args.vg, px, py, DOT_RADIUS);
                nvgFillColor(args.vg, nvgRGBA(
                    (int)(P_R * val),
                    (int)(P_G * val),
                    (int)(P_B * val),
                    (int)(40 + val * 215.f)));
                nvgFill(args.vg);
            }
        }

        // ── Scanlines ──
        float scanY = DY + std::fmod(frameCount * 1.2f, DH);
        for (float y = DY; y < DY + DH; y += 3.f) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, y, DW, 1.f);
            nvgFillColor(args.vg, nvgRGBA(0, 0, 0, 18));
            nvgFill(args.vg);
        }

        // Moving scanline beam
        nvgBeginPath(args.vg);
        nvgRect(args.vg, DX, scanY - 1.f, DW, 2.f);
        nvgFillColor(args.vg, nvgRGBA(P_R / 3, P_G / 3, P_B / 3, 20));
        nvgFill(args.vg);

        // ── CRT vignette (darker edges) ──
        {
            // Top edge
            NVGpaint vig = nvgLinearGradient(args.vg, DX, DY, DX, DY + 20.f,
                nvgRGBA(0, 0, 0, 80), nvgRGBA(0, 0, 0, 0));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY, DW, 20.f);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);

            // Bottom edge
            vig = nvgLinearGradient(args.vg, DX, DY + DH - 20.f, DX, DY + DH,
                nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 80));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY + DH - 20.f, DW, 20.f);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);

            // Left edge
            vig = nvgLinearGradient(args.vg, DX, DY, DX + 15.f, DY,
                nvgRGBA(0, 0, 0, 60), nvgRGBA(0, 0, 0, 0));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX, DY, 15.f, DH);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);

            // Right edge
            vig = nvgLinearGradient(args.vg, DX + DW - 15.f, DY, DX + DW, DY,
                nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 60));
            nvgBeginPath(args.vg);
            nvgRect(args.vg, DX + DW - 15.f, DY, 15.f, DH);
            nvgFillPaint(args.vg, vig);
            nvgFill(args.vg);
        }

        // ── FPS text ──
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

        // ── Screen reflection highlight ──
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

        // Camera toggle
        float y = 355.f;
        addParam(createParamCentered<VCVButton>(Vec(box.size.x / 2.f, y), module, Mirror::CAM_PARAM));
        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            Vec(box.size.x / 2.f + 14, y - 10), module, Mirror::CAM_GREEN_LIGHT));
    }

    void appendContextMenu(Menu* menu) override {
        Mirror* module = dynamic_cast<Mirror*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("MIRROR Settings"));

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

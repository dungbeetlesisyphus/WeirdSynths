// Depth.cpp — WeirdSynths DEPTH module
//
// Receives Kinect depth data from kinect_bridge.py via UDP (port 9005/9006)
// and outputs 10 CVs derived from the depth field.
//
// Supported sensors (auto-detected by bridge):
//   Kinect 360      — structured light, 640×480, 0.8m–4.0m
//   Kinect One      — ToF, 512×424, 0.5m–4.5m
//   Azure Kinect DK — ToF, 512×512, 0.25m–2.88m + 32-joint body tracking
//
// Outputs (10 total):
//   DIST   — nearest foreground object distance (0-10V = far..close)
//   MOTN   — motion energy / depth change (0-10V)
//   CNTX   — horizontal centroid (-5V..+5V, L..R)
//   CNTY   — vertical centroid   (-5V..+5V, up..down)
//   AREA   — foreground silhouette fraction (0-10V)
//   DPTH L — left zone depth (0-10V)
//   DPTH R — right zone depth (0-10V)
//   ENTR   — depth field entropy/complexity (0-10V)
//   BODY G — body presence gate (0 or 10V)
//   BODY N — body count (0/3.3/6.6/10V for 0/1/2/3 bodies)
//
// Parameters:
//   SMOOTH  — slew rate for all CV outputs (0=instant, 1=max smooth)
//   PORT    — UDP listen port (default 9005)
//
// Context menu:
//   Depth port  — set KINT listen port
//   Skel port   — set SKEL listen port
//   Source info — shows detected Kinect device

#include "plugin.hpp"
#include "DepthUDP.hpp"

using namespace depth;


struct Depth : Module {

    // ── Outputs ───────────────────────────────────────────
    enum OutputId {
        DIST_OUTPUT,
        MOTN_OUTPUT,
        CNTX_OUTPUT,
        CNTY_OUTPUT,
        AREA_OUTPUT,
        DPTHL_OUTPUT,
        DPTHR_OUTPUT,
        ENTR_OUTPUT,
        BODYGATE_OUTPUT,
        BODYCOUNT_OUTPUT,
        NUM_OUTPUTS
    };

    // ── Params ────────────────────────────────────────────
    enum ParamId {
        SMOOTH_PARAM,
        NUM_PARAMS
    };

    // ── Lights ────────────────────────────────────────────
    enum LightId {
        STATUS_LIGHT_R,
        STATUS_LIGHT_G,
        STATUS_LIGHT_B,
        NUM_LIGHTS
    };


    // ── State ─────────────────────────────────────────────
    DepthDataBuffer    depthBuf;
    SkeletonDataBuffer skelBuf;
    DepthUDPListener   listener{&depthBuf, &skelBuf};

    SlewLimiter slew[10];   // one per output

    int depthPort = 9005;
    int skelPort  = 9006;

    // Status
    uint64_t lastVersion = 0;
    float    signalAge   = 0.f;    // seconds since last packet
    bool     hasSignal   = false;

    // Display state (read by widget)
    std::atomic<float> displayDist{0.f};
    std::atomic<float> displayMotion{0.f};
    std::atomic<int>   displayBodies{0};
    std::atomic<uint8_t> displaySource{255};
    std::atomic<float> displayFPS{0.f};


    Depth() {
        config(NUM_PARAMS, 0, NUM_OUTPUTS, NUM_LIGHTS);

        configParam(SMOOTH_PARAM, 0.f, 0.98f, 0.88f, "Smoothing", "%", 0.f, 100.f);

        configOutput(DIST_OUTPUT,    "Distance (nearest object)");
        configOutput(MOTN_OUTPUT,    "Motion energy");
        configOutput(CNTX_OUTPUT,    "Centroid X (left/right)");
        configOutput(CNTY_OUTPUT,    "Centroid Y (up/down)");
        configOutput(AREA_OUTPUT,    "Foreground area");
        configOutput(DPTHL_OUTPUT,   "Depth left zone");
        configOutput(DPTHR_OUTPUT,   "Depth right zone");
        configOutput(ENTR_OUTPUT,    "Depth entropy");
        configOutput(BODYGATE_OUTPUT,"Body gate");
        configOutput(BODYCOUNT_OUTPUT,"Body count");

        listener.start(depthPort, skelPort);
    }

    ~Depth() {
        listener.stop();
    }

    void onReset() override {
        for (auto& s : slew) s.reset();
    }

    void process(const ProcessArgs& args) override {
        const float smooth = params[SMOOTH_PARAM].getValue();

        // ── Check for new depth data ──
        uint64_t version = depthBuf.getVersion();
        const DepthData& d = depthBuf.read();

        if (version != lastVersion && d.valid) {
            lastVersion = version;
            signalAge   = 0.f;
            hasSignal   = true;

            // Update display atomics (read by UI thread)
            displayDist.store(d.cvs.dist);
            displayMotion.store(d.cvs.motion);
            displayBodies.store(d.bodyCount);
            displaySource.store(static_cast<uint8_t>(d.source));
            displayFPS.store(listener.depthFPS.load());
        } else {
            signalAge += args.sampleTime;
            if (signalAge > 1.5f) hasSignal = false;
        }

        // ── CV Targets from latest depth data ──
        const DepthCVs& cvs = d.cvs;
        int bodyCount = d.bodyCount;
        bool hasBody  = bodyCount > 0;

        // Uni-polar (0-10V) outputs
        float tDist  = cvs.dist    * 10.f;
        float tMotn  = cvs.motion  * 10.f;
        float tArea  = cvs.area    * 10.f;
        float tDpthL = cvs.depthL  * 10.f;
        float tDpthR = cvs.depthR  * 10.f;
        float tEntr  = cvs.entropy * 10.f;
        float tGate  = hasBody     ? 10.f : 0.f;
        float tCnt   = std::min(bodyCount, 3) * (10.f / 3.f);   // 0/3.3/6.6/10V

        // Bi-polar (-5..+5V) for centroid
        float tCntX  = cvs.cntX * 5.f;
        float tCntY  = cvs.cntY * 5.f;

        // ── Apply slew and write outputs ──
        outputs[DIST_OUTPUT   ].setVoltage(slew[0].process(tDist,  smooth));
        outputs[MOTN_OUTPUT   ].setVoltage(slew[1].process(tMotn,  smooth));
        outputs[CNTX_OUTPUT   ].setVoltage(slew[2].process(tCntX,  smooth));
        outputs[CNTY_OUTPUT   ].setVoltage(slew[3].process(tCntY,  smooth));
        outputs[AREA_OUTPUT   ].setVoltage(slew[4].process(tArea,  smooth));
        outputs[DPTHL_OUTPUT  ].setVoltage(slew[5].process(tDpthL, smooth));
        outputs[DPTHR_OUTPUT  ].setVoltage(slew[6].process(tDpthR, smooth));
        outputs[ENTR_OUTPUT   ].setVoltage(slew[7].process(tEntr,  smooth));
        // Gate and count: no slew (they're already discrete)
        outputs[BODYGATE_OUTPUT ].setVoltage(tGate);
        outputs[BODYCOUNT_OUTPUT].setVoltage(tCnt);

        // ── Status LED ──
        // Green = signal, Amber = stale, Red = no signal
        if (hasSignal && d.valid) {
            float fps = displayFPS.load();
            if (fps >= 20.f) {
                // Green — healthy signal
                lights[STATUS_LIGHT_R].setBrightness(0.f);
                lights[STATUS_LIGHT_G].setBrightness(0.9f);
                lights[STATUS_LIGHT_B].setBrightness(0.1f);
            } else {
                // Amber — slow/degraded
                lights[STATUS_LIGHT_R].setBrightness(0.8f);
                lights[STATUS_LIGHT_G].setBrightness(0.4f);
                lights[STATUS_LIGHT_B].setBrightness(0.f);
            }
        } else {
            // Red — no signal
            lights[STATUS_LIGHT_R].setBrightness(0.7f);
            lights[STATUS_LIGHT_G].setBrightness(0.f);
            lights[STATUS_LIGHT_B].setBrightness(0.f);
        }
    }


    // ── JSON state persistence ──────────────────────────

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "depthPort", json_integer(depthPort));
        json_object_set_new(root, "skelPort",  json_integer(skelPort));
        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* dp = json_object_get(root, "depthPort");
        json_t* sp = json_object_get(root, "skelPort");
        bool portsChanged = false;

        if (dp && json_is_integer(dp)) {
            int p = (int)json_integer_value(dp);
            if (p != depthPort) { depthPort = p; portsChanged = true; }
        }
        if (sp && json_is_integer(sp)) {
            int p = (int)json_integer_value(sp);
            if (p != skelPort) { skelPort = p; portsChanged = true; }
        }
        if (portsChanged) {
            listener.stop();
            listener.start(depthPort, skelPort);
        }
    }
};


// ─────────────────────────────────────────────────────────
// Widget
// ─────────────────────────────────────────────────────────

struct DepthWidget : ModuleWidget {

    // ── Port-entry menu items ──

    struct PortMenuItem : ui::TextField {
        Depth* module;
        bool isDepthPort;

        PortMenuItem() {
            box.size.x = 80.f;
            multiline = false;
        }

        void onSelectKey(const SelectKeyEvent& e) override {
            if (e.key == GLFW_KEY_ENTER && module) {
                int port = std::stoi(text);
                port = rack::math::clamp(port, 1024, 65535);
                if (isDepthPort) {
                    module->depthPort = port;
                } else {
                    module->skelPort = port;
                }
                module->listener.stop();
                module->listener.start(module->depthPort, module->skelPort);
                ui::MenuOverlay::getAncestorOfType<ui::MenuOverlay>()->requestDelete();
                e.consume(this);
            }
            TextField::onSelectKey(e);
        }
    };


    DepthWidget(Depth* module) {
        setModule(module);

        // ── Panel ──
        // 14HP = 71.12mm
        box.size = Vec(14 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Depth.svg")));

        // ── Screws ──
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // ── Panel layout constants ──
        const float colL = 22.f;   // left column X
        const float colR = 50.f;   // right column X
        const float yStart = 52.f;
        const float yStep  = 30.f;

        // ── Smooth knob ──
        addParam(createParamCentered<RoundBlackKnob>(
            Vec(box.size.x / 2.f, 32.f), module, Depth::SMOOTH_PARAM));

        // ── Status RGB LED ──
        addChild(createLightCentered<MediumLight<RedGreenBlueLight>>(
            Vec(box.size.x - 14.f, 14.f), module,
            Depth::STATUS_LIGHT_R));

        // ── Outputs — left column ──
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colL, yStart + 0 * yStep), module, Depth::DIST_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colL, yStart + 1 * yStep), module, Depth::MOTN_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colL, yStart + 2 * yStep), module, Depth::CNTX_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colL, yStart + 3 * yStep), module, Depth::CNTY_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colL, yStart + 4 * yStep), module, Depth::AREA_OUTPUT));

        // ── Outputs — right column ──
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colR, yStart + 0 * yStep), module, Depth::DPTHL_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colR, yStart + 1 * yStep), module, Depth::DPTHR_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colR, yStart + 2 * yStep), module, Depth::ENTR_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colR, yStart + 3 * yStep), module, Depth::BODYGATE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(
            Vec(colR, yStart + 4 * yStep), module, Depth::BODYCOUNT_OUTPUT));
    }


    // ── Context Menu ──────────────────────────────────────

    void appendContextMenu(Menu* menu) override {
        Depth* module = dynamic_cast<Depth*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);

        // Source info
        uint8_t src = module->displaySource.load();
        std::string srcName = (src == 255)
            ? "No signal"
            : std::string(kinectSourceName(static_cast<KinectSource>(src)));
        float fps = module->displayFPS.load();
        int bodies = module->displayBodies.load();

        menu->addChild(createMenuLabel("─── Kinect Depth ───"));
        menu->addChild(createMenuLabel("Source: " + srcName));
        menu->addChild(createMenuLabel("FPS: " + std::to_string((int)fps)));
        menu->addChild(createMenuLabel("Bodies: " + std::to_string(bodies)));

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Depth port (default 9005):"));

        auto* dpField = new PortMenuItem;
        dpField->module = module;
        dpField->isDepthPort = true;
        dpField->text = std::to_string(module->depthPort);
        menu->addChild(dpField);

        menu->addChild(createMenuLabel("Skeleton port (default 9006):"));

        auto* spField = new PortMenuItem;
        spField->module = module;
        spField->isDepthPort = false;
        spField->text = std::to_string(module->skelPort);
        menu->addChild(spField);

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("─── Output Reference ───"));
        menu->addChild(createMenuLabel("DIST  0-10V  nearest object distance"));
        menu->addChild(createMenuLabel("MOTN  0-10V  motion energy"));
        menu->addChild(createMenuLabel("CNTX ±5V    centroid X (L/R)"));
        menu->addChild(createMenuLabel("CNTY ±5V    centroid Y (U/D)"));
        menu->addChild(createMenuLabel("AREA  0-10V  silhouette fraction"));
        menu->addChild(createMenuLabel("DPTH L/R  0-10V  zone depths"));
        menu->addChild(createMenuLabel("ENTR  0-10V  depth entropy"));
        menu->addChild(createMenuLabel("BODY G  0/10V   presence gate"));
        menu->addChild(createMenuLabel("BODY N  0/3.3/6.6/10V  body count"));
    }
};


Model* modelDepth = createModel<Depth, DepthWidget>("Depth");

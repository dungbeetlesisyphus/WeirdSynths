#include "plugin.hpp"
#include "NerveUDP.hpp"
#include "NerveSmoothing.hpp"
#include <cstdlib>


struct Nerve : Module {

    enum ParamId {
        SMOOTH_PARAM,
        SCALE_PARAM,
        LOOP_LEN_PARAM,
        REC_PARAM,
        CAM_PARAM,
        FACES_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        SMOOTH_INPUT,
        SCALE_INPUT,
        CLOCK_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        HEAD_X_OUTPUT,
        HEAD_Y_OUTPUT,
        HEAD_Z_OUTPUT,
        DIST_OUTPUT,
        L_EYE_OUTPUT,
        R_EYE_OUTPUT,
        GAZE_X_OUTPUT,
        GAZE_Y_OUTPUT,
        MOUTH_W_OUTPUT,
        MOUTH_H_OUTPUT,
        JAW_OUTPUT,
        LIPS_OUTPUT,
        BROW_L_OUTPUT,
        BROW_R_OUTPUT,
        BLINK_OUTPUT,
        EXPR_OUTPUT,
        TONGUE_OUTPUT,
        BROW_INNER_UP_OUTPUT,
        BROW_DOWN_L_OUTPUT,
        BROW_DOWN_R_OUTPUT,

        ASYM_OUTPUT,
        INTNS_OUTPUT,
        SHAKE_OUTPUT,
        NOD_OUTPUT,
        TENSION_OUTPUT,
        MICRO1_OUTPUT,
        MICRO2_OUTPUT,
        EMOTION_OUTPUT,

        LOOP1_OUTPUT,
        LOOP2_OUTPUT,
        LOOP3_OUTPUT,
        LOOP4_OUTPUT,

        OUTPUTS_LEN
    };

    enum LightId {
        CAM_GREEN_LIGHT,
        CAM_RED_LIGHT,
        REC_LIGHT,
        CONNECT_LIGHT,
        LIGHTS_LEN
    };

    static constexpr int NUM_RAW_OUTPUTS = 20;

    nerve::FaceDataBuffer faceDataBuffer;
    nerve::UDPListener udpListener{&faceDataBuffer};
    nerve::SlewSmoother smoothers[NUM_RAW_OUTPUTS];
    nerve::TimeoutTracker timeout;

    dsp::PulseGenerator blinkPulse;
    bool lastBlink = false;
    uint64_t lastSeenVersion = 0;

    int udpPort = 9000;
    float faceTimeoutSec = 0.5f;

    dsp::ClockDivider threadCheckDivider;


    Nerve() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(SMOOTH_PARAM, 0.f, 1.f, 0.15f, "Smoothing", " ms", 0.f, 500.f);
        configParam(SCALE_PARAM, 0.f, 1.f, 1.f, "Output Scale", "%", 0.f, 100.f);
        configParam(LOOP_LEN_PARAM, 0.5f, 8.f, 2.f, "Loop Length", " s");
        configParam(REC_PARAM, 0.f, 1.f, 0.f, "Record Gesture");
        configParam(CAM_PARAM, 0.f, 1.f, 1.f, "Camera Enable");
        configParam(FACES_PARAM, 0.f, 1.f, 0.f, "Face Mode");

        configInput(SMOOTH_INPUT, "Smoothing CV");
        configInput(SCALE_INPUT, "Scale CV");
        configInput(CLOCK_INPUT, "Clock Sync");

        configOutput(HEAD_X_OUTPUT, "Head X (Yaw)");
        configOutput(HEAD_Y_OUTPUT, "Head Y (Pitch)");
        configOutput(HEAD_Z_OUTPUT, "Head Z (Roll)");
        configOutput(DIST_OUTPUT,   "Distance");
        configOutput(L_EYE_OUTPUT,  "Left Eye");
        configOutput(R_EYE_OUTPUT,  "Right Eye");
        configOutput(GAZE_X_OUTPUT, "Gaze X");
        configOutput(GAZE_Y_OUTPUT, "Gaze Y");
        configOutput(MOUTH_W_OUTPUT,"Mouth Width");
        configOutput(MOUTH_H_OUTPUT,"Mouth Height");
        configOutput(JAW_OUTPUT,    "Jaw");
        configOutput(LIPS_OUTPUT,   "Lips");
        configOutput(BROW_L_OUTPUT, "Left Brow");
        configOutput(BROW_R_OUTPUT, "Right Brow");
        configOutput(BLINK_OUTPUT,  "Blink Trigger");
        configOutput(EXPR_OUTPUT,   "Expression");
        configOutput(TONGUE_OUTPUT, "Tongue");
        configOutput(BROW_INNER_UP_OUTPUT, "Brow Inner Up");
        configOutput(BROW_DOWN_L_OUTPUT,   "Brow Down Left");
        configOutput(BROW_DOWN_R_OUTPUT,   "Brow Down Right");

        configOutput(ASYM_OUTPUT,    "Asymmetry");
        configOutput(INTNS_OUTPUT,   "Intensity");
        configOutput(SHAKE_OUTPUT,   "Head Shake");
        configOutput(NOD_OUTPUT,     "Nod");
        configOutput(TENSION_OUTPUT, "Tension");
        configOutput(MICRO1_OUTPUT,  "Micro: Surprise");
        configOutput(MICRO2_OUTPUT,  "Micro: Disgust");
        configOutput(EMOTION_OUTPUT, "Emotion");

        configOutput(LOOP1_OUTPUT, "Loop 1");
        configOutput(LOOP2_OUTPUT, "Loop 2");
        configOutput(LOOP3_OUTPUT, "Loop 3");
        configOutput(LOOP4_OUTPUT, "Loop 4");

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

    void onReset() override {
        for (int i = 0; i < NUM_RAW_OUTPUTS; i++) {
            smoothers[i].reset();
        }
        lastBlink = false;
        lastSeenVersion = 0;
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

        float targets[NUM_RAW_OUTPUTS];

        if (faceValid) {
            targets[HEAD_X_OUTPUT]  = face.headX * 5.f;
            targets[HEAD_Y_OUTPUT]  = face.headY * 5.f;
            targets[HEAD_Z_OUTPUT]  = face.headZ * 5.f;
            targets[DIST_OUTPUT]    = face.headDist * 10.f;
            targets[L_EYE_OUTPUT]   = face.leftEye * 10.f;
            targets[R_EYE_OUTPUT]   = face.rightEye * 10.f;
            targets[GAZE_X_OUTPUT]  = face.gazeX * 5.f;
            targets[GAZE_Y_OUTPUT]  = face.gazeY * 5.f;
            targets[MOUTH_W_OUTPUT] = face.mouthW * 10.f;
            targets[MOUTH_H_OUTPUT] = face.mouthH * 10.f;
            targets[JAW_OUTPUT]     = face.jaw * 10.f;
            targets[LIPS_OUTPUT]    = face.lips * 10.f;
            targets[BROW_L_OUTPUT]  = face.browL * 10.f;
            targets[BROW_R_OUTPUT]  = face.browR * 10.f;
            targets[BLINK_OUTPUT]   = 0.f;
            targets[EXPR_OUTPUT]    = face.expression * 10.f;
            targets[TONGUE_OUTPUT]  = face.tongue * 10.f;
            targets[BROW_INNER_UP_OUTPUT] = face.browInnerUp * 10.f;
            targets[BROW_DOWN_L_OUTPUT]   = face.browDownL * 10.f;
            targets[BROW_DOWN_R_OUTPUT]   = face.browDownR * 10.f;
        } else {
            for (int i = 0; i < NUM_RAW_OUTPUTS; i++) {
                targets[i] = 0.f;
            }
        }

        float smoothParam = params[SMOOTH_PARAM].getValue();
        float smoothCV = inputs[SMOOTH_INPUT].isConnected()
            ? inputs[SMOOTH_INPUT].getVoltage() / 10.f : 0.f;
        float smoothTime = math::clamp(smoothParam + smoothCV, 0.f, 2.f);

        float scaleParam = params[SCALE_PARAM].getValue();
        float scaleCV = inputs[SCALE_INPUT].isConnected()
            ? inputs[SCALE_INPUT].getVoltage() / 10.f : 0.f;
        float scale = math::clamp(scaleParam + scaleCV, 0.f, 2.f);

        for (int i = 0; i < NUM_RAW_OUTPUTS; i++) {
            if (i == BLINK_OUTPUT) continue;
            float smoothed = smoothers[i].process(targets[i], smoothTime, args.sampleTime);
            outputs[i].setVoltage(smoothed * scale);
        }

        bool blinkNow = faceValid && (face.blinkL > 0.5f || face.blinkR > 0.5f);
        if (blinkNow && !lastBlink) {
            blinkPulse.trigger(1e-3f);
        }
        lastBlink = blinkNow;
        outputs[BLINK_OUTPUT].setVoltage(
            blinkPulse.process(args.sampleTime) ? 10.f : 0.f);

        lights[CAM_GREEN_LIGHT].setSmoothBrightness(
            faceValid ? 1.f : 0.f, args.sampleTime);
        lights[CAM_RED_LIGHT].setSmoothBrightness(
            (!faceValid && camEnabled) ? 1.f : 0.f, args.sampleTime);
        lights[CONNECT_LIGHT].setSmoothBrightness(
            faceValid ? 1.f : 0.f, args.sampleTime);

        for (int i = ASYM_OUTPUT; i <= EMOTION_OUTPUT; i++) {
            outputs[i].setVoltage(0.f);
        }
        for (int i = LOOP1_OUTPUT; i <= LOOP4_OUTPUT; i++) {
            outputs[i].setVoltage(0.f);
        }
    }

    json_t* dataToJson() override {
        json_t* root = json_object();
        json_object_set_new(root, "udpPort", json_integer(udpPort));
        json_object_set_new(root, "faceTimeout", json_real(faceTimeoutSec));
        json_object_set_new(root, "version", json_integer(1));
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


struct NerveWidget : ModuleWidget {

    NerveWidget(Nerve* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Nerve.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        const float LEFT = 62.f;
        const float RIGHT = 165.f;
        const float COL2 = 55.f;
        const float COL4 = 130.f;
        const float COL6 = 200.f;

        addChild(createLightCentered<SmallLight<GreenLight>>(
            Vec(280, 18), module, Nerve::CONNECT_LIGHT));

        float y;

        y = 50.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(LEFT, y), module, Nerve::HEAD_X_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(RIGHT, y), module, Nerve::HEAD_Y_OUTPUT));

        y = 78.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(LEFT, y), module, Nerve::HEAD_Z_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(RIGHT, y), module, Nerve::DIST_OUTPUT));

        y = 110.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(LEFT, y), module, Nerve::L_EYE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(RIGHT, y), module, Nerve::R_EYE_OUTPUT));

        y = 138.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(LEFT, y), module, Nerve::GAZE_X_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(RIGHT, y), module, Nerve::GAZE_Y_OUTPUT));

        y = 170.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(LEFT, y), module, Nerve::MOUTH_W_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(RIGHT, y), module, Nerve::MOUTH_H_OUTPUT));

        y = 198.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(LEFT, y), module, Nerve::JAW_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(RIGHT, y), module, Nerve::LIPS_OUTPUT));

        y = 226.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(LEFT, y), module, Nerve::BROW_L_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(RIGHT, y), module, Nerve::BROW_R_OUTPUT));

        y = 254.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(LEFT, y), module, Nerve::BLINK_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(RIGHT, y), module, Nerve::EXPR_OUTPUT));

        y = 278.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(LEFT, y), module, Nerve::TONGUE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(Vec(RIGHT, y), module, Nerve::BROW_INNER_UP_OUTPUT));

        y = 298.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(COL2, y), module, Nerve::SMOOTH_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(COL4, y), module, Nerve::SCALE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(COL6, y), module, Nerve::LOOP_LEN_PARAM));

        y = 326.f;
        addInput(createInputCentered<PJ301MPort>(Vec(COL2, y), module, Nerve::SMOOTH_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(COL4, y), module, Nerve::SCALE_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(COL6, y), module, Nerve::CLOCK_INPUT));

        y = 358.f;
        addParam(createParamCentered<VCVButton>(Vec(COL2, y), module, Nerve::CAM_PARAM));
        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            Vec(COL2 + 14, y - 10), module, Nerve::CAM_GREEN_LIGHT));

        addParam(createParamCentered<VCVButton>(Vec(COL4, y), module, Nerve::REC_PARAM));
        addChild(createLightCentered<SmallLight<RedLight>>(
            Vec(COL4 + 14, y - 10), module, Nerve::REC_LIGHT));

        addParam(createParamCentered<CKSS>(Vec(COL6, y), module, Nerve::FACES_PARAM));
    }

    void appendContextMenu(Menu* menu) override {
        Nerve* module = dynamic_cast<Nerve*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("NERVE Settings"));

        struct PortField : ui::TextField {
            Nerve* module;
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
        portField->placeholder = "9000";

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


Model* modelNerve = createModel<Nerve, NerveWidget>("Nerve");

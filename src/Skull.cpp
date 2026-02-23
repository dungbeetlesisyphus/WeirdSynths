#include "plugin.hpp"
#include "NerveUDP.hpp"
#include "NerveSmoothing.hpp"
#include "SkullDSP.hpp"
#include <cstdlib>


struct Skull : Module {

    enum ParamId {
        KIT_PARAM,
        SENS_PARAM,
        DECAY_PARAM,
        TONE_PARAM,
        PAN_PARAM,
        LEVEL_PARAM,
        CAM_PARAM,
        MUTE_PARAM,
        MODE_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        TRIG_INPUT,
        ACCENT_INPUT,
        CLOCK_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        KICK_OUTPUT,
        SNARE_OUTPUT,
        CH_OUTPUT,
        OH_OUTPUT,
        MIX_L_OUTPUT,
        MIX_R_OUTPUT,
        OUTPUTS_LEN
    };

    enum LightId {
        CAM_GREEN_LIGHT,
        CAM_RED_LIGHT,
        KICK_LIGHT,
        SNARE_LIGHT,
        CH_LIGHT,
        OH_LIGHT,
        LIGHTS_LEN
    };

    nerve::FaceDataBuffer faceDataBuffer;
    nerve::UDPListener udpListener{&faceDataBuffer};
    nerve::TimeoutTracker timeout;
    skull::DrumEngine drums;

    uint64_t lastSeenVersion = 0;
    int udpPort = 9001;  // Different default from NERVE
    float faceTimeoutSec = 0.5f;

    dsp::ClockDivider threadCheckDivider;
    dsp::ClockDivider lightDivider;

    // Light pulse trackers
    float kickLightVal = 0.f;
    float snareLightVal = 0.f;
    float chLightVal = 0.f;
    float ohLightVal = 0.f;


    Skull() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configParam(KIT_PARAM, 0.f, 1.f, 0.f, "Kit", "", 0.f, 1.f);
        configParam(SENS_PARAM, 0.f, 1.f, 0.6f, "Sensitivity", "%", 0.f, 100.f);
        configParam(DECAY_PARAM, 0.f, 1.f, 0.5f, "Decay", "", 0.f, 1.f);
        configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone", "", 0.f, 1.f);
        configParam(PAN_PARAM, -1.f, 1.f, 0.f, "Pan");
        configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level", "%", 0.f, 100.f);
        configParam(CAM_PARAM, 0.f, 1.f, 1.f, "Camera Enable");
        configParam(MUTE_PARAM, 0.f, 1.f, 0.f, "Mute");
        configParam(MODE_PARAM, 0.f, 1.f, 0.f, "Mode");

        configInput(TRIG_INPUT, "External Trigger");
        configInput(ACCENT_INPUT, "Accent CV");
        configInput(CLOCK_INPUT, "Clock Sync");

        configOutput(KICK_OUTPUT, "Kick");
        configOutput(SNARE_OUTPUT, "Snare");
        configOutput(CH_OUTPUT, "Closed Hi-Hat");
        configOutput(OH_OUTPUT, "Open Hi-Hat");
        configOutput(MIX_L_OUTPUT, "Mix Left");
        configOutput(MIX_R_OUTPUT, "Mix Right");

        timeout.setTimeoutSeconds(faceTimeoutSec);
        threadCheckDivider.setDivision(1024);
        lightDivider.setDivision(256);
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
        lastSeenVersion = 0;
    }

    void process(const ProcessArgs& args) override {

        // Manage UDP thread
        bool camEnabled = params[CAM_PARAM].getValue() > 0.5f;
        if (threadCheckDivider.process()) {
            if (camEnabled && !udpListener.isRunning()) {
                udpListener.start(udpPort);
            } else if (!camEnabled && udpListener.isRunning()) {
                udpListener.stop();
            }
        }

        // Read face data
        const nerve::FaceData& face = faceDataBuffer.read();

        uint64_t currentVersion = faceDataBuffer.getVersion();
        if (currentVersion != lastSeenVersion) {
            lastSeenVersion = currentVersion;
            timeout.reset();
        }
        timeout.tick(args.sampleTime);
        bool faceValid = face.valid && !timeout.isTimedOut();
        bool muted = params[MUTE_PARAM].getValue() > 0.5f;

        // Read parameters
        float kit = params[KIT_PARAM].getValue();
        float sensitivity = params[SENS_PARAM].getValue();
        float decay = params[DECAY_PARAM].getValue();
        float tone = params[TONE_PARAM].getValue();
        float pan = params[PAN_PARAM].getValue();
        float level = params[LEVEL_PARAM].getValue();

        // Face values (or zero if no face)
        float blinkL = faceValid ? face.blinkL : 0.f;
        float blinkR = faceValid ? face.blinkR : 0.f;
        float jaw = faceValid ? face.jaw : 0.f;
        float browL = faceValid ? face.browL : 0.f;
        float browR = faceValid ? face.browR : 0.f;
        float mouthW = faceValid ? face.mouthW : 0.f;
        float headX = faceValid ? face.headX : 0.f;
        float headY = faceValid ? face.headY : 0.f;
        float expression = faceValid ? face.expression : 0.5f;

        // Process drums
        if (!muted) {
            drums.process(
                blinkL, blinkR, jaw, browL, browR,
                mouthW, headX, headY, expression,
                kit, sensitivity, decay, tone, pan, level,
                args.sampleRate
            );
        } else {
            drums.kickOut = 0.f;
            drums.snareOut = 0.f;
            drums.chOut = 0.f;
            drums.ohOut = 0.f;
            drums.mixL = 0.f;
            drums.mixR = 0.f;
        }

        // Set outputs
        outputs[KICK_OUTPUT].setVoltage(drums.kickOut);
        outputs[SNARE_OUTPUT].setVoltage(drums.snareOut);
        outputs[CH_OUTPUT].setVoltage(drums.chOut);
        outputs[OH_OUTPUT].setVoltage(drums.ohOut);
        outputs[MIX_L_OUTPUT].setVoltage(drums.mixL * 5.f);
        outputs[MIX_R_OUTPUT].setVoltage(drums.mixR * 5.f);

        // Update lights (at reduced rate)
        if (lightDivider.process()) {
            // Trigger lights flash on drum hits
            if (std::abs(drums.kickOut) > 0.1f) kickLightVal = 1.f;
            if (std::abs(drums.snareOut) > 0.1f) snareLightVal = 1.f;
            if (std::abs(drums.chOut) > 0.1f) chLightVal = 1.f;
            if (std::abs(drums.ohOut) > 0.1f) ohLightVal = 1.f;

            float lightDecay = 0.85f;
            kickLightVal *= lightDecay;
            snareLightVal *= lightDecay;
            chLightVal *= lightDecay;
            ohLightVal *= lightDecay;

            lights[KICK_LIGHT].setBrightness(kickLightVal);
            lights[SNARE_LIGHT].setBrightness(snareLightVal);
            lights[CH_LIGHT].setBrightness(chLightVal);
            lights[OH_LIGHT].setBrightness(ohLightVal);

            lights[CAM_GREEN_LIGHT].setSmoothBrightness(
                faceValid ? 1.f : 0.f, args.sampleTime * 256);
            lights[CAM_RED_LIGHT].setSmoothBrightness(
                (!faceValid && camEnabled) ? 1.f : 0.f, args.sampleTime * 256);
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


struct SkullWidget : ModuleWidget {

    SkullWidget(Skull* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/Skull.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        const float COL1 = 62.f;
        const float COL2 = 165.f;
        const float COL3 = 242.f;

        const float KCOL1 = 55.f;
        const float KCOL2 = 130.f;
        const float KCOL3 = 200.f;

        float y;

        // Voice outputs with activity lights
        y = 170.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(COL1, y), module, Skull::KICK_OUTPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(COL1 + 16, y - 10), module, Skull::KICK_LIGHT));

        addOutput(createOutputCentered<PJ301MPort>(Vec(COL2, y), module, Skull::SNARE_OUTPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(COL2 + 16, y - 10), module, Skull::SNARE_LIGHT));

        y = 200.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(COL1, y), module, Skull::CH_OUTPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(COL1 + 16, y - 10), module, Skull::CH_LIGHT));

        addOutput(createOutputCentered<PJ301MPort>(Vec(COL2, y), module, Skull::OH_OUTPUT));
        addChild(createLightCentered<SmallLight<RedLight>>(Vec(COL2 + 16, y - 10), module, Skull::OH_LIGHT));

        // Stereo mix outputs
        y = 170.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(COL3, y), module, Skull::MIX_L_OUTPUT));

        y = 200.f;
        addOutput(createOutputCentered<PJ301MPort>(Vec(COL3, y), module, Skull::MIX_R_OUTPUT));

        // Controls row 1: KIT, SENS, DECAY
        y = 250.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(KCOL1, y), module, Skull::KIT_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(KCOL2, y), module, Skull::SENS_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(KCOL3, y), module, Skull::DECAY_PARAM));

        // Controls row 2: TONE, PAN, LEVEL
        y = 285.f;
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(KCOL1, y), module, Skull::TONE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(KCOL2, y), module, Skull::PAN_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(Vec(KCOL3, y), module, Skull::LEVEL_PARAM));

        // Inputs row
        y = 320.f;
        addInput(createInputCentered<PJ301MPort>(Vec(KCOL1, y), module, Skull::TRIG_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(KCOL2, y), module, Skull::ACCENT_INPUT));
        addInput(createInputCentered<PJ301MPort>(Vec(KCOL3, y), module, Skull::CLOCK_INPUT));

        // Buttons
        y = 355.f;
        addParam(createParamCentered<VCVButton>(Vec(KCOL1, y), module, Skull::CAM_PARAM));
        addChild(createLightCentered<SmallLight<GreenRedLight>>(
            Vec(KCOL1 + 14, y - 10), module, Skull::CAM_GREEN_LIGHT));

        addParam(createParamCentered<VCVButton>(Vec(KCOL2, y), module, Skull::MUTE_PARAM));
        addParam(createParamCentered<CKSS>(Vec(KCOL3, y), module, Skull::MODE_PARAM));
    }

    void appendContextMenu(Menu* menu) override {
        Skull* module = dynamic_cast<Skull*>(this->module);
        if (!module) return;

        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("SKULL Settings"));

        struct PortField : ui::TextField {
            Skull* module;
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
        portField->placeholder = "9001";

        menu->addChild(createMenuLabel("UDP Port"));
        menu->addChild(portField);

        // Kit selector submenu
        menu->addChild(createSubmenuItem("Kit", "", [=](Menu* subMenu) {
            subMenu->addChild(createCheckMenuItem("Analog (808)", "",
                [=]() { return module->params[Skull::KIT_PARAM].getValue() < 0.33f; },
                [=]() { module->params[Skull::KIT_PARAM].setValue(0.f); }
            ));
            subMenu->addChild(createCheckMenuItem("Digital (Glitch)", "",
                [=]() { return module->params[Skull::KIT_PARAM].getValue() >= 0.33f && module->params[Skull::KIT_PARAM].getValue() < 0.66f; },
                [=]() { module->params[Skull::KIT_PARAM].setValue(0.5f); }
            ));
            subMenu->addChild(createCheckMenuItem("Physical (Acoustic)", "",
                [=]() { return module->params[Skull::KIT_PARAM].getValue() >= 0.66f; },
                [=]() { module->params[Skull::KIT_PARAM].setValue(1.f); }
            ));
        }));

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


Model* modelSkull = createModel<Skull, SkullWidget>("Skull");

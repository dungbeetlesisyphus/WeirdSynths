#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;

    // Register modules
    p->addModel(modelNerve);
    p->addModel(modelSkull);
    p->addModel(modelMirror);
}

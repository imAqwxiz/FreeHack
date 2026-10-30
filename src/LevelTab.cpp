// Level tab: Safe Mode, Auto Safe Mode
#include "FreeHack.hpp"

namespace fh::level {
    namespace {
        bool g_cheated = false;          // a cheat was on during this attempt
        CCLabelBMFont* g_label = nullptr;

        bool safeNow() {
            return on("safemode") || (on("autosafe") && g_cheated);
        }
    }

    void init() {
        addOpt("Level", Opt{
            "safemode", "Safe Mode", "Completions never save. Always on.", false, false, {}
        });
        addOpt("Level", Opt{
            "autosafe", "Auto Safe Mode", "Turns Safe Mode on by itself when you use a cheat.", true, false, {}
        });
    }

    void onPlayInit(PlayLayer* pl) {
        auto win = CCDirector::get()->getWinSize();
        g_label = CCLabelBMFont::create("Safe Mode", "bigFont.fnt");
        g_label->setScale(0.4f);
        g_label->setOpacity(170);
        g_label->setAnchorPoint({0.f, 1.f});
        g_label->setPosition({8.f, win.height - 24.f});
        g_label->setZOrder(1001);
        g_label->setVisible(false);
        pl->addChild(g_label);
        g_cheated = false;
    }

    void onReset(PlayLayer*) {
        g_cheated = false;
    }

    void onUpdate(PlayLayer*, float) {
        if (anyCheatOn()) g_cheated = true;
        if (g_label) g_label->setVisible(safeNow());
    }

    // called right before the level is completed
    void beforeComplete(PlayLayer* pl) {
        if (anyCheatOn()) g_cheated = true;
        if (safeNow()) pl->m_isTestMode = true; // test runs never save
    }
}

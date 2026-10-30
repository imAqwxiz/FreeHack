#pragma once
// FreeHack - shared declarations. Every tab has its own .cpp that registers
// its modules (init) and holds its own gameplay logic.
#include <Geode/Geode.hpp>
#include <array>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace fh {
    // ---------------------------------------------------------------
    //  Describing a module and its settings
    // ---------------------------------------------------------------
    enum class CtlType { Header, Toggle, Slider, Color, Presets };

    struct Ctl {
        CtlType type = CtlType::Toggle;
        std::string id;
        std::string label;
        float min = 0.f;
        float max = 1.f;
        float def = 0.f;
        bool isInt = false;
        std::array<float, 3> col = {0.f, 0.f, 0.f};
        std::vector<float> presets;
    };

    // section title inside a settings popup
    inline Ctl header(std::string text) {
        Ctl c; c.type = CtlType::Header; c.label = std::move(text); return c;
    }
    inline Ctl toggle(std::string id, std::string label, bool def) {
        Ctl c; c.type = CtlType::Toggle; c.id = std::move(id); c.label = std::move(label);
        c.def = def ? 1.f : 0.f; return c;
    }
    inline Ctl slider(std::string id, std::string label, float min, float max, float def, bool isInt = false) {
        Ctl c; c.type = CtlType::Slider; c.id = std::move(id); c.label = std::move(label);
        c.min = min; c.max = max; c.def = def; c.isInt = isInt; return c;
    }
    // three sliders (idR, idG, idB) + a live swatch
    inline Ctl color(std::string id, std::string label, float r, float g, float b) {
        Ctl c; c.type = CtlType::Color; c.id = std::move(id); c.label = std::move(label);
        c.col = {r, g, b}; return c;
    }
    // quick-pick buttons that set an existing slider
    inline Ctl presets(std::string sliderId, std::vector<float> values) {
        Ctl c; c.type = CtlType::Presets; c.id = std::move(sliderId); c.presets = std::move(values); return c;
    }

    struct Opt {
        std::string id;
        std::string label;
        std::string desc;     // small grey text under the name
        bool def = false;     // on by default?
        bool cheat = false;   // counts as a cheat for Auto Safe Mode
        std::vector<Ctl> ctls; // empty = no Settings button
    };

    struct Tab {
        std::string name;
        std::vector<Opt> opts;
    };

    // ---------------------------------------------------------------
    //  Registry + saved settings (Registry.cpp)
    // ---------------------------------------------------------------
    std::vector<Tab>& tabs();
    void addOpt(std::string const& tab, Opt opt);

    float defaultOf(std::string const& id);
    bool on(std::string const& id);
    void setOn(std::string const& id, bool v);
    float num(std::string const& id);
    void setNum(std::string const& id, float v);

    bool anyCheatOn();   // is any module marked `cheat` enabled?
    void changed();      // call after a setting changes (applies audio etc.)

    // ---------------------------------------------------------------
    //  One namespace per tab
    // ---------------------------------------------------------------
    namespace player {
        void init();
        float speedMul();
        void applyAudio();
        void onPlayInit(PlayLayer* pl);
        void onReset(PlayLayer* pl);
        bool blockDeath(PlayLayer* pl, PlayerObject* player, GameObject* hazard); // true = swallow death
        void onUpdate(PlayLayer* pl, float dt);
        void onQuit();
    }
    namespace level {
        void init();
        void onPlayInit(PlayLayer* pl);
        void onReset(PlayLayer* pl);
        void onUpdate(PlayLayer* pl, float dt);
        void beforeComplete(PlayLayer* pl);
    }
    namespace editor  { void init(); }
    namespace visual  { void init(); }
    namespace utility { void init(); }
}

#include "FreeHack.hpp"
#include <unordered_map>

namespace fh {
    namespace {
        // defaults come from the modules themselves (see addOpt)
        std::unordered_map<std::string, float>& defs() {
            static std::unordered_map<std::string, float> m;
            return m;
        }
    }

    // tab order in the menu is decided here
    std::vector<Tab>& tabs() {
        static std::vector<Tab> t = {
            {"Player", {}},
            {"Level", {}},
            {"Editor", {}},
            {"Visual", {}},
            {"Utility", {}},
        };
        return t;
    }

    void addOpt(std::string const& tab, Opt opt) {
        defs()[opt.id] = opt.def ? 1.f : 0.f;
        for (auto const& c : opt.ctls) {
            switch (c.type) {
                case CtlType::Toggle:
                case CtlType::Slider:
                    defs()[c.id] = c.def;
                    break;
                case CtlType::Color:
                    defs()[c.id + "R"] = c.col[0];
                    defs()[c.id + "G"] = c.col[1];
                    defs()[c.id + "B"] = c.col[2];
                    break;
                default:
                    break;
            }
        }
        for (auto& t : tabs()) {
            if (t.name == tab) {
                t.opts.push_back(std::move(opt));
                return;
            }
        }
    }

    float defaultOf(std::string const& id) {
        auto it = defs().find(id);
        return it == defs().end() ? 0.f : it->second;
    }

    bool on(std::string const& id) {
        return Mod::get()->getSavedValue<bool>(id, defaultOf(id) > 0.5f);
    }
    void setOn(std::string const& id, bool v) {
        Mod::get()->setSavedValue<bool>(id, v);
    }
    float num(std::string const& id) {
        return Mod::get()->getSavedValue<float>(id, defaultOf(id));
    }
    void setNum(std::string const& id, float v) {
        Mod::get()->setSavedValue<float>(id, v);
    }

    bool anyCheatOn() {
        for (auto const& t : tabs()) {
            for (auto const& o : t.opts) {
                if (o.cheat && on(o.id)) return true;
            }
        }
        return false;
    }

    void changed() {
        player::applyAudio();
    }
}

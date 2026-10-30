// FreeHack - stage 2 (Geode v5)
#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCScheduler.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <unordered_map>

using namespace geode::prelude;

// =====================================================================
//  Settings helpers + the module table
// =====================================================================
namespace fh {
    inline std::unordered_map<std::string, float> const& defaults() {
        static std::unordered_map<std::string, float> d = {
            {"noclip", 0.f}, {"noclipTint", 1.f}, {"noclipAcc", 1.f},
            {"noclipMinAcc", 0.f},
            {"tintR", 255.f}, {"tintG", 0.f}, {"tintB", 0.f}, {"tintOpacity", 110.f},
            {"speedhack", 0.f}, {"speed", 0.5f}, {"speedAudio", 1.f},
            {"autoclick", 0.f}, {"cps", 10.f},
            {"safemode", 0.f}, {"autosafe", 1.f},
            {"waveSize", 0.f}, {"waveSizeVal", 1.f},
        };
        return d;
    }
    inline float defaultOf(std::string const& id) {
        auto it = defaults().find(id);
        return it == defaults().end() ? 0.f : it->second;
    }
    inline bool on(std::string const& id) {
        return Mod::get()->getSavedValue<bool>(id, defaultOf(id) > 0.5f);
    }
    inline void setOn(std::string const& id, bool v) {
        Mod::get()->setSavedValue<bool>(id, v);
    }
    inline float num(std::string const& id) {
        return Mod::get()->getSavedValue<float>(id, defaultOf(id));
    }
    inline void setNum(std::string const& id, float v) {
        Mod::get()->setSavedValue<float>(id, v);
    }

    // anything that gives an advantage
    inline bool cheating() {
        return on("noclip") || on("speedhack") || on("autoclick");
    }

    // speed only applies inside a level
    inline float speedMul() {
        return (PlayLayer::get() && on("speedhack")) ? num("speed") : 1.f;
    }

    inline void setPitch(float pitch) {
        auto engine = FMODAudioEngine::get();
        if (!engine || !engine->m_system) return;
        FMOD::ChannelGroup* group = nullptr;
        engine->m_system->getMasterChannelGroup(&group);
        if (group) group->setPitch(pitch);
    }
    inline void applyAudio() {
        bool active = PlayLayer::get() && on("speedhack") && on("speedAudio");
        setPitch(active ? num("speed") : 1.f);
    }

    enum class CtlType { Toggle, Slider, Color };
    struct Ctl {
        CtlType type;
        char const* id;
        char const* label;
        float min;
        float max;
        bool isInt;
    };
    struct Opt {
        char const* id;
        char const* label;
        std::vector<Ctl> ctls; // empty = no settings button
    };
    struct Tab {
        char const* name;
        std::vector<Opt> opts;
    };

    // add a module = add a row here (+ the logic for it below)
    inline std::vector<Tab> const& tabs() {
        static std::vector<Tab> t = {
            {"Player", {
                {"noclip", "Noclip", {
                    {CtlType::Toggle, "noclipTint", "Tint on death", 0, 0, false},
                    {CtlType::Color,  "tint", "Tint color", 0, 255, true},
                    {CtlType::Slider, "tintOpacity", "Tint opacity", 0, 255, true},
                    {CtlType::Toggle, "noclipAcc", "Noclip accuracy", 0, 0, false},
                    {CtlType::Slider, "noclipMinAcc", "Min accuracy (0 = off)", 0, 100, true},
                }},
                {"speedhack", "Speedhack", {
                    {CtlType::Slider, "speed", "Speed", 0.1f, 3.f, false},
                }},
                {"speedAudio", "Speedhack Audio", {}},
                {"autoclick", "Auto Clicker", {
                    {CtlType::Slider, "cps", "Clicks per second", 1, 30, true},
                }},
            }},
            {"Level", {
                {"safemode", "Safe Mode", {}},
                {"autosafe", "Auto Safe Mode", {}},
            }},
            {"Editor", {}},
            {"Visual", {
                {"waveSize", "Wave Trail Size", {
                    {CtlType::Slider, "waveSizeVal", "Size", 0.1f, 3.f, false},
                }},
            }},
            {"Utility", {}},
        };
        return t;
    }
}

// =====================================================================
//  Settings popup (opened from a module's Settings button)
// =====================================================================
class FHSettingsPopup : public Popup {
protected:
    struct Row {
        std::string id;
        float min;
        float max;
        bool isInt;
        std::string name;
        CCLabelBMFont* node = nullptr;

        std::string text(float v) const {
            if (isInt) return fmt::format("{}: {}", name, static_cast<int>(std::round(v)));
            return fmt::format("{}: {:.2f}", name, v);
        }
    };

    std::vector<Row> m_rows;
    CCLayerColor* m_swatch = nullptr;

    void addSlider(std::string const& id, std::string const& name, float min, float max, bool isInt, float y) {
        float w = m_mainLayer->getContentSize().width;

        Row r;
        r.id = id; r.min = min; r.max = max; r.isInt = isInt; r.name = name;
        float val = fh::num(id);

        r.node = CCLabelBMFont::create(r.text(val).c_str(), "bigFont.fnt");
        r.node->setScale(0.38f);
        r.node->setAnchorPoint({0.f, 0.5f});
        r.node->setPosition({18.f, y});
        m_mainLayer->addChild(r.node);

        auto slider = Slider::create(this, menu_selector(FHSettingsPopup::onSlider), 0.55f);
        slider->setPosition({w - 125.f, y});
        slider->setValue((val - min) / (max - min));
        slider->getThumb()->setTag(static_cast<int>(m_rows.size()));
        m_mainLayer->addChild(slider);

        m_rows.push_back(r);
    }

    void updateSwatch() {
        if (!m_swatch) return;
        auto u8 = [](float v) { return static_cast<GLubyte>(std::clamp(v, 0.f, 255.f)); };
        m_swatch->setColor({u8(fh::num("tintR")), u8(fh::num("tintG")), u8(fh::num("tintB"))});
    }

    bool init(fh::Opt const& opt) {
        float contentH = 0.f;
        for (auto const& c : opt.ctls) {
            contentH += (c.type == fh::CtlType::Color ? 3.f : 1.f) * 28.f;
        }
        float winH = CCDirector::get()->getWinSize().height;
        float h = std::min(contentH + 70.f, winH - 20.f);
        float w = 380.f;

        if (!Popup::init(w, h)) return false;
        this->setTitle(fmt::format("{} Settings", opt.label));

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(menu);

        float y = h - 55.f;
        for (auto const& c : opt.ctls) {
            if (c.type == fh::CtlType::Toggle) {
                auto t = CCMenuItemToggler::createWithStandardSprites(
                    this, menu_selector(FHSettingsPopup::onToggle), 0.5f
                );
                t->setID(c.id);
                t->toggle(fh::on(c.id));
                t->setPosition({30.f, y});
                menu->addChild(t);

                auto lbl = CCLabelBMFont::create(c.label, "bigFont.fnt");
                lbl->setScale(0.4f);
                lbl->setAnchorPoint({0.f, 0.5f});
                lbl->setPosition({52.f, y});
                m_mainLayer->addChild(lbl);
                y -= 28.f;
            }
            else if (c.type == fh::CtlType::Slider) {
                this->addSlider(c.id, c.label, c.min, c.max, c.isInt, y);
                y -= 28.f;
            }
            else { // Color: three sliders + a swatch
                std::string base = c.id;
                this->addSlider(base + "R", std::string(c.label) + " R", 0.f, 255.f, true, y);
                this->addSlider(base + "G", std::string(c.label) + " G", 0.f, 255.f, true, y - 28.f);
                this->addSlider(base + "B", std::string(c.label) + " B", 0.f, 255.f, true, y - 56.f);

                m_swatch = CCLayerColor::create({255, 255, 255, 255}, 22.f, 22.f);
                m_swatch->setPosition({w - 41.f, y - 39.f});
                m_mainLayer->addChild(m_swatch);
                this->updateSwatch();
                y -= 84.f;
            }
        }
        return true;
    }

    void onToggle(CCObject* sender) {
        auto t = static_cast<CCMenuItemToggler*>(sender);
        // the state flips after the callback, so the new value is the opposite
        fh::setOn(t->getID(), !t->isToggled());
        fh::applyAudio();
    }

    void onSlider(CCObject* sender) {
        auto thumb = static_cast<SliderThumb*>(sender);
        int idx = thumb->getTag();
        if (idx < 0 || idx >= static_cast<int>(m_rows.size())) return;

        auto& r = m_rows[idx];
        float v = r.min + thumb->getValue() * (r.max - r.min);
        v = r.isInt ? std::round(v) : std::round(v * 20.f) / 20.f; // snap to 0.05
        fh::setNum(r.id, v);
        r.node->setString(r.text(v).c_str());

        this->updateSwatch();
        fh::applyAudio();
    }

public:
    static FHSettingsPopup* create(fh::Opt const& opt) {
        auto ret = new FHSettingsPopup();
        if (ret->init(opt)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// =====================================================================
//  Main menu popup: sidebar tabs on the left, module list on the right
// =====================================================================
class FreeHackPopup : public Popup {
protected:
    float m_w = 0.f;
    float m_h = 0.f;
    CCNode* m_content = nullptr;
    std::vector<CCMenuItemSpriteExtra*> m_tabBtns;

    bool init() {
        auto win = CCDirector::get()->getWinSize();
        m_w = std::min(win.width - 40.f, 460.f);
        m_h = std::min(win.height - 20.f, 290.f);

        if (!Popup::init(m_w, m_h)) return false;
        this->setTitle("FreeHack");

        // dark panel behind the module list
        auto panel = CCScale9Sprite::create("square02b_001.png", {0.f, 0.f, 80.f, 80.f});
        panel->setContentSize({m_w - 126.f, m_h - 74.f});
        panel->setAnchorPoint({0.f, 0.f});
        panel->setPosition({112.f, 14.f});
        panel->setColor({0, 0, 0});
        panel->setOpacity(90);
        m_mainLayer->addChild(panel);

        // sidebar tabs
        auto tabMenu = CCMenu::create();
        tabMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(tabMenu);

        auto const& tabs = fh::tabs();
        for (size_t i = 0; i < tabs.size(); i++) {
            auto spr = ButtonSprite::create(
                tabs[i].name, 84, true, "bigFont.fnt", "GJ_button_04.png", 28.f, 0.6f
            );
            auto btn = CCMenuItemSpriteExtra::create(
                spr, this, menu_selector(FreeHackPopup::onTab)
            );
            btn->setTag(static_cast<int>(i));
            btn->setPosition({62.f, m_h - 62.f - static_cast<float>(i) * 36.f});
            tabMenu->addChild(btn);
            m_tabBtns.push_back(btn);
        }

        this->showTab(0);
        return true;
    }

    void showTab(int i) {
        if (m_content) m_content->removeFromParent();
        m_content = CCNode::create();
        m_mainLayer->addChild(m_content);

        for (size_t j = 0; j < m_tabBtns.size(); j++) {
            auto spr = static_cast<ButtonSprite*>(m_tabBtns[j]->getNormalImage());
            spr->updateBGImage(static_cast<int>(j) == i ? "GJ_button_01.png" : "GJ_button_04.png");
        }

        auto const& tab = fh::tabs()[i];
        float areaX = 112.f;
        float areaW = m_w - 126.f;

        if (tab.opts.empty()) {
            auto lbl = CCLabelBMFont::create("Coming soon", "bigFont.fnt");
            lbl->setScale(0.6f);
            lbl->setOpacity(150);
            lbl->setPosition({areaX + areaW / 2.f, m_h / 2.f - 10.f});
            m_content->addChild(lbl);
            return;
        }

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        m_content->addChild(menu);

        float y = m_h - 60.f - 26.f;
        for (size_t j = 0; j < tab.opts.size(); j++) {
            auto const& o = tab.opts[j];

            auto toggler = CCMenuItemToggler::createWithStandardSprites(
                this, menu_selector(FreeHackPopup::onToggle), 0.6f
            );
            toggler->setID(o.id);
            toggler->toggle(fh::on(o.id));
            toggler->setPosition({areaX + 24.f, y});
            menu->addChild(toggler);

            auto lbl = CCLabelBMFont::create(o.label, "bigFont.fnt");
            lbl->setScale(0.5f);
            lbl->setAnchorPoint({0.f, 0.5f});
            lbl->setPosition({areaX + 46.f, y});
            m_content->addChild(lbl);

            if (!o.ctls.empty()) {
                auto spr = ButtonSprite::create(
                    "Settings", 72, true, "bigFont.fnt", "GJ_button_04.png", 24.f, 0.42f
                );
                auto btn = CCMenuItemSpriteExtra::create(
                    spr, this, menu_selector(FreeHackPopup::onSettings)
                );
                btn->setTag(i * 100 + static_cast<int>(j));
                btn->setPosition({areaX + areaW - 46.f, y});
                menu->addChild(btn);
            }
            y -= 38.f;
        }
    }

    void onTab(CCObject* sender) {
        this->showTab(static_cast<CCNode*>(sender)->getTag());
    }

    void onToggle(CCObject* sender) {
        auto t = static_cast<CCMenuItemToggler*>(sender);
        fh::setOn(t->getID(), !t->isToggled());
        fh::applyAudio();
    }

    void onSettings(CCObject* sender) {
        int tag = static_cast<CCNode*>(sender)->getTag();
        auto const& opt = fh::tabs()[tag / 100].opts[tag % 100];
        FHSettingsPopup::create(opt)->show();
    }

public:
    static FreeHackPopup* create() {
        auto ret = new FreeHackPopup();
        if (ret->init()) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

// =====================================================================
//  Buttons: main menu + pause menu
// =====================================================================
class $modify(FHMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto spr = ButtonSprite::create("FH");
        spr->setScale(0.8f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(FHMenuLayer::onFreeHack)
        );
        btn->setID("freehack-button"_spr);

        if (auto menu = this->getChildByID("bottom-menu")) {
            menu->addChild(btn);
            menu->updateLayout();
        }
        return true;
    }

    void onFreeHack(CCObject*) {
        FreeHackPopup::create()->show();
    }
};

class $modify(FHPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto menu = this->getChildByID("right-button-menu");
        if (!menu) return;

        auto spr = ButtonSprite::create("FH");
        spr->setScale(0.8f);
        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(FHPauseLayer::onFreeHack)
        );
        btn->setID("freehack-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();
    }

    void onFreeHack(CCObject*) {
        FreeHackPopup::create()->show();
    }
};

// =====================================================================
//  Speedhack: scale the scheduler's dt.
//  GD resets CCScheduler's time scale on every respawn (timewarp), which
//  is why setTimeScale barely worked. Multiplying dt here can't be reset.
// =====================================================================
class $modify(FHScheduler, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt * fh::speedMul());
    }
};

// =====================================================================
//  Wave trail size
// =====================================================================
class $modify(FHHardStreak, HardStreak) {
    void updateStroke(float dt) {
        if (fh::on("waveSize")) m_pulseSize = fh::num("waveSizeVal");
        HardStreak::updateStroke(dt);
    }
};

// =====================================================================
//  Gameplay: noclip, accuracy, tint, auto clicker, safe mode
// =====================================================================
class $modify(FHPlayLayer, PlayLayer) {
    struct Fields {
        int total = 0;
        int dead = 0;
        bool diedNow = false;
        bool cheated = false;
        float tintAlpha = 0.f;
        float clickTimer = 0.f;
        bool clickDown = false;
        CCLayerColor* tint = nullptr;
        CCLabelBMFont* accLabel = nullptr;
        CCLabelBMFont* safeLabel = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto win = CCDirector::get()->getWinSize();

        m_fields->tint = CCLayerColor::create({255, 0, 0, 0}, win.width, win.height);
        m_fields->tint->setZOrder(1000);
        this->addChild(m_fields->tint);

        m_fields->accLabel = CCLabelBMFont::create("", "bigFont.fnt");
        m_fields->accLabel->setScale(0.4f);
        m_fields->accLabel->setOpacity(170);
        m_fields->accLabel->setAnchorPoint({0.f, 1.f});
        m_fields->accLabel->setPosition({8.f, win.height - 8.f});
        m_fields->accLabel->setZOrder(1001);
        m_fields->accLabel->setVisible(false);
        this->addChild(m_fields->accLabel);

        m_fields->safeLabel = CCLabelBMFont::create("Safe Mode", "bigFont.fnt");
        m_fields->safeLabel->setScale(0.4f);
        m_fields->safeLabel->setOpacity(170);
        m_fields->safeLabel->setAnchorPoint({0.f, 1.f});
        m_fields->safeLabel->setPosition({8.f, win.height - 24.f});
        m_fields->safeLabel->setZOrder(1001);
        m_fields->safeLabel->setVisible(false);
        this->addChild(m_fields->safeLabel);

        fh::applyAudio();
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        m_fields->total = 0;
        m_fields->dead = 0;
        m_fields->diedNow = false;
        m_fields->cheated = false;
        m_fields->tintAlpha = 0.f;
        m_fields->clickTimer = 0.f;
    }

    // ---- noclip ----
    void destroyPlayer(PlayerObject* player, GameObject* object) {
        // let the anti-cheat spike through, and everything when noclip is off
        if (!fh::on("noclip") || object == m_anticheatSpike) {
            PlayLayer::destroyPlayer(player, object);
            return;
        }

        // accuracy floor: die for real if accuracy would drop below the minimum
        float minAcc = fh::num("noclipMinAcc");
        if (minAcc > 0.f && m_fields->total > 0) {
            float acc = 100.f * (1.f - static_cast<float>(m_fields->dead + 1) / static_cast<float>(m_fields->total + 1));
            if (acc < minAcc) {
                PlayLayer::destroyPlayer(player, object);
                return;
            }
        }

        m_fields->diedNow = true;
        if (fh::on("noclipTint")) m_fields->tintAlpha = 1.f;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        if (fh::cheating()) m_fields->cheated = true;

        // ---- noclip accuracy ----
        bool noclip = fh::on("noclip");
        if (noclip) {
            m_fields->total++;
            if (m_fields->diedNow) m_fields->dead++;
        }
        m_fields->diedNow = false;

        if (m_fields->accLabel) {
            if (noclip && fh::on("noclipAcc") && m_fields->total > 0) {
                float acc = 100.f * (1.f - static_cast<float>(m_fields->dead) / static_cast<float>(m_fields->total));
                m_fields->accLabel->setString(fmt::format("Accuracy: {:.2f}%", acc).c_str());
                m_fields->accLabel->setVisible(true);
            } else {
                m_fields->accLabel->setVisible(false);
            }
        }

        // ---- death tint (fades out) ----
        if (m_fields->tint) {
            if (m_fields->tintAlpha > 0.f) {
                m_fields->tintAlpha = std::max(0.f, m_fields->tintAlpha - dt * 4.f);
                auto u8 = [](float v) { return static_cast<GLubyte>(std::clamp(v, 0.f, 255.f)); };
                m_fields->tint->setColor({u8(fh::num("tintR")), u8(fh::num("tintG")), u8(fh::num("tintB"))});
            }
            m_fields->tint->setOpacity(static_cast<GLubyte>(
                std::clamp(m_fields->tintAlpha * fh::num("tintOpacity"), 0.f, 255.f)
            ));
        }

        // ---- safe mode label ----
        if (m_fields->safeLabel) {
            bool safe = fh::on("safemode") || (fh::on("autosafe") && m_fields->cheated);
            m_fields->safeLabel->setVisible(safe);
        }

        // ---- auto clicker ----
        if (fh::on("autoclick") && m_player1 && !m_player1->m_isDead) {
            float period = 1.f / std::max(1.f, fh::num("cps"));
            m_fields->clickTimer += dt;
            bool down = std::fmod(m_fields->clickTimer, period) < period * 0.5f;
            if (down != m_fields->clickDown) {
                m_fields->clickDown = down;
                this->handleButton(down, 1, true);
            }
        } else if (m_fields->clickDown) {
            m_fields->clickDown = false;
            this->handleButton(false, 1, true);
        }
    }

    // ---- safe mode / auto safe mode ----
    void levelComplete() {
        if (fh::cheating()) m_fields->cheated = true;

        bool safe = fh::on("safemode") || (fh::on("autosafe") && m_fields->cheated);
        if (safe) m_isTestMode = true;

        PlayLayer::levelComplete();
    }

    void onQuit() {
        fh::setPitch(1.f);
        PlayLayer::onQuit();
    }
};

// FreeHack - stage 1: menu skeleton + speedhack + safe mode
#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// ---------- helpers ----------
namespace fh {
    inline bool getOpt(std::string const& id) {
        return Mod::get()->getSavedValue<bool>(id, false);
    }
    inline void setOpt(std::string const& id, bool v) {
        Mod::get()->setSavedValue<bool>(id, v);
    }
    inline float getSpeed() {
        return Mod::get()->getSavedValue<float>("speed", 0.5f);
    }
    // any option that gives an unfair advantage goes here
    inline bool cheating() {
        return getOpt("speedhack");
    }
    // speed only applies while inside a level
    inline void applySpeed() {
        float s = (PlayLayer::get() && getOpt("speedhack")) ? getSpeed() : 1.f;
        CCDirector::get()->getScheduler()->setTimeScale(s);
    }

    struct Opt { char const* id; char const* label; };
    struct Tab { char const* name; std::vector<Opt> opts; };

    // add new modules here and they show up in the menu
    inline std::vector<Tab> const& tabs() {
        static std::vector<Tab> t = {
            {"Player",  {{"speedhack", "Speedhack"}}},
            {"Level",   {}},
            {"Editor",  {}},
            {"Visual",  {}},
            {"Utility", {}},
        };
        return t;
    }
}

// ---------- the menu popup ----------
class FreeHackPopup : public Popup {
protected:
    int m_tab = 0;
    CCNode* m_content = nullptr;
    CCLabelBMFont* m_speedLabel = nullptr;
    std::vector<CCMenuItemSpriteExtra*> m_tabBtns;

    bool init() {
        if (!Popup::init(380.f, 240.f)) return false;
        this->setTitle("FreeHack");

        auto size = m_mainLayer->getContentSize();
        auto tabMenu = CCMenu::create();
        tabMenu->setPosition({0, 0});
        m_mainLayer->addChild(tabMenu);

        auto const& tabs = fh::tabs();
        for (size_t i = 0; i < tabs.size(); i++) {
            auto spr = ButtonSprite::create(
                tabs[i].name, 60, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.6f
            );
            auto btn = CCMenuItemSpriteExtra::create(
                spr, this, menu_selector(FreeHackPopup::onTab)
            );
            btn->setTag(static_cast<int>(i));
            btn->setPosition({size.width * (i + 0.5f) / tabs.size(), size.height - 50.f});
            tabMenu->addChild(btn);
            m_tabBtns.push_back(btn);
        }

        this->showTab(0);
        return true;
    }

    void showTab(int i) {
        m_tab = i;
        if (m_content) m_content->removeFromParent();
        m_speedLabel = nullptr;
        m_content = CCNode::create();
        m_mainLayer->addChild(m_content);

        for (size_t j = 0; j < m_tabBtns.size(); j++) {
            auto spr = static_cast<ButtonSprite*>(m_tabBtns[j]->getNormalImage());
            spr->updateBGImage(static_cast<int>(j) == i ? "GJ_button_01.png" : "GJ_button_04.png");
        }

        auto size = m_mainLayer->getContentSize();
        auto const& tab = fh::tabs()[i];

        if (tab.opts.empty()) {
            auto lbl = CCLabelBMFont::create("Coming soon", "bigFont.fnt");
            lbl->setScale(0.6f);
            lbl->setOpacity(150);
            lbl->setPosition({size.width / 2, size.height / 2 - 20.f});
            m_content->addChild(lbl);
            return;
        }

        auto menu = CCMenu::create();
        menu->setPosition({0, 0});
        m_content->addChild(menu);

        float y = size.height - 95.f;
        for (auto const& o : tab.opts) {
            auto toggler = CCMenuItemToggler::createWithStandardSprites(
                this, menu_selector(FreeHackPopup::onToggle), 0.6f
            );
            toggler->setID(o.id);
            toggler->toggle(fh::getOpt(o.id));
            toggler->setPosition({40.f, y});
            menu->addChild(toggler);

            auto lbl = CCLabelBMFont::create(o.label, "bigFont.fnt");
            lbl->setScale(0.5f);
            lbl->setAnchorPoint({0.f, 0.5f});
            lbl->setPosition({62.f, y});
            m_content->addChild(lbl);
            y -= 32.f;
        }

        // speed slider lives on the Player tab
        if (i == 0) {
            float speed = fh::getSpeed();
            m_speedLabel = CCLabelBMFont::create(
                fmt::format("Speed: {:.2f}x", speed).c_str(), "bigFont.fnt"
            );
            m_speedLabel->setScale(0.45f);
            m_speedLabel->setPosition({size.width / 2, y - 5.f});
            m_content->addChild(m_speedLabel);

            auto slider = Slider::create(this, menu_selector(FreeHackPopup::onSpeed), 0.7f);
            slider->setPosition({size.width / 2, y - 30.f});
            slider->setValue((speed - 0.1f) / 1.9f);
            m_content->addChild(slider);

            auto note = CCLabelBMFont::create(
                "Cheats on = completions won't save", "chatFont.fnt"
            );
            note->setScale(0.6f);
            note->setOpacity(160);
            note->setPosition({size.width / 2, 22.f});
            m_content->addChild(note);
        }
    }

    void onTab(CCObject* sender) {
        this->showTab(static_cast<CCNode*>(sender)->getTag());
    }

    void onToggle(CCObject* sender) {
        auto t = static_cast<CCMenuItemToggler*>(sender);
        // state flips after the callback, so the new value is the opposite
        fh::setOpt(t->getID(), !t->isToggled());
        fh::applySpeed();
    }

    void onSpeed(CCObject* sender) {
        float v = static_cast<SliderThumb*>(sender)->getValue();
        float speed = std::round((0.1f + v * 1.9f) * 20.f) / 20.f; // snap to 0.05
        Mod::get()->setSavedValue<float>("speed", speed);
        if (m_speedLabel) {
            m_speedLabel->setString(fmt::format("Speed: {:.2f}x", speed).c_str());
        }
        fh::applySpeed();
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

// ---------- button on the main menu ----------
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

// ---------- button in the pause menu ----------
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

// ---------- gameplay hooks ----------
class $modify(FHPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        fh::applySpeed();
        return true;
    }

    // safe mode: with cheats on, treat the run as a test run so it never saves
    void levelComplete() {
        if (fh::cheating()) m_isTestMode = true;
        PlayLayer::levelComplete();
    }

    void onQuit() {
        CCDirector::get()->getScheduler()->setTimeScale(1.f);
        PlayLayer::onQuit();
    }
};

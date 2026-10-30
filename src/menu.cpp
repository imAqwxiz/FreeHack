// FreeHack menu: sidebar tabs + scrolling module list + per-module settings.
#include "FreeHack.hpp"
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/ScrollLayer.hpp>

namespace {
    constexpr float kRowH = 46.f;
    constexpr float kSettingsW = 420.f;

    // label that shrinks instead of overflowing its box
    CCLabelBMFont* makeLabel(std::string const& text, char const* font, float scale, float maxW) {
        auto l = CCLabelBMFont::create(text.c_str(), font);
        l->limitLabelWidth(maxW, scale, 0.1f);
        l->setAnchorPoint({0.f, 0.5f});
        return l;
    }

    // scroll area with a bottom-left anchor so positions are predictable
    ScrollLayer* makeScroll(float x, float y, float w, float h) {
        auto s = ScrollLayer::create({w, h});
        s->ignoreAnchorPointForPosition(false);
        s->setAnchorPoint({0.f, 0.f});
        s->setPosition({x, y});
        return s;
    }

    float ctlHeight(fh::Ctl const& c) {
        switch (c.type) {
            case fh::CtlType::Header: return 24.f;
            case fh::CtlType::Toggle: return 28.f;
            case fh::CtlType::Color:  return 90.f; // three sliders
            default:                  return 30.f; // slider / presets
        }
    }
}

// =====================================================================
//  Settings popup for one module
// =====================================================================
class FHSettingsPopup : public Popup {
protected:
    struct Row {
        std::string id;
        std::string name;
        float min = 0.f;
        float max = 1.f;
        float labelW = 150.f;
        bool isInt = false;
        CCLabelBMFont* node = nullptr;
        Slider* slider = nullptr;

        std::string text(float v) const {
            if (isInt) return fmt::format("{}: {}", name, static_cast<int>(std::round(v)));
            return fmt::format("{}: {:.2f}", name, v);
        }
    };
    struct Preset { int row; float value; };

    fh::Opt const* m_opt = nullptr;
    std::vector<Row> m_rows;
    std::vector<Preset> m_presets;
    std::string m_colorBase;
    CCLayerColor* m_swatch = nullptr;
    ScrollLayer* m_scroll = nullptr;
    float m_w = kSettingsW;
    float m_viewH = 0.f;

    bool init(fh::Opt const& opt) {
        m_opt = &opt;

        float total = 12.f;
        for (auto const& c : opt.ctls) total += ctlHeight(c);

        auto win = CCDirector::get()->getWinSize();
        m_w = std::min(win.width - 40.f, kSettingsW);
        m_viewH = std::min(total, win.height - 20.f - 100.f);

        if (!Popup::init(m_w, m_viewH + 100.f)) return false;
        this->setTitle(fmt::format("{} Settings", opt.label));

        this->buildList();

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(menu);

        auto spr = ButtonSprite::create("Reset", 70, true, "bigFont.fnt", "GJ_button_06.png", 26.f, 0.5f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(FHSettingsPopup::onResetAll));
        btn->setPosition({m_w / 2.f, 24.f});
        menu->addChild(btn);
        return true;
    }

    void buildList() {
        if (m_scroll) m_scroll->removeFromParent();
        m_rows.clear();
        m_presets.clear();
        m_swatch = nullptr;

        float listW = m_w - 20.f;
        float total = 12.f;
        for (auto const& c : m_opt->ctls) total += ctlHeight(c);
        float listH = std::max(total, m_viewH);

        m_scroll = makeScroll(10.f, 48.f, listW, m_viewH);
        m_mainLayer->addChild(m_scroll);

        auto list = m_scroll->m_contentLayer;
        list->setContentSize({listW, listH});

        auto menu = CCMenu::create();
        menu->setContentSize({listW, listH});
        menu->setPosition({0.f, 0.f});
        list->addChild(menu);

        float top = listH - 6.f;
        for (auto const& c : m_opt->ctls) {
            float h = ctlHeight(c);
            float cy = top - h / 2.f;

            switch (c.type) {
                case fh::CtlType::Header: {
                    auto lbl = CCLabelBMFont::create(c.label.c_str(), "goldFont.fnt");
                    lbl->setScale(0.5f);
                    lbl->setAnchorPoint({0.f, 0.5f});
                    lbl->setPosition({8.f, cy});
                    list->addChild(lbl);
                    break;
                }
                case fh::CtlType::Toggle: {
                    auto t = CCMenuItemToggler::createWithStandardSprites(
                        this, menu_selector(FHSettingsPopup::onToggle), 0.5f
                    );
                    t->setID(c.id);
                    t->toggle(fh::on(c.id));
                    t->setPosition({22.f, cy});
                    menu->addChild(t);

                    auto lbl = makeLabel(c.label, "bigFont.fnt", 0.4f, listW - 80.f);
                    lbl->setPosition({44.f, cy});
                    list->addChild(lbl);
                    break;
                }
                case fh::CtlType::Slider:
                    this->addSlider(list, menu, c.id, c.label, c.min, c.max, c.isInt, cy);
                    break;
                case fh::CtlType::Color: {
                    m_colorBase = c.id;
                    this->addSlider(list, menu, c.id + "R", c.label + " R", 0.f, 255.f, true, cy + 30.f);
                    this->addSlider(list, menu, c.id + "G", c.label + " G", 0.f, 255.f, true, cy);
                    this->addSlider(list, menu, c.id + "B", c.label + " B", 0.f, 255.f, true, cy - 30.f);

                    m_swatch = CCLayerColor::create({255, 255, 255, 255}, 22.f, 22.f);
                    m_swatch->setPosition({listW - 33.f, cy - 11.f});
                    list->addChild(m_swatch);
                    this->updateSwatch();
                    break;
                }
                case fh::CtlType::Presets: {
                    int row = -1;
                    for (size_t i = 0; i < m_rows.size(); i++) {
                        if (m_rows[i].id == c.id) row = static_cast<int>(i);
                    }
                    if (row < 0) break;

                    for (size_t i = 0; i < c.presets.size(); i++) {
                        auto spr = ButtonSprite::create(
                            fmt::format("{}", c.presets[i]).c_str(), 44, true,
                            "bigFont.fnt", "GJ_button_04.png", 22.f, 0.4f
                        );
                        auto btn = CCMenuItemSpriteExtra::create(
                            spr, this, menu_selector(FHSettingsPopup::onPreset)
                        );
                        btn->setTag(static_cast<int>(m_presets.size()));
                        btn->setPosition({34.f + static_cast<float>(i) * 54.f, cy});
                        menu->addChild(btn);
                        m_presets.push_back({row, c.presets[i]});
                    }
                    break;
                }
            }
            top -= h;
        }

        m_scroll->moveToTop();
    }

    // label + slider + two step arrows
    void addSlider(CCNode* list, CCMenu* menu, std::string const& id, std::string const& name,
                   float min, float max, bool isInt, float cy) {
        float listW = list->getContentSize().width;
        int idx = static_cast<int>(m_rows.size());

        Row r;
        r.id = id; r.name = name; r.min = min; r.max = max; r.isInt = isInt;
        r.labelW = 185.f;
        float val = fh::num(id);

        r.node = makeLabel(r.text(val), "bigFont.fnt", 0.38f, r.labelW);
        r.node->setPosition({8.f, cy});
        list->addChild(r.node);

        r.slider = Slider::create(this, menu_selector(FHSettingsPopup::onSlider), 0.55f);
        r.slider->setPosition({listW - 150.f, cy});
        r.slider->setValue((val - min) / (max - min));
        r.slider->getThumb()->setTag(idx);
        list->addChild(r.slider);

        for (int dir = 0; dir < 2; dir++) {
            auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
            spr->setScale(0.4f);
            spr->setFlipX(dir == 1);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(FHSettingsPopup::onStep));
            btn->setTag(idx * 2 + dir);
            btn->setPosition({listW - 66.f + static_cast<float>(dir) * 22.f, cy});
            menu->addChild(btn);
        }

        m_rows.push_back(r);
    }

    void updateSwatch() {
        if (!m_swatch) return;
        auto u8 = [](float v) { return static_cast<GLubyte>(std::clamp(v, 0.f, 255.f)); };
        m_swatch->setColor({
            u8(fh::num(m_colorBase + "R")),
            u8(fh::num(m_colorBase + "G")),
            u8(fh::num(m_colorBase + "B"))
        });
    }

    void commit(int idx, float v, bool moveSlider) {
        if (idx < 0 || idx >= static_cast<int>(m_rows.size())) return;
        auto& r = m_rows[idx];

        v = std::clamp(v, r.min, r.max);
        v = r.isInt ? std::round(v) : std::round(v * 20.f) / 20.f; // snap to 0.05
        fh::setNum(r.id, v);

        r.node->setString(r.text(v).c_str());
        r.node->limitLabelWidth(r.labelW, 0.38f, 0.1f);
        if (moveSlider && r.slider) r.slider->setValue((v - r.min) / (r.max - r.min));

        this->updateSwatch();
        fh::changed();
    }

    void onToggle(CCObject* sender) {
        auto t = static_cast<CCMenuItemToggler*>(sender);
        // the state flips after the callback, so the new value is the opposite
        fh::setOn(t->getID(), !t->isToggled());
        fh::changed();
    }

    void onSlider(CCObject* sender) {
        auto thumb = static_cast<SliderThumb*>(sender);
        int idx = thumb->getTag();
        if (idx < 0 || idx >= static_cast<int>(m_rows.size())) return;
        auto const& r = m_rows[idx];
        this->commit(idx, r.min + thumb->getValue() * (r.max - r.min), false);
    }

    void onStep(CCObject* sender) {
        int tag = static_cast<CCNode*>(sender)->getTag();
        int idx = tag / 2;
        if (idx < 0 || idx >= static_cast<int>(m_rows.size())) return;
        auto const& r = m_rows[idx];
        float step = r.isInt ? 1.f : 0.05f;
        this->commit(idx, fh::num(r.id) + (tag % 2 == 1 ? step : -step), true);
    }

    void onPreset(CCObject* sender) {
        int i = static_cast<CCNode*>(sender)->getTag();
        if (i < 0 || i >= static_cast<int>(m_presets.size())) return;
        this->commit(m_presets[i].row, m_presets[i].value, true);
    }

    // put every setting of this module back to its default
    void onResetAll(CCObject*) {
        for (auto const& c : m_opt->ctls) {
            switch (c.type) {
                case fh::CtlType::Toggle: fh::setOn(c.id, c.def > 0.5f); break;
                case fh::CtlType::Slider: fh::setNum(c.id, c.def); break;
                case fh::CtlType::Color:
                    fh::setNum(c.id + "R", c.col[0]);
                    fh::setNum(c.id + "G", c.col[1]);
                    fh::setNum(c.id + "B", c.col[2]);
                    break;
                default: break;
            }
        }
        fh::changed();
        this->buildList();
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
//  Main popup: sidebar tabs on the left, scrolling module list on the right
// =====================================================================
class FreeHackPopup : public Popup {
protected:
    static inline int s_lastTab = 0; // reopen on the tab you used last

    float m_w = 0.f;
    float m_h = 0.f;
    int m_tab = 0;
    CCNode* m_content = nullptr;
    std::vector<CCMenuItemSpriteExtra*> m_tabBtns;

    bool init() {
        auto win = CCDirector::get()->getWinSize();
        m_w = std::min(win.width - 40.f, 460.f);
        m_h = std::min(win.height - 20.f, 290.f);

        if (!Popup::init(m_w, m_h)) return false;
        this->setTitle("FreeHack");

        auto panel = CCScale9Sprite::create("square02b_001.png", {0.f, 0.f, 80.f, 80.f});
        panel->setContentSize({m_w - 126.f, m_h - 74.f});
        panel->setAnchorPoint({0.f, 0.f});
        panel->setPosition({112.f, 14.f});
        panel->setColor({0, 0, 0});
        panel->setOpacity(90);
        m_mainLayer->addChild(panel);

        auto tabMenu = CCMenu::create();
        tabMenu->setPosition({0.f, 0.f});
        m_mainLayer->addChild(tabMenu);

        auto const& tabs = fh::tabs();
        for (size_t i = 0; i < tabs.size(); i++) {
            auto spr = ButtonSprite::create(
                tabs[i].name.c_str(), 84, true, "bigFont.fnt", "GJ_button_04.png", 28.f, 0.6f
            );
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(FreeHackPopup::onTab));
            btn->setTag(static_cast<int>(i));
            btn->setPosition({62.f, m_h - 62.f - static_cast<float>(i) * 36.f});
            tabMenu->addChild(btn);
            m_tabBtns.push_back(btn);
        }

        this->showTab(std::min(s_lastTab, static_cast<int>(tabs.size()) - 1));
        return true;
    }

    void showTab(int i) {
        s_lastTab = i;
        m_tab = i;

        if (m_content) m_content->removeFromParent();
        m_content = CCNode::create();
        m_mainLayer->addChild(m_content);

        for (size_t j = 0; j < m_tabBtns.size(); j++) {
            auto spr = static_cast<ButtonSprite*>(m_tabBtns[j]->getNormalImage());
            spr->updateBGImage(static_cast<int>(j) == i ? "GJ_button_01.png" : "GJ_button_04.png");
        }

        auto const& tab = fh::tabs()[i];
        float areaW = m_w - 126.f;
        float viewW = areaW - 8.f;
        float viewH = m_h - 74.f - 8.f;

        if (tab.opts.empty()) {
            auto lbl = CCLabelBMFont::create("Coming soon", "bigFont.fnt");
            lbl->setScale(0.6f);
            lbl->setOpacity(150);
            lbl->setPosition({112.f + areaW / 2.f, m_h / 2.f - 10.f});
            m_content->addChild(lbl);
            return;
        }

        float listH = std::max(viewH, static_cast<float>(tab.opts.size()) * kRowH + 8.f);

        auto scroll = makeScroll(112.f + 4.f, 14.f + 4.f, viewW, viewH);
        m_content->addChild(scroll);

        auto list = scroll->m_contentLayer;
        list->setContentSize({viewW, listH});

        auto menu = CCMenu::create();
        menu->setContentSize({viewW, listH});
        menu->setPosition({0.f, 0.f});
        list->addChild(menu);

        for (size_t j = 0; j < tab.opts.size(); j++) {
            auto const& o = tab.opts[j];
            float cy = listH - 4.f - kRowH * (static_cast<float>(j) + 0.5f);

            auto toggler = CCMenuItemToggler::createWithStandardSprites(
                this, menu_selector(FreeHackPopup::onToggle), 0.65f
            );
            toggler->setID(o.id);
            toggler->toggle(fh::on(o.id));
            toggler->setPosition({24.f, cy});
            menu->addChild(toggler);

            float textW = viewW - (o.ctls.empty() ? 70.f : 130.f);

            auto name = makeLabel(o.label, "bigFont.fnt", 0.5f, textW);
            name->setPosition({46.f, cy + 8.f});
            list->addChild(name);

            auto desc = makeLabel(o.desc, "chatFont.fnt", 0.5f, textW);
            desc->setOpacity(150);
            desc->setPosition({46.f, cy - 9.f});
            list->addChild(desc);

            if (!o.ctls.empty()) {
                auto spr = ButtonSprite::create(
                    "Settings", 72, true, "bigFont.fnt", "GJ_button_04.png", 24.f, 0.42f
                );
                auto btn = CCMenuItemSpriteExtra::create(
                    spr, this, menu_selector(FreeHackPopup::onSettings)
                );
                btn->setTag(static_cast<int>(j));
                btn->setPosition({viewW - 46.f, cy});
                menu->addChild(btn);
            }
        }

        scroll->moveToTop();
    }

    void onTab(CCObject* sender) {
        this->showTab(static_cast<CCNode*>(sender)->getTag());
    }

    void onToggle(CCObject* sender) {
        auto t = static_cast<CCMenuItemToggler*>(sender);
        fh::setOn(t->getID(), !t->isToggled());
        fh::changed();
    }

    void onSettings(CCObject* sender) {
        int j = static_cast<CCNode*>(sender)->getTag();
        auto const& opt = fh::tabs()[m_tab].opts[j];
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
//  FH button on the main menu and the pause menu
// =====================================================================
class $modify(FHMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto spr = ButtonSprite::create("FH");
        spr->setScale(0.8f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(FHMenuLayer::onFreeHack));
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
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(FHPauseLayer::onFreeHack));
        btn->setID("freehack-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();
    }

    void onFreeHack(CCObject*) {
        FreeHackPopup::create()->show();
    }
};

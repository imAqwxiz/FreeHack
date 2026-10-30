// Player tab: Noclip, Speedhack, Speedhack Audio, Auto Clicker
#include "FreeHack.hpp"
#include <Geode/modify/CCScheduler.hpp>

namespace fh::player {
    namespace {
        // noclip accuracy
        int g_total = 0;
        int g_dead = 0;
        bool g_diedNow = false;

        // death tint
        float g_tintAlpha = 0.f;
        CCLayerColor* g_tint = nullptr;
        CCLabelBMFont* g_accLabel = nullptr;

        // auto clicker
        float g_clickTimer = 0.f;
        bool g_clickDown = false;

        GLubyte u8(float v) {
            return static_cast<GLubyte>(std::clamp(v, 0.f, 255.f));
        }
    }

    // ------------------------------------------------------------
    //  Menu entries
    // ------------------------------------------------------------
    void init() {
        addOpt("Player", Opt{
            "noclip", "Noclip", "Survive spikes and walls.", false, true, {
                header("Players"),
                toggle("noclipP1", "Player 1", true),
                toggle("noclipP2", "Player 2", true),
                header("On death"),
                toggle("noclipTint", "Tint screen on death", true),
                color("tint", "Tint color", 255.f, 0.f, 0.f),
                slider("tintOpacity", "Tint opacity", 0.f, 255.f, 110.f, true),
                header("Accuracy"),
                toggle("noclipAcc", "Show accuracy", true),
                slider("noclipMinAcc", "Die below accuracy % (0 = off)", 0.f, 100.f, 0.f, true),
            }
        });

        addOpt("Player", Opt{
            "speedhack", "Speedhack", "Change the game speed in levels.", false, true, {
                slider("speed", "Speed", 0.1f, 3.f, 0.5f),
                presets("speed", {0.25f, 0.5f, 0.75f, 1.5f, 2.f}),
            }
        });

        addOpt("Player", Opt{
            "speedAudio", "Speedhack Audio", "Music and sounds follow the speed.", true, false, {}
        });

        addOpt("Player", Opt{
            "autoclick", "Auto Clicker", "Clicks for you while you play.", false, true, {
                header("Timing"),
                slider("cps", "Clicks per second", 1.f, 30.f, 10.f, true),
                slider("clickHold", "Hold time %", 10.f, 90.f, 50.f, true),
                header("Players"),
                toggle("clickP1", "Player 1", true),
                toggle("clickP2", "Player 2", false),
            }
        });
    }

    // ------------------------------------------------------------
    //  Speedhack
    // ------------------------------------------------------------
    float speedMul() {
        return (PlayLayer::get() && on("speedhack")) ? num("speed") : 1.f;
    }

    static void setPitch(float pitch) {
        auto engine = FMODAudioEngine::get();
        if (!engine || !engine->m_system) return;
        FMOD::ChannelGroup* group = nullptr;
        engine->m_system->getMasterChannelGroup(&group);
        if (group) group->setPitch(pitch);
    }

    void applyAudio() {
        bool active = PlayLayer::get() && on("speedhack") && on("speedAudio");
        setPitch(active ? num("speed") : 1.f);
    }

    // ------------------------------------------------------------
    //  Level lifecycle (called from PlayHooks.cpp)
    // ------------------------------------------------------------
    void onPlayInit(PlayLayer* pl) {
        auto win = CCDirector::get()->getWinSize();

        g_tint = CCLayerColor::create({255, 0, 0, 0}, win.width, win.height);
        g_tint->setZOrder(1000);
        pl->addChild(g_tint);

        g_accLabel = CCLabelBMFont::create("", "bigFont.fnt");
        g_accLabel->setScale(0.4f);
        g_accLabel->setOpacity(170);
        g_accLabel->setAnchorPoint({0.f, 1.f});
        g_accLabel->setPosition({8.f, win.height - 8.f});
        g_accLabel->setZOrder(1001);
        g_accLabel->setVisible(false);
        pl->addChild(g_accLabel);

        g_total = 0;
        g_dead = 0;
        g_diedNow = false;
        g_tintAlpha = 0.f;
        g_clickTimer = 0.f;
        g_clickDown = false;

        applyAudio();
    }

    void onReset(PlayLayer*) {
        g_total = 0;
        g_dead = 0;
        g_diedNow = false;
        g_tintAlpha = 0.f;
        g_clickTimer = 0.f;
    }

    void onQuit() {
        setPitch(1.f);
        g_tint = nullptr;
        g_accLabel = nullptr;
        g_clickDown = false;
    }

    // ------------------------------------------------------------
    //  Noclip
    // ------------------------------------------------------------
    bool blockDeath(PlayLayer* pl, PlayerObject* player, GameObject* hazard) {
        // anti-cheat spike always kills; everything else is ignored while noclip is on
        if (!on("noclip") || hazard == pl->m_anticheatSpike) return false;

        bool isP2 = player == pl->m_player2;
        if (isP2 ? !on("noclipP2") : !on("noclipP1")) return false;

        // accuracy floor: die for real if accuracy would drop under the minimum
        float minAcc = num("noclipMinAcc");
        if (minAcc > 0.f && g_total > 0) {
            float acc = 100.f * (1.f - static_cast<float>(g_dead + 1) / static_cast<float>(g_total + 1));
            if (acc < minAcc) return false;
        }

        g_diedNow = true;
        if (on("noclipTint")) g_tintAlpha = 1.f;
        return true;
    }

    // ------------------------------------------------------------
    //  Every frame: accuracy, tint fade, auto clicker
    // ------------------------------------------------------------
    void onUpdate(PlayLayer* pl, float dt) {
        // ---- accuracy ----
        bool noclip = on("noclip");
        if (noclip) {
            g_total++;
            if (g_diedNow) g_dead++;
        }
        g_diedNow = false;

        if (g_accLabel) {
            if (noclip && on("noclipAcc") && g_total > 0) {
                float acc = 100.f * (1.f - static_cast<float>(g_dead) / static_cast<float>(g_total));
                g_accLabel->setString(fmt::format("Accuracy: {:.2f}%", acc).c_str());
                g_accLabel->setVisible(true);
            } else {
                g_accLabel->setVisible(false);
            }
        }

        // ---- death tint (fades out) ----
        if (g_tint) {
            if (g_tintAlpha > 0.f) {
                g_tintAlpha = std::max(0.f, g_tintAlpha - dt * 4.f);
                g_tint->setColor({u8(num("tintR")), u8(num("tintG")), u8(num("tintB"))});
            }
            g_tint->setOpacity(u8(g_tintAlpha * num("tintOpacity")));
        }

        // ---- auto clicker ----
        if (on("autoclick") && pl->m_player1 && !pl->m_player1->m_isDead) {
            float period = 1.f / std::max(1.f, num("cps"));
            float hold = period * std::clamp(num("clickHold") / 100.f, 0.05f, 0.95f);
            g_clickTimer += dt;
            bool down = std::fmod(g_clickTimer, period) < hold;
            if (down != g_clickDown) {
                g_clickDown = down;
                if (on("clickP1")) pl->handleButton(down, 1, true);
                if (on("clickP2")) pl->handleButton(down, 1, false);
            }
        } else if (g_clickDown) {
            g_clickDown = false;
            pl->handleButton(false, 1, true);
            pl->handleButton(false, 1, false);
        }
    }
}

// ----------------------------------------------------------------
//  Speedhack: scale the scheduler's dt. GD resets CCScheduler's time
//  scale on every respawn, so setTimeScale never stuck.
// ----------------------------------------------------------------
class $modify(FHScheduler, CCScheduler) {
    void update(float dt) {
        CCScheduler::update(dt * fh::player::speedMul());
    }
};

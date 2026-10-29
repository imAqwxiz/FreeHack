
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include <algorithm>
#include <cstdint>

using namespace geode::prelude;

namespace {
    bool g_holding = false;
    bool g_syntheticInput = false;
    bool g_spamButtonDown = true;

    float g_holdTime = 0.0f;
    float g_tickTimer = 0.0f;

    int g_button = 0;
    bool g_player1 = true;
}

class $modify(HoldToSpamGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        // Let synthetic input pass through without resetting the hold timer.
        if (g_syntheticInput) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        if (down) {
            if (!g_holding) {
                g_holding = true;
                g_holdTime = 0.0f;
                g_tickTimer = 0.0f;

                g_button = button;
                g_player1 = isPlayer1;
                g_spamButtonDown = true;
            }
        } else if (
            g_holding &&
            button == g_button &&
            isPlayer1 == g_player1
        ) {
            // Releasing the original held input resets the activation timer.
            g_holding = false;
            g_holdTime = 0.0f;
            g_tickTimer = 0.0f;
            g_spamButtonDown = true;
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void update(float dt) {
        GJBaseGameLayer::update(dt);

        if (!g_holding) {
            return;
        }

        g_holdTime += dt;

        const float activationDelay = std::clamp(
            static_cast<float>(
                Mod::get()->getSettingValue<double>("hold-duration")
            ),
            1.0f,
            60.0f
        );

        if (g_holdTime < activationDelay) {
            return;
        }

        const int cps = std::clamp(
            static_cast<int>(
                Mod::get()->getSettingValue<int64_t>("cps")
            ),
            1,
            60
        );

        // Each complete cycle consists of a press and a release.
        const float halfInterval = 0.5f / static_cast<float>(cps);

        g_tickTimer += dt;

        if (g_tickTimer < halfInterval) {
            return;
        }

        // Preserve excess time to reduce timing drift on slower frames.
        g_tickTimer -= halfInterval;

        g_spamButtonDown = !g_spamButtonDown;

        g_syntheticInput = true;
        GJBaseGameLayer::handleButton(
            g_spamButtonDown,
            g_button,
            g_player1
        );
        g_syntheticInput = false;
    }
};


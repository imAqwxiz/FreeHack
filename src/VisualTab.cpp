// Visual tab: Wave Trail Size
#include "FreeHack.hpp"
#include <Geode/modify/HardStreak.hpp>

namespace fh::visual {
    void init() {
        addOpt("Visual", Opt{
            "waveSize", "Wave Trail Size", "Make the wave trail thicker or thinner.", false, false, {
                slider("waveSizeVal", "Size", 0.1f, 3.f, 1.f),
                presets("waveSizeVal", {0.5f, 1.f, 1.5f, 2.f}),
            }
        });
    }
}

class $modify(FHHardStreak, HardStreak) {
    void updateStroke(float dt) {
        if (fh::on("waveSize")) m_pulseSize = fh::num("waveSizeVal");
        HardStreak::updateStroke(dt);
    }
};

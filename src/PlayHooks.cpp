// All PlayLayer hooks live here, once, and forward to the tab files.
#include "FreeHack.hpp"
#include <Geode/modify/PlayLayer.hpp>

class $modify(FHPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        fh::player::onPlayInit(this);
        fh::level::onPlayInit(this);
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        fh::player::onReset(this);
        fh::level::onReset(this);
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        if (fh::player::blockDeath(this, player, object)) return;
        PlayLayer::destroyPlayer(player, object);
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        fh::player::onUpdate(this, dt);
        fh::level::onUpdate(this, dt);
    }

    void levelComplete() {
        fh::level::beforeComplete(this);
        PlayLayer::levelComplete();
    }

    void onQuit() {
        fh::player::onQuit();
        PlayLayer::onQuit();
    }
};

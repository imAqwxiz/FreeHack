// FreeHack entry point. Each tab registers its own modules.
#include "FreeHack.hpp"

$execute {
    fh::player::init();
    fh::level::init();
    fh::editor::init();
    fh::visual::init();
    fh::utility::init();
}

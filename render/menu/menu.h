#pragma once
#include "rbx.h"
#include <vector>

namespace menu {
    void render(const std::vector<roblox::player_t>& players);
    inline void draw(const std::vector<roblox::player_t>& players) { render(players); }
}

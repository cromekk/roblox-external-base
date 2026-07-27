#pragma once
#include "rbx.h"
#include <vector>

namespace esp {
    inline bool  enabled             = true;
    inline bool  box_enabled         = true;
    inline bool  ignore_local_player = false;
    inline float max_distance        = 1500.f;

    void render(const std::vector<roblox::player_t>& players);
}

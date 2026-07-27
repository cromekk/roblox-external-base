#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "vec.h"

namespace roblox {

    inline uintptr_t data_model            = 0;
    inline uintptr_t local_player_address  = 0;
    inline uintptr_t workspace_address     = 0;
    inline uintptr_t camera_address        = 0;
    inline uintptr_t visual_engine_address = 0;

    struct player_t {
        uintptr_t   address;
        uintptr_t   character;
        uintptr_t   root_part;
        uintptr_t   humanoid;
        std::string name;
        std::string display_name;
        float       health;
        float       max_health;
        Vec3        position;
        bool        is_teammate;
    };

    bool update_cache();
    std::vector<player_t> get_players();

    std::string  get_instance_name(uintptr_t instance);
    std::string  get_class_name(uintptr_t instance);
    uintptr_t    find_child(uintptr_t instance, const char* name);
    uintptr_t    find_child_by_class(uintptr_t instance, const char* class_name);
    std::vector<uintptr_t> get_children(uintptr_t instance);

    Mat4  get_view_matrix();
    Vec2  get_viewport_size();
    bool  world_to_screen(const Vec3& world_pos, Vec2& screen_pos, const Mat4& view_matrix, float screen_width, float screen_height);
}

#include "esp.h"
#include "mem.h"
#include "offsets.h"
#include "overlay.h"
#include "imgui.h"
#include <cmath>

namespace esp {

static bool is_game_focused()
{
    HWND foreground_window = GetForegroundWindow();
    return foreground_window == overlay::target_window || foreground_window == overlay::window_handle;
}

void render(const std::vector<roblox::player_t>& players)
{
    if (!enabled) return;
    if (!roblox::visual_engine_address) return;
    if (!is_game_focused()) return;

    Mat4 view_matrix = roblox::get_view_matrix();
    ImGuiIO& io = ImGui::GetIO();
    float screen_width  = io.DisplaySize.x;
    float screen_height = io.DisplaySize.y;

    Vec3 camera_position{};
    if (roblox::camera_address && offsets::CameraPos) {
        camera_position = memory::read<Vec3>(roblox::camera_address + offsets::CameraPos);
    }

    auto* draw_list = ImGui::GetBackgroundDrawList();

    for (const auto& player : players) {
        if (!player.root_part) continue;
        if (player.health <= 0.f) continue;

        bool is_local = (player.address == roblox::local_player_address);
        if (is_local && !ignore_local_player) continue;

        Vec3 delta = player.position - camera_position;
        float distance = sqrtf(delta.dot(delta));
        if (distance > max_distance) continue;

        Vec3 head_pos = { player.position.x, player.position.y + 3.0f,  player.position.z };
        Vec3 feet_pos = { player.position.x, player.position.y - 3.5f, player.position.z };

        Vec2 screen_top, screen_bottom;
        if (!roblox::world_to_screen(head_pos, screen_top, view_matrix, screen_width, screen_height)) continue;
        if (!roblox::world_to_screen(feet_pos, screen_bottom, view_matrix, screen_width, screen_height)) continue;

        float box_height = screen_bottom.y - screen_top.y;
        if (box_height < 4.0f) continue;

        float box_width = box_height * 0.5f;
        float box_x = screen_top.x - box_width * 0.5f;
        float box_y = screen_top.y;

        if (box_enabled) {
            ImU32 box_color  = player.is_teammate ? IM_COL32(60, 220, 100, 255) : IM_COL32(242, 78, 107, 255);
            ImU32 shadow_col = IM_COL32(0, 0, 0, 180);

            draw_list->AddRect({ box_x - 1.0f, box_y - 1.0f }, { box_x + box_width + 1.0f, box_y + box_height + 1.0f }, shadow_col, 0.f, 0, 1.0f);
            draw_list->AddRect({ box_x + 1.0f, box_y + 1.0f }, { box_x + box_width - 1.0f, box_y + box_height - 1.0f }, shadow_col, 0.f, 0, 1.0f);
            draw_list->AddRect({ box_x, box_y }, { box_x + box_width, box_y + box_height }, box_color, 0.f, 0, 1.4f);
        }
    }
}

}

#include "menu.h"
#include "overlay.h"
#include "esp.h"
#include "rbx.h"
#include "imgui.h"

namespace menu {

void render(const std::vector<roblox::player_t>& players)
{
    if (!overlay::menu_visible) return;

    ImGui::SetNextWindowSize({ 300, 160 }, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("roblox-external-base", &overlay::menu_visible)) {
        ImGui::Checkbox("ESP Enabled", &esp::enabled);
        if (esp::enabled) {
            ImGui::Checkbox("Box", &esp::box_enabled);
            ImGui::Checkbox("Ignore Self", &esp::ignore_local_player);
            ImGui::SliderFloat("Max Distance", &esp::max_distance, 50.f, 2000.f, "%.0f");
        }
    }
    ImGui::End();
}

}

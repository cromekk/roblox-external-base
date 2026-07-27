#include "rbx.h"
#include "mem.h"
#include "offsets.h"
#include <unordered_map>

namespace roblox {

static std::unordered_map<uintptr_t, std::vector<uintptr_t>> children_cache;
static std::unordered_map<uintptr_t, std::string>            name_cache;
static std::unordered_map<uintptr_t, std::string>            class_name_cache;

static const std::vector<uintptr_t>& get_cached_children(uintptr_t instance)
{
    auto it = children_cache.find(instance);
    if (it != children_cache.end()) return it->second;
    return children_cache[instance] = get_children(instance);
}

static const std::string& get_cached_name(uintptr_t instance)
{
    auto it = name_cache.find(instance);
    if (it != name_cache.end()) return it->second;
    return name_cache[instance] = get_instance_name(instance);
}

static const std::string& get_cached_class_name(uintptr_t instance)
{
    auto it = class_name_cache.find(instance);
    if (it != class_name_cache.end()) return it->second;
    return class_name_cache[instance] = get_class_name(instance);
}

bool update_cache()
{
    children_cache.clear();
    name_cache.clear();
    class_name_cache.clear();

    if (!memory::base_address || !offsets::FakeDataModelPointer) return false;
    uintptr_t fake_data_model = memory::read<uintptr_t>(memory::base_address + offsets::FakeDataModelPointer);
    if (!fake_data_model) return false;

    data_model = memory::read<uintptr_t>(fake_data_model + offsets::FakeDataModelToDataModel);
    if (!data_model) return false;

    workspace_address = find_child_by_class(data_model, "Workspace");
    if (workspace_address) {
        camera_address = memory::read<uintptr_t>(workspace_address + offsets::Camera);
    }

    uintptr_t players_service = find_child_by_class(data_model, "Players");
    if (players_service) {
        local_player_address = memory::read<uintptr_t>(players_service + offsets::LocalPlayer);
    }

    if (offsets::VisualEnginePointer) {
        uintptr_t fresh_engine = memory::read<uintptr_t>(memory::base_address + offsets::VisualEnginePointer);
        if (fresh_engine) visual_engine_address = fresh_engine;
    }
    return true;
}

std::string get_instance_name(uintptr_t instance)
{
    if (!instance) return {};
    uintptr_t name_str = memory::read<uintptr_t>(instance + offsets::Name);
    if (!name_str) return {};
    return memory::read_roblox_string(name_str);
}

std::string get_class_name(uintptr_t instance)
{
    if (!instance) return {};
    uintptr_t descriptor = memory::read<uintptr_t>(instance + offsets::ClassDescriptor);
    if (!descriptor) return {};
    uintptr_t class_name_ptr = memory::read<uintptr_t>(descriptor + offsets::ClassDescriptorToClassName);
    if (!class_name_ptr) return {};
    return memory::read_string(class_name_ptr);
}

std::vector<uintptr_t> get_children(uintptr_t instance)
{
    std::vector<uintptr_t> result;
    if (!instance) return result;

    uintptr_t children_list = memory::read<uintptr_t>(instance + offsets::Children);
    if (!children_list) return result;

    uintptr_t start = memory::read<uintptr_t>(children_list);
    uintptr_t end   = memory::read<uintptr_t>(children_list + offsets::ChildrenEnd);
    if (!start || end <= start) return result;

    size_t span = end - start;
    if (span > 4096 * 16) return result;

    result.reserve(span / 16);
    for (uintptr_t current = start; current < end; current += 16) {
        uintptr_t child = memory::read<uintptr_t>(current);
        if (child) result.push_back(child);
    }
    return result;
}

uintptr_t find_child(uintptr_t instance, const char* name)
{
    for (auto child : get_cached_children(instance)) {
        if (get_cached_name(child) == name) return child;
    }
    return 0;
}

uintptr_t find_child_by_class(uintptr_t instance, const char* class_name)
{
    for (auto child : get_cached_children(instance)) {
        if (get_cached_class_name(child) == class_name) return child;
    }
    return 0;
}

std::vector<player_t> get_players()
{
    std::vector<player_t> result;
    uintptr_t players_service = find_child_by_class(data_model, "Players");
    if (!players_service) return result;

    int local_team = local_player_address ? memory::read<int>(local_player_address + offsets::TeamColor) : 0;

    for (auto player_instance : get_cached_children(players_service)) {
        if (get_cached_class_name(player_instance) != "Player") continue;

        player_t player{};
        player.address = player_instance;
        player.character = memory::read<uintptr_t>(player_instance + offsets::ModelInstance);
        player.name = get_cached_name(player_instance);
        player.is_teammate = (local_team != 0) && (memory::read<int>(player_instance + offsets::TeamColor) == local_team);

        if (player.character) {
            player.root_part = find_child(player.character, "HumanoidRootPart");
            player.humanoid  = find_child_by_class(player.character, "Humanoid");
            if (player.humanoid) {
                player.health     = memory::read<float>(player.humanoid + offsets::Health);
                player.max_health = memory::read<float>(player.humanoid + offsets::MaxHealth);
            }
            if (player.root_part) {
                uintptr_t primitive = memory::read<uintptr_t>(player.root_part + offsets::Primitive);
                if (primitive) {
                    player.position = memory::read<Vec3>(primitive + offsets::Position);
                }
            }
        }

        result.push_back(player);
    }
    return result;
}

Mat4 get_view_matrix()
{
    Mat4 matrix{};
    if (!visual_engine_address || !offsets::viewmatrix) return matrix;
    return memory::read<Mat4>(visual_engine_address + offsets::viewmatrix);
}

Vec2 get_viewport_size()
{
    if (!visual_engine_address || !offsets::ViewportSize) return { 1920.f, 1080.f };
    return memory::read<Vec2>(visual_engine_address + offsets::ViewportSize);
}

bool world_to_screen(const Vec3& world_pos, Vec2& screen_pos, const Mat4& view_matrix, float screen_width, float screen_height)
{
    float w_clip = view_matrix.m[12] * world_pos.x + view_matrix.m[13] * world_pos.y + view_matrix.m[14] * world_pos.z + view_matrix.m[15];
    if (w_clip <= 0.001f) return false;

    float tx = view_matrix.m[0] * world_pos.x + view_matrix.m[1] * world_pos.y + view_matrix.m[2] * world_pos.z + view_matrix.m[3];
    float ty = view_matrix.m[4] * world_pos.x + view_matrix.m[5] * world_pos.y + view_matrix.m[6] * world_pos.z + view_matrix.m[7];

    float inverse_w = 1.0f / w_clip;
    screen_pos.x = (screen_width * 0.5f) * (tx * inverse_w + 1.0f);
    screen_pos.y = (screen_height * 0.5f) * (1.0f - ty * inverse_w);
    return true;
}

}

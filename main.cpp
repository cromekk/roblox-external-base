#include <windows.h>
#include <chrono>
#include <thread>
#include <cstdio>

#include "mem.h"
#include "offsets.h"
#include "rbx.h"
#include "overlay.h"
#include "esp.h"
#include "menu.h"

static bool initialize_base()
{
    printf("[*] Waiting for RobloxPlayerBeta.exe...\n");
    while (!memory::attach_process(L"RobloxPlayerBeta.exe")) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    printf("[+] Attached to PID %lu at base address 0x%llX\n", memory::process_id, static_cast<unsigned long long>(memory::base_address));
    printf("[+] Using manual offset configuration\n");

    if (!overlay::initialize()) {
        printf("[-] Failed to initialize overlay window\n");
        return false;
    }
    printf("[+] Overlay initialized\n");
    return true;
}

int main()
{
    SetConsoleTitleW(L"roblox-external-base");

    if (!initialize_base()) {
        std::this_thread::sleep_for(std::chrono::seconds(3));
        return 1;
    }

    bool insert_key_was_pressed = false;

    while (overlay::is_running()) {
        bool insert_key_down = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insert_key_down && !insert_key_was_pressed) {
            overlay::menu_visible = !overlay::menu_visible;
        }
        insert_key_was_pressed = insert_key_down;

        roblox::update_cache();
        std::vector<roblox::player_t> players = roblox::get_players();

        overlay::begin_frame();
        esp::render(players);
        menu::render(players);
        overlay::end_frame();
    }

    overlay::shutdown();
    memory::detach_process();
    return 0;
}

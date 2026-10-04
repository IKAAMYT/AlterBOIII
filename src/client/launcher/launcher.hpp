#pragma once

#include <std_include.hpp>

namespace launcher {
bool run();
bool is_game_process_running();
const std::filesystem::path &get_launcher_ui_file();
void ensure_launcher_ui();
void check_self_update();
} // namespace launcher

#pragma once

namespace launcher {
bool run();
bool is_game_process_running();
std::filesystem::path get_launcher_ui_file();
void ensure_launcher_ui();
void cleanup_old_launcher();
void check_launcher_update();
void check_self_update();
} // namespace launcher

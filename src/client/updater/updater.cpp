#include <std_include.hpp>

#include "updater.hpp"
#include "updater_ui.hpp"
#include "file_updater.hpp"

namespace updater {
void run(const std::filesystem::path &base) {
  // AlterBO3 (IKAAM): auto-updater disabled.
  // The upstream server (r2.ezz.lol) would otherwise replace our custom exe
  // and launcher UI on every launch. We skip the update step entirely so the
  // AlterBO3 build stays intact. NOTE: this means no automatic updates — if a
  // Steam BO3 patch breaks compatibility, a new build must be shipped manually.
  (void)base;

  // --- original update routine, intentionally disabled ---
  // const auto self = utils::nt::library::get_by_address(run);
  // const auto self_file = self.get_path();
  //
  // updater_ui updater_ui{};
  // const file_updater file_updater{updater_ui, base, self_file};
  //
  // file_updater.run();
}
} // namespace updater

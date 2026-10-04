#include <std_include.hpp>

#include "bundled_ui_scripts.hpp"

#include <game/game.hpp>
#include <utils/nt.hpp>

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

namespace launcher {
namespace {
namespace fs = std::filesystem;

// Cle de comparaison d'un chemin relatif. Windows ne distingue pas la casse :
// sans cette normalisation, un fichier deja present sous une autre casse
// serait copie puis aussitot pris pour un fichier en trop et supprime.
std::wstring path_key(const fs::path &relative) {
  std::wstring key = relative.generic_wstring();
  std::transform(key.begin(), key.end(), key.begin(), [](const wchar_t c) {
    return static_cast<wchar_t>(std::towlower(static_cast<wint_t>(c)));
  });
  return key;
}

// Fichiers reguliers sous `root`, en chemins relatifs a `root`.
bool list_relative_files(const fs::path &root, std::vector<fs::path> &files) {
  std::error_code ec;
  fs::recursive_directory_iterator it(
      root, fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    return false;
  }

  const fs::recursive_directory_iterator end;
  while (it != end) {
    std::error_code entry_ec;
    if (it->is_regular_file(entry_ec) && !entry_ec) {
      fs::path relative = it->path().lexically_relative(root);
      if (!relative.empty()) {
        files.emplace_back(std::move(relative));
      }
    }

    it.increment(ec);
    if (ec) {
      return false;
    }
  }

  return true;
}

// Sous-dossiers de `root`, les plus profonds d'abord.
std::vector<fs::path> list_directories(const fs::path &root) {
  std::vector<fs::path> directories;

  std::error_code ec;
  fs::recursive_directory_iterator it(
      root, fs::directory_options::skip_permission_denied, ec);
  const fs::recursive_directory_iterator end;
  while (!ec && it != end) {
    std::error_code entry_ec;
    if (it->is_directory(entry_ec) && !entry_ec) {
      directories.emplace_back(it->path());
    }
    it.increment(ec);
  }

  std::sort(directories.begin(), directories.end(),
            [](const fs::path &a, const fs::path &b) {
              return a.native().size() > b.native().size();
            });
  return directories;
}

bool read_whole_file(const fs::path &file, std::string &data) {
  std::ifstream stream(file, std::ios::binary);
  if (!stream.is_open()) {
    return false;
  }

  data.assign(std::istreambuf_iterator<char>(stream),
              std::istreambuf_iterator<char>());
  return !stream.bad();
}

bool same_content(const fs::path &a, const fs::path &b) {
  std::error_code ec;
  const std::uintmax_t size_a = fs::file_size(a, ec);
  if (ec) {
    return false;
  }
  const std::uintmax_t size_b = fs::file_size(b, ec);
  if (ec || size_a != size_b) {
    return false;
  }

  std::string data_a;
  std::string data_b;
  return read_whole_file(a, data_a) && read_whole_file(b, data_b) &&
         data_a == data_b;
}
} // namespace

void mirror_ui_scripts(const std::filesystem::path &source,
                       const std::filesystem::path &target) noexcept {
  try {
    std::error_code ec;
    if (!fs::is_directory(source, ec) || ec) {
      return;
    }

    // Source et destination identiques : rien a faire, et surtout rien a
    // supprimer.
    if (fs::exists(target, ec) && !ec && fs::equivalent(source, target, ec) &&
        !ec) {
      return;
    }
    ec.clear();

    std::vector<fs::path> wanted;
    if (!list_relative_files(source, wanted)) {
      return;
    }

    // Garde-fou : un dossier source sans aucun __init__.lua n'est pas un vrai
    // dossier de scripts. On ne touche alors a rien, pour ne jamais vider la
    // destination a cause d'une archive mal decompressee.
    const bool has_script =
        std::any_of(wanted.begin(), wanted.end(), [](const fs::path &file) {
          return file.filename() == fs::path("__init__.lua");
        });
    if (!has_script) {
      return;
    }

    bool all_copied = true;
    for (const fs::path &relative : wanted) {
      const fs::path from = source / relative;
      const fs::path to = target / relative;
      if (same_content(from, to)) {
        continue;
      }

      fs::create_directories(to.parent_path(), ec);
      ec.clear();
      fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
      if (ec) {
        all_copied = false;
        ec.clear();
      }
    }

    // Si une copie a echoue (fichier verrouille, disque plein...), on garde
    // les anciens scripts tels quels plutot que de laisser un melange.
    if (!all_copied) {
      return;
    }

    std::vector<std::wstring> wanted_keys;
    wanted_keys.reserve(wanted.size());
    for (const fs::path &relative : wanted) {
      wanted_keys.emplace_back(path_key(relative));
    }
    std::sort(wanted_keys.begin(), wanted_keys.end());

    // Retire les scripts qui n'existent plus : un script renomme ou supprime
    // cote depot resterait sinon charge en double avec son remplacant.
    std::vector<fs::path> present;
    if (!list_relative_files(target, present)) {
      return;
    }
    for (const fs::path &relative : present) {
      if (!std::binary_search(wanted_keys.begin(), wanted_keys.end(),
                              path_key(relative))) {
        fs::remove(target / relative, ec);
        ec.clear();
      }
    }

    // fs::remove echoue sur un dossier non vide : seuls les dossiers devenus
    // vides disparaissent.
    for (const fs::path &directory : list_directories(target)) {
      fs::remove(directory, ec);
      ec.clear();
    }
  } catch (...) {
    // Jamais bloquant : au pire le joueur garde ses anciens scripts.
  }
}

void install_bundled_ui_scripts() noexcept {
  try {
    const utils::nt::library self{};

    std::error_code ec;
    fs::path source = self.get_folder() / "data" / "ui_scripts";
    if (!fs::is_directory(source, ec) || ec) {
      // L'exe peut etre lance depuis un autre dossier que celui du jeu : le
      // dossier courant est alors le dossier du jeu (voir resolve_game_path).
      ec.clear();
      const fs::path current = fs::current_path(ec);
      if (ec) {
        return;
      }
      source = current / "data" / "ui_scripts";
    }

    mirror_ui_scripts(source, game::get_appdata_path() / "data" / "ui_scripts");
  } catch (...) {
  }
}
} // namespace launcher

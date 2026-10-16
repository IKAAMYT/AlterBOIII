#include <std_include.hpp>

#include "bundled_ui_scripts.hpp"
#include "resource.hpp"

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

bool same_content(const fs::path &file, const std::string &content) {
  std::error_code ec;
  const std::uintmax_t size = fs::file_size(file, ec);
  if (ec || size != content.size()) {
    return false;
  }

  std::string data;
  return read_whole_file(file, data) && data == content;
}

bool write_whole_file(const fs::path &file, const std::string &content) {
  std::ofstream stream(file, std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) {
    return false;
  }
  stream.write(content.data(), static_cast<std::streamsize>(content.size()));
  stream.close();
  return !stream.fail();
}

struct script_file {
  fs::path relative;
  std::string content;
};

// Un chemin venu de la ressource ne doit jamais sortir du dossier cible.
bool safe_relative(const fs::path &relative) {
  if (relative.empty() || relative.is_absolute() || relative.has_root_name()) {
    return false;
  }
  return std::none_of(relative.begin(), relative.end(),
                      [](const fs::path &part) { return part == ".."; });
}

// Ressource UI_SCRIPTS generee par premake5.lua : pour chaque fichier,
// u32 taille du chemin, chemin, u32 taille du contenu, contenu.
bool read_embedded_scripts(std::vector<script_file> &files) {
  const std::string bundle = utils::nt::load_resource(UI_SCRIPTS);
  size_t offset = 0;
  const auto read_u32 = [&](uint32_t &value) {
    if (bundle.size() - offset < sizeof(value)) {
      return false;
    }
    std::memcpy(&value, bundle.data() + offset, sizeof(value));
    offset += sizeof(value);
    return true;
  };
  const auto read_bytes = [&](const uint32_t size, std::string &out) {
    if (bundle.size() - offset < size) {
      return false;
    }
    out.assign(bundle.data() + offset, size);
    offset += size;
    return true;
  };

  while (offset < bundle.size()) {
    uint32_t path_size{};
    uint32_t content_size{};
    std::string path;
    script_file file;
    if (!read_u32(path_size) || !read_bytes(path_size, path) ||
        !read_u32(content_size) || !read_bytes(content_size, file.content)) {
      files.clear();
      return false;
    }
    file.relative =
        fs::path(std::u8string(path.begin(), path.end())).lexically_normal();
    if (!safe_relative(file.relative)) {
      files.clear();
      return false;
    }
    files.emplace_back(std::move(file));
  }
  return !files.empty();
}

bool read_folder_scripts(const fs::path &source,
                         std::vector<script_file> &files) {
  std::vector<fs::path> relatives;
  if (!list_relative_files(source, relatives)) {
    return false;
  }
  for (fs::path &relative : relatives) {
    script_file file{std::move(relative), {}};
    if (!read_whole_file(source / file.relative, file.content)) {
      return false;
    }
    files.emplace_back(std::move(file));
  }
  return true;
}

// Anciens dossiers de scripts, remplaces depuis : charges en plus des
// nouveaux, ils ecraseraient le menu Social natif (alter_social) ou le menu
// de l'hote (kick_menu).
constexpr const wchar_t *OBSOLETE_SCRIPTS[] = {
    L"alter_social",          L"kick_menu", L"cac_options", L"luafixes",
    L"server_browser_button",
};

void remove_obsolete_scripts(const fs::path &root) {
  for (const wchar_t *name : OBSOLETE_SCRIPTS) {
    std::error_code ec;
    const fs::path folder = root / name;
    if (fs::is_directory(folder, ec) && !ec) {
      fs::remove_all(folder, ec);
    }
  }
}

void mirror_files(const std::vector<script_file> &wanted,
                  const fs::path &target) {
  std::error_code ec;

  // Garde-fou : une source sans aucun __init__.lua n'est pas un vrai dossier
  // de scripts. On ne touche alors a rien, pour ne jamais vider la
  // destination a cause d'une archive mal decompressee.
  const bool has_script =
      std::any_of(wanted.begin(), wanted.end(), [](const script_file &file) {
        return file.relative.filename() == fs::path("__init__.lua");
      });
  if (!has_script) {
    return;
  }

  bool all_copied = true;
  for (const script_file &file : wanted) {
    const fs::path to = target / file.relative;
    if (same_content(to, file.content)) {
      continue;
    }

    fs::create_directories(to.parent_path(), ec);
    ec.clear();
    if (!write_whole_file(to, file.content)) {
      all_copied = false;
    }
  }

  // Si une copie a echoue (fichier verrouille, disque plein...), on garde
  // les anciens scripts tels quels plutot que de laisser un melange.
  if (!all_copied) {
    return;
  }

  std::vector<std::wstring> wanted_keys;
  wanted_keys.reserve(wanted.size());
  for (const script_file &file : wanted) {
    wanted_keys.emplace_back(path_key(file.relative));
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

    std::vector<script_file> wanted;
    if (read_folder_scripts(source, wanted)) {
      mirror_files(wanted, target);
    }
  } catch (...) {
    // Jamais bloquant : au pire le joueur garde ses anciens scripts.
  }
}

void install_bundled_ui_scripts() noexcept {
  try {
    const utils::nt::library self{};
    const fs::path target = game::get_appdata_path() / "data" / "ui_scripts";

    // Les anciens scripts deposes a cote de l'exe sont aussi charges par le
    // jeu (<dossier exe>\boiii\ui_scripts) : on retire ceux qui sont perimes.
    // Idem dans data\ui_scripts du dossier de l'exe, ou ils restent quand on
    // decompresse un nouveau zip par-dessus une ancienne version.
    remove_obsolete_scripts(self.get_folder() / "boiii" / "ui_scripts");
    remove_obsolete_scripts(self.get_folder() / "data" / "ui_scripts");

    // 1. Les scripts embarques dans l'exe : toujours ceux de cette version.
    std::vector<script_file> embedded;
    if (read_embedded_scripts(embedded)) {
      mirror_files(embedded, target);
      return;
    }

    // 2. Sinon, le dossier data\ui_scripts du zip.
    std::error_code ec;
    fs::path source = self.get_folder() / "data" / "ui_scripts";
    if (!fs::is_directory(source, ec) || ec) {
      // L'exe peut etre lance depuis un autre dossier que celui du jeu : le
      // dossier courant est alors le dossier du jeu (voir resolve_game_path).
      ec.clear();
      const fs::path current = fs::current_path(ec);
      if (ec) {
        remove_obsolete_scripts(target);
        return;
      }
      source = current / "data" / "ui_scripts";
      remove_obsolete_scripts(source);
    }

    mirror_ui_scripts(source, target);
    remove_obsolete_scripts(target);
  } catch (...) {
  }
}
} // namespace launcher

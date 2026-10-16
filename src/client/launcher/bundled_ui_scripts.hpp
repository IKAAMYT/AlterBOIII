#pragma once

#include <std_include.hpp>

namespace launcher {
// AlterBO3 (IKAAM) : installe les scripts Lua livres avec l'exe.
//
// Le jeu ne lit ses scripts d'interface qu'a deux endroits :
//   %LOCALAPPDATA%\boiii\data\ui_scripts   et   <dossier exe>\boiii\ui_scripts
// Or le zip de release depose les notres dans <dossier exe>\data\ui_scripts,
// et l'updater a interdiction de toucher a data/ui_scripts (pour ne pas
// ecraser nos traductions avec les fichiers d'Ezz). Resultat : personne ne
// les installait, les joueurs gardaient d'anciens scripts ou n'en avaient pas.
//
// Cette fonction installe donc nos scripts dans
// %LOCALAPPDATA%\boiii\data\ui_scripts a chaque lancement du client. La
// source est la ressource UI_SCRIPTS embarquee dans l'exe (generee par
// premake5.lua depuis data/ui_scripts), ce qui marche meme quand l'exe est
// lance seul ; a defaut, le dossier <dossier exe>\data\ui_scripts du zip.
// Les anciens dossiers de scripts remplaces depuis (alter_social, kick_menu...)
// sont retires, y compris de <dossier exe>\boiii\ui_scripts.
void install_bundled_ui_scripts() noexcept;

// Miroir de `source` vers `target` : copie ce qui manque ou differe, puis
// retire de `target` ce qui n'existe plus dans `source`. Ne leve jamais
// d'exception et ne supprime rien si `source` ne contient aucun script.
void mirror_ui_scripts(const std::filesystem::path &source,
                       const std::filesystem::path &target) noexcept;
} // namespace launcher

# AlterBOIII 2.4.0 — merge de la beta d'Ezz (boiii-free `f45ed84`)

Base précédente : boiii-free `main` v3.0.0 (`5aa7fac`). Cette version intègre
les 218 commits de la branche `beta` d'Ezz (au 15 oct. 2026), en conservant
le launcher AlterBOIII, le branding, les traductions FR et le heartbeat
AlterCOD.

## Amis / lobby
- Join d'un ami via le lobby natif de l'hôte (rendez-vous master + punch NAT,
  repli `connect` direct), vérification/téléchargement du mod et de la usermap
  avant le join.
- Hot-join des parties en cours (chat vocal et session moteur fonctionnels),
  y compris ranked Zombies privé et hôtes en LAN.
- Présence à trois états : hors ligne / en ligne (party fermée) / en partie
  rejoignable.
- Invitations relayées par le master server (toast, bouton Accepter), valides
  5 min. **Le master server doit supporter `friendInvite` / `friendOnline` /
  `friendOffline`.**
- Onglet « Récents » dans le menu Social, bouton « Ajouter en ami » en un
  clic, « Copier le code ami », actions joueur (Rejoindre, Inviter,
  Promouvoir, Retirer de la party, Muet).
- Party ouverte/fermée (`friends_open`), bouton dans le menu principal,
  Social > Party et le menu pause de l'hôte.
- Le menu social custom `alter_social` est retiré : le menu natif patché par
  `social_friends` (version Ezz, textes FR) reprend la main.

## Chat vocal
- Implémentation réelle (PortAudio + Opus 48 kHz, décodeur par joueur), voix
  relayée par le serveur de jeu en match, choix du micro dans les réglages
  (`voice_input_device`), `muteall` / `unmuteall`, push-to-talk corrigé.

## Joueur
- Menu « Paramètres AlterBOIII » (ex-Paramètres rapides) : les flags de
  lancement deviennent des dvars modifiables en jeu (`boiii_skipIntro`,
  `boiii_skipForestCinematic`, `boiii_skipCinematics`, `boiii_fullLogs`,
  `boiii_ultrawide`, `boiii_allowCheats`...).
- Black Market multijoueur hors ligne : cryptokeys gagnés en jouant, supply
  drops, achats, stockés en SQLite (`boiii_players/user/dw.db`). Commande
  `cryptokeys [n]`.
- Économie locale rééquilibrée (`cg_codpoints_per_minute` 120 → 15, cap 5000
  → 1800), toggles « Max » séparés par monnaie.
- Kits d'armes Zombies (Gunsmith sur les armes Zombies ajoutées, PaP inclus).
- Menu pause de l'hôte : changer de map (stock + usermaps), expulser,
  redémarrer, réglages, party ouverte/fermée.
- `map` depuis le menu principal, split-screen en appuyant sur A sur une
  deuxième manette, préchargement des fastfiles de mode, rangs des bots,
  autocomplétion console limitée aux vrais dvars/commandes.

## Workshop / mods
- Téléchargement direct depuis le CDN Steam (repli SteamCMD), file d'attente
  avec pause/annulation/réordonnancement.
- Menu Workshop en jeu (catalogue paginé, aperçus, file de téléchargement,
  suppression). Page Mods refaite avec aperçus et fiche détaillée.
- `loadmod ""` décharge le mod, téléchargement des maps manquantes depuis le
  serveur courant, `customkeymap`.

## Launcher AlterBOIII
- Nouvelles sections Réglages > Stockage (espace par mod/map, purge des
  rapports de crash > 7 jours) et Sauvegardes de profil (zip de
  `boiii_players`, restauration).
- Page Amis : pseudo affiché avec le code ami, rafraîchissement 5 s.
- L'updater récupère désormais les données du **canal beta** d'Ezz
  (`r2.ezz.lol/boiii/beta/`), puisque le client est construit sur la beta.
  L'exe, `data/launcher` et `data/ui_scripts` restent protégés.
- **Mise à jour automatique de l'exe** : au lancement, AlterBOIII lit
  `update.json` (branche `main`). Si `version` est plus récente, il télécharge
  `exe_url` (uniquement depuis les releases du dépôt), vérifie `exe_size` et
  `exe_sha1`, remplace l'exe (l'ancien passe en `.exe.old`, supprimé au
  lancement suivant) puis se relance. Désactivable via le réglage « mises à
  jour automatiques » du launcher ou `-noupdate`. Le workflow « AlterBO3
  Release » réécrit `update.json` lui-même (version, URL, taille, SHA-1) quand
  le tag correspond au `tagName` de `premake5.lua` ; un exe déjà identique à
  celui publié n'est jamais retéléchargé.

## Serveur dédié
- Redémarrage automatique après erreur de script (`sv_errorRestartDelay`,
  5 s) avec rapport clair ; les joueurs voient la vraie raison.
- Autodiagnostic au démarrage (`server_lobby_selector`, `gamesettings`,
  `playlists.info`, `sv_lobby_mode`, `sv_maprotation`...).
- `net_port` est enfin le vrai port d'écoute. Bots exclus du rate-limit par
  IP, fixes `EXE_ERR_RELIABLE_CYCLED_OUT` et `EXE_BAD_STATS`.

## Scripting GSC/CSC
- Compilateur réorganisé, builtins `noclip`, `clientindex`, `rmdir`,
  `print`/`trace` variadiques, erreurs de link expliquées à la source,
  scripts avec `#using` manquant ignorés au lieu de crasher, détection de
  boucles infinies derrière `-scr-debug`.

## Robustesse
- Erreurs expliquées avec cause et correctif, dialogue de crash
  « Redémarrer / Fermer », détection de freeze (30 s sans frame).

## Changements cassants
- Mods Lua : `game.setcurrenciesmaxed` → `setcurrencymaxed` /
  `iscurrencymaxed` ; `httpget` / `httppost` / `hotreload*` demandent la
  permission unsafe ; `kick_menu` supprimé (remplacé par `host_menu`).
- Hébergeurs : `net_port` change de sens ; `machinecfg/playlists.info` et
  `lobby_scripts/server_lobby_selector` requis ; une erreur de script
  redémarre la map après 5 s.
- Protocole réseau : nouveaux paquets (`lobbyJoin`, `friendInvite`,
  `friendOnline`, `serverError`, `voice`, `nameoverride`), champ `lobby` dans
  `getInfo`. Clients 2.3.0 (et avant) et 2.4.0 non interopérables pour le join d'amis.
- Build : VS 2026 / `premake5 vs2026`, `-fbracket-depth=1024`, nouveaux
  submodules `opus`, `portaudio`, `sqlite`, `sqlite_orm`, `xz`.

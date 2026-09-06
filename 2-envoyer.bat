@echo off
setlocal EnableDelayedExpansion
title AlterBOIII - Envoyer et compiler
color 0B

cd /d "%~dp0"

echo ===============================================================
echo   AlterBOIII - envoi vers GitHub + lancement du build
echo ===============================================================
echo.

where git >nul 2>&1
if errorlevel 1 (
    echo [ERREUR] git introuvable. Lance d'abord 1-installer-git.bat
    echo.
    pause
    exit /b 1
)

if not exist ".git" (
    echo [ERREUR] Ce dossier n'est pas un depot git.
    echo Place ce fichier dans le clone, pas dans le ZIP decompresse.
    echo.
    pause
    exit /b 1
)

REM --- Identite git configuree ? ------------------------------------------
REM Sans nom + email, git refuse de committer. On demande une seule fois.
set "GNAME="
for /f "delims=" %%n in ('git config --get user.name 2^>nul') do set "GNAME=%%n"
set "GMAIL="
for /f "delims=" %%m in ('git config --get user.email 2^>nul') do set "GMAIL=%%m"

if "!GNAME!"=="" goto :demander_identite
if "!GMAIL!"=="" goto :demander_identite
goto :identite_ok

:demander_identite
echo ---------------------------------------------------------------
echo   Premiere utilisation : git a besoin de savoir qui tu es.
echo   Ces infos apparaitront comme auteur de tes commits.
echo ---------------------------------------------------------------
echo.
set "SAISIE_NOM="
set /p "SAISIE_NOM=Ton nom (Entree = IKAAM) : "
if "!SAISIE_NOM!"=="" set "SAISIE_NOM=IKAAM"

set "SAISIE_MAIL="
set /p "SAISIE_MAIL=Ton email GitHub : "
if "!SAISIE_MAIL!"=="" (
    echo.
    echo [ERREUR] L'email est obligatoire pour committer.
    echo.
    pause
    exit /b 1
)

git config --global user.name "!SAISIE_NOM!"
git config --global user.email "!SAISIE_MAIL!"
echo.
echo   Identite enregistree. Tu ne le reverras plus.
echo.

:identite_ok

REM --- Ce script ne doit pas se committer lui-meme -------------------------
findstr /x /c:"2-envoyer.bat" .gitignore >nul 2>&1
if errorlevel 1 (
    echo.>>.gitignore
    echo 2-envoyer.bat>>.gitignore
    echo 1-installer-git.bat>>.gitignore
)
git rm --cached "2-envoyer.bat" >nul 2>&1
git rm --cached "1-installer-git.bat" >nul 2>&1

REM --- Y a-t-il quelque chose a envoyer ? ---------------------------------
git add -A
git diff --cached --quiet
if not errorlevel 1 (
    echo Aucun changement a envoyer. Tout est deja sur GitHub.
    echo.
    pause
    exit /b 0
)

echo Fichiers modifies :
echo ---------------------------------------------------------------
git diff --cached --name-status
echo ---------------------------------------------------------------
echo.

REM --- Branche ------------------------------------------------------------
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD') do set "BRANCHE=%%b"
echo Branche courante : !BRANCHE!
if /i "!BRANCHE!"=="main" (
    echo.
    echo   [!] Tu es sur main. Le launcher se telecharge depuis main :
    echo       un fichier casse ici casse le launcher de TOUS tes joueurs.
    echo       Pour tester sans risque, donne un nom de branche ci-dessous.
)
echo.
set "NOUVELLE="
set /p "NOUVELLE=Nom d'une NOUVELLE branche (Entree = rester sur !BRANCHE!) : "
if not "!NOUVELLE!"=="" (
    git checkout -b "!NOUVELLE!"
    if errorlevel 1 (
        echo [ERREUR] Impossible de creer la branche !NOUVELLE!
        echo.
        pause
        exit /b 1
    )
    set "BRANCHE=!NOUVELLE!"
)

REM --- Message de commit --------------------------------------------------
set "MSG="
set /p "MSG=Message de commit (Entree = message automatique) : "
if "!MSG!"=="" set "MSG=Mise a jour AlterBOIII"

git commit -m "!MSG!"
if errorlevel 1 (
    echo [ERREUR] Le commit a echoue.
    echo.
    pause
    exit /b 1
)

echo.
echo Envoi vers GitHub...
git push -u origin "!BRANCHE!"
if errorlevel 1 (
    echo.
    echo [ERREUR] Le push a echoue.
    echo Si c'est la premiere fois, une fenetre de connexion GitHub
    echo a pu s'ouvrir : accepte-la puis relance ce script.
    echo.
    pause
    exit /b 1
)

echo.
echo ===============================================================
echo   ENVOYE. Le build demarre tout seul.
echo.
echo   Suivi en direct :
echo   https://github.com/IKAAMYT/AlterBOIII/actions
echo.
echo   Compte environ 30 minutes.
echo   L'exe compile sera dans l'artefact "AlterBO3-ci" en bas
echo   de la page du build.
echo ===============================================================
echo.

set "OUVRIR="
set /p "OUVRIR=Ouvrir la page des builds maintenant ? (o/N) : "
if /i "!OUVRIR!"=="o" start "" "https://github.com/IKAAMYT/AlterBOIII/actions"

pause

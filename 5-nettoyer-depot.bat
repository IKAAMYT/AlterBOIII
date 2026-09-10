@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
title AlterBOIII - Nettoyer le depot
color 0E

echo ===============================================================
echo   Retirer du depot ce qui n'a rien a y faire
echo.
echo   Le dossier "Claude outputs" (746 Ko d'apercus PNG que je
echo   t'ai envoyes) a ete versionne par un "git add -A".
echo   Les fichiers restent sur ton disque, ils sortent juste du
echo   depot et sont ajoutes au .gitignore.
echo ===============================================================
echo.

if not exist "%~dp0.git" (
    echo [ERREUR] Ce dossier n'est pas un depot git.
    pause
    exit /b 1
)

echo [1/3] Mise a jour du .gitignore...
findstr /x /c:"Claude outputs/" .gitignore >nul 2>&1
if errorlevel 1 echo Claude outputs/>>.gitignore

echo [2/3] Retrait du suivi...
git rm -r --cached "Claude outputs" >nul 2>&1
if errorlevel 1 (
    echo       Deja hors du depot, rien a retirer.
)
git add .gitignore
git commit -m "Ne pas versionner le dossier d'apercus"
if errorlevel 1 (
    echo.
    echo   Rien a enregistrer.
    echo.
    pause
    exit /b 0
)

echo [3/3] Envoi...
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "BR=%%b"
git push origin "!BR!"
if errorlevel 1 (
    echo.
    echo   [ERREUR] Envoi refuse. Regarde le message ci-dessus.
    echo.
    pause
    exit /b 1
)

echo.
echo ===============================================================
echo   TERMINE - les apercus sont toujours sur ton disque,
echo   simplement plus dans le depot.
echo ===============================================================
echo.
pause

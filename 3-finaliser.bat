@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
title AlterBOIII - Finaliser la 2.1.0
color 0E

set "TAG=v2.1.0"
set "LOG=%~dp0finalisation.log"
echo AlterBOIII - finalisation > "%LOG%"
echo Date : %DATE% %TIME% >> "%LOG%"

echo ===============================================================
echo   Finalisation de la publication 2.1.0
echo.
echo   Deux corrections :
echo     1. retirer publication.log du depot (il n'y a rien a y faire)
echo     2. deplacer le tag %TAG% sur le dernier commit
echo ===============================================================
echo.

REM --- 1. le journal ne doit pas etre versionne --------------------------
echo [1/4] Nettoyage du journal...
findstr /x /c:"publication.log" .gitignore >nul 2>&1
if errorlevel 1 echo publication.log>>.gitignore
findstr /x /c:"finalisation.log" .gitignore >nul 2>&1
if errorlevel 1 echo finalisation.log>>.gitignore
findstr /x /c:"restauration.log" .gitignore >nul 2>&1
if errorlevel 1 echo restauration.log>>.gitignore
git rm --cached publication.log >> "%LOG%" 2>&1
git add .gitignore >> "%LOG%" 2>&1
git commit -m "Ne pas versionner les journaux des scripts" >> "%LOG%" 2>&1
if errorlevel 1 echo       Rien a nettoyer.

REM --- 2. etat du tag distant -------------------------------------------
echo [2/4] Lecture des tags distants...
git fetch origin --tags >> "%LOG%" 2>&1

for /f "delims=" %%h in ('git rev-parse --short HEAD 2^>nul') do set "ICI=%%h"
set "TAGSUR="
for /f "delims=" %%t in ('git rev-parse --short "%TAG%^{commit}" 2^>nul') do set "TAGSUR=%%t"

echo       commit actuel : !ICI!
if "!TAGSUR!"=="" (
    echo       le tag %TAG% n'existe pas encore
) else (
    echo       tag %TAG% pose sur : !TAGSUR!
)

if "!TAGSUR!"=="!ICI!" (
    echo.
    echo   Le tag est deja au bon endroit. Rien a deplacer.
    goto :envoi
)

echo.
echo   -----------------------------------------------------------
echo   Le tag %TAG% existe sur GitHub mais pointe sur un ANCIEN
echo   commit. Si ton workflow de release se declenche sur les tags,
echo   il compilerait donc l'ancien code.
echo.
echo   Le deplacer sur !ICI! ecrase le tag distant.
echo   -----------------------------------------------------------
echo.
set /p OK="Deplacer le tag %TAG% sur le commit actuel ? (oui/non) : "
if /i not "!OK!"=="oui" (
    echo Deplacement refuse. >> "%LOG%"
    echo.
    echo   Tag laisse en place. On envoie quand meme le nettoyage.
    goto :envoi
)

echo [3/4] Deplacement du tag...
git tag -f "%TAG%" >> "%LOG%" 2>&1
if errorlevel 1 goto :echec
git push origin "%TAG%" --force >> "%LOG%" 2>&1
if errorlevel 1 goto :echec
echo       tag %TAG% deplace sur !ICI!

:envoi
echo [4/4] Envoi...
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "BR=%%b"
git push origin "!BR!" >> "%LOG%" 2>&1
if errorlevel 1 goto :echec

echo. >> "%LOG%"
git log --oneline -3 >> "%LOG%" 2>&1
git tag --points-at HEAD >> "%LOG%" 2>&1

echo.
echo ===============================================================
echo   TERMINE
echo.
git log --oneline -1
echo.
echo   Verifie l'onglet Actions du depot : c'est le build qui
echo   dira si le C++ compile.
echo ===============================================================
echo.
pause
exit /b 0

:echec
echo. >> "%LOG%"
echo ECHEC. >> "%LOG%"
echo.
echo ---------------------------------------------------------------
echo   [ERREUR] Echec. Ouvre finalisation.log, ou dis-le moi.
echo ---------------------------------------------------------------
echo.
pause
exit /b 1

@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"
title AlterBOIII - Corriger le CI
color 0E

echo ===============================================================
echo   Retirer la croix rouge permanente de l'onglet Actions
echo.
echo   "Build and Publish to R2" se declenche sur chaque tag et
echo   echoue toujours : il publie vers Cloudflare R2, que tu
echo   n'utilises pas. Il passe en declenchement manuel.
echo.
echo   Les deux autres workflows ne sont PAS touches.
echo ===============================================================
echo.

if not exist "%~dp0.git" (
    echo [ERREUR] Ce dossier n'est pas un depot git.
    pause
    exit /b 1
)

echo [1/3] Modification du workflow...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0corriger-ci.ps1"
set "RES=%errorlevel%"
if "%RES%"=="1" (
    echo.
    echo   Rien a publier.
    echo.
    pause
    exit /b 0
)
if not "%RES%"=="0" (
    echo.
    echo   Abandon, aucun fichier modifie.
    echo.
    pause
    exit /b 1
)

echo [2/3] Enregistrement...
git add .github/workflows/build-and-publish.yml
git commit -m "CI: build-and-publish (R2) en declenchement manuel uniquement"
if errorlevel 1 (
    echo   Rien a enregistrer.
    pause
    exit /b 0
)

echo [3/3] Envoi...
for /f "delims=" %%b in ('git rev-parse --abbrev-ref HEAD 2^>nul') do set "BR=%%b"
git push origin "!BR!"
if errorlevel 1 (
    echo.
    echo ---------------------------------------------------------------
    echo   [ERREUR] Envoi refuse.
    echo.
    echo   Si le message parle de "workflow scope" : modifier un fichier
    echo   de .github/workflows demande une autorisation particuliere.
    echo   Le plus simple est alors de faire la modification directement
    echo   sur github.com, dans l'editeur de fichier.
    echo ---------------------------------------------------------------
    echo.
    pause
    exit /b 1
)

echo.
echo ===============================================================
echo   TERMINE
echo.
echo   La prochaine publication n'aura plus de croix rouge.
echo   Le workflow reste lancable a la main depuis l'onglet Actions.
echo ===============================================================
echo.
pause

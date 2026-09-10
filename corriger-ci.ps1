$ErrorActionPreference = 'Stop'
$p = Join-Path $PSScriptRoot '.github\workflows\build-and-publish.yml'

if (-not (Test-Path $p)) {
    Write-Host "  Fichier introuvable : $p"
    exit 2
}

$s = [IO.File]::ReadAllText($p)

$nouveau = @"
on:
  # Declenchement MANUEL uniquement.
  #
  # Ce workflow publie les binaires vers Cloudflare R2. Le depot AlterBOIII
  # n'utilise pas R2 : les secrets R2_BUCKET, R2_ACCOUNT_ID, R2_ACCESS_KEY_ID
  # et R2_SECRET_ACCESS_KEY n'existent pas. Declenche sur chaque tag "v*", il
  # echouait donc a CHAQUE publication et laissait une croix rouge permanente
  # dans l'onglet Actions, ce qui finirait par masquer une vraie panne.
  #
  # C'est alterbo3-release.yml qui construit et publie reellement.
  workflow_dispatch:

"@

$re = [regex]::new('(?m)^on:\r?\n(?:[ \t#].*\r?\n|\r?\n)*?(?=^concurrency:)')

if ($s -match '(?m)^\s+# Declenchement MANUEL uniquement\.') {
    Write-Host "  Deja corrige, rien a faire."
    exit 1
}
if (-not $re.IsMatch($s)) {
    Write-Host "  Bloc 'on:' introuvable — le fichier a change de forme."
    Write-Host "  Corrige-le a la main sur GitHub, je ne touche a rien."
    exit 2
}

$s2 = $re.Replace($s, $nouveau, 1)

# Controle : le corps du workflow doit etre intact.
if (($s2 -notmatch '(?m)^  build:') -or ($s2 -notmatch 'R2_BUCKET')) {
    Write-Host "  Resultat suspect, modification annulee."
    exit 2
}

[IO.File]::WriteAllText($p, $s2)
Write-Host "  build-and-publish.yml : declencheur 'push tags' retire."
exit 0

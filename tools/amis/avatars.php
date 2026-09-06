<?php
/**
 * AlterBOIII — photos de profil (IKAAM)
 * ------------------------------------------------------------------
 * Deux actions, aucune base de données : tout tient dans des fichiers.
 *
 *   POST  action=set    code=<code ami>  image=<data URI ou base64>  [token=<jeton>]
 *         -> {"ok":true,"token":"...","url":"..."}
 *         Le premier envoi pour un code le REVENDIQUE et renvoie un
 *         jeton. Les envois suivants exigent ce jeton : sans ça,
 *         n'importe qui pourrait remplacer la photo de n'importe qui.
 *
 *   GET   action=get    codes=<code1,code2,...>
 *         -> {"ok":true,"avatars":{"<code>":"<url>", ...}}
 *
 * À déposer dans  https://ikaam.fr/amis/avatars.php
 *
 * Prérequis : l'extension GD (présente par défaut chez IONOS).
 * Crée automatiquement son dossier de stockage au premier appel.
 */

declare(strict_types=1);

// Le launcher est charge en file:// : son origine vaut "null", donc
// il faut autoriser explicitement, sinon le navigateur bloque la reponse.
header('Access-Control-Allow-Origin: *');
header('X-Content-Type-Options: nosniff');

if (($_SERVER['REQUEST_METHOD'] ?? '') === 'OPTIONS') {
    header('Access-Control-Allow-Methods: GET, POST, OPTIONS');
    header('Access-Control-Allow-Headers: Content-Type');
    http_response_code(204);
    exit;
}

// ── Réglages ────────────────────────────────────────────────────────
const DOSSIER       = __DIR__ . '/avatars';   // stockage des images
// Les images ne sont PLUS servies en statique depuis le dossier : chez
// IONOS, ce dossier renvoyait 500 quelle que soit la configuration. On les
// sert via le script lui-meme (action=img), ce qui rend le stockage
// totalement independant de la configuration Apache.
const URL_PUBLIQUE  = 'https://ikaam.fr/amis/avatars.php?action=img&code=';
const TAILLE_PX     = 128;                    // côté de l'image finale
const MAX_ENVOI     = 400 * 1024;             // 400 Ko de charge utile max
const MAX_CODES     = 200;                    // codes demandés par appel
const DELAI_ENVOI   = 20;                     // secondes entre deux envois

function repondre(array $data, int $code = 200): void
{
    header('Content-Type: application/json; charset=utf-8');
    http_response_code($code);
    echo json_encode($data, JSON_UNESCAPED_SLASHES);
    exit;
}

function erreur(string $message, int $code = 400): void
{
    repondre(['ok' => false, 'error' => $message], $code);
}

/** Un code ami est une suite de 5 à 20 chiffres — même règle que le launcher. */
function code_valide(string $code): bool
{
    return preg_match('/^[0-9]{5,20}$/', $code) === 1;
}

function chemin_image(string $code): string { return DOSSIER . '/' . $code . '.jpg'; }
function chemin_jeton(string $code): string { return DOSSIER . '/' . $code . '.token'; }

function preparer_dossier(): void
{
    if (!is_dir(DOSSIER) && !mkdir(DOSSIER, 0755, true) && !is_dir(DOSSIER)) {
        erreur('storage_unavailable', 500);
    }
    // PAS de .htaccess ici.
    //
    // La version precedente y ecrivait "php_flag engine off". Chez IONOS,
    // PHP tourne en FastCGI : php_flag est une directive inconnue d'Apache,
    // qui repond alors 500 sur TOUT le dossier — les images devenaient
    // illisibles.
    //
    // La protection ne repose de toute facon pas sur Apache : le nom de
    // fichier est construit a partir d'un code valide contre /^[0-9]{5,20}$/
    // et suffixe .jpg en dur, donc rien d'executable ne peut y atterrir ; et
    // le contenu est integralement re-encode par GD, ce qui detruit toute
    // charge utile cachee dans l'image d'origine.
}

/**
 * Décode, VALIDE et ré-encode l'image.
 *
 * Le ré-encodage n'est pas cosmétique : il transforme le fichier reçu en
 * un JPEG propre généré par GD. Un fichier « polyglotte » (image valide
 * en apparence, code PHP concaténé à la fin) ne survit pas à l'opération.
 */
function normaliser_image(string $charge_utile): string
{
    // Accepte "data:image/png;base64,xxxx" comme du base64 brut.
    if (str_starts_with($charge_utile, 'data:')) {
        $virgule = strpos($charge_utile, ',');
        if ($virgule === false) {
            erreur('bad_image');
        }
        $charge_utile = substr($charge_utile, $virgule + 1);
    }

    $binaire = base64_decode(strtr($charge_utile, ' ', '+'), true);
    if ($binaire === false || $binaire === '') {
        erreur('bad_image');
    }
    if (strlen($binaire) > MAX_ENVOI) {
        erreur('image_too_large', 413);
    }

    $info = @getimagesizefromstring($binaire);
    if ($info === false) {
        erreur('not_an_image');
    }
    if (!in_array($info[2], [IMAGETYPE_JPEG, IMAGETYPE_PNG, IMAGETYPE_GIF, IMAGETYPE_WEBP], true)) {
        erreur('unsupported_format');
    }
    // Garde-fou contre les images « bombe » : 4000 px de côté suffisent
    // largement pour une photo de profil.
    if ($info[0] > 4000 || $info[1] > 4000) {
        erreur('image_too_large', 413);
    }

    $source = @imagecreatefromstring($binaire);
    if ($source === false) {
        erreur('not_an_image');
    }

    // Recadrage centré en carré, puis mise à l'échelle.
    $l = imagesx($source);
    $h = imagesy($source);
    $cote = min($l, $h);
    $x = (int) (($l - $cote) / 2);
    $y = (int) (($h - $cote) / 2);

    $dest = imagecreatetruecolor(TAILLE_PX, TAILLE_PX);
    imagecopyresampled($dest, $source, 0, 0, $x, $y, TAILLE_PX, TAILLE_PX, $cote, $cote);
    imagedestroy($source);

    ob_start();
    imagejpeg($dest, null, 86);
    $jpeg = (string) ob_get_clean();
    imagedestroy($dest);

    if ($jpeg === '') {
        erreur('encode_failed', 500);
    }
    return $jpeg;
}

/** Limite le débit par IP, pour que le dossier ne serve pas de dépotoir. */
function limiter_debit(): void
{
    $ip = $_SERVER['REMOTE_ADDR'] ?? '0.0.0.0';
    $marqueur = sys_get_temp_dir() . '/alterboiii_av_' . sha1($ip);
    if (is_file($marqueur) && (time() - (int) filemtime($marqueur)) < DELAI_ENVOI) {
        erreur('too_many_requests', 429);
    }
    @touch($marqueur);
}

// ── Routage ─────────────────────────────────────────────────────────
$action = (string) ($_REQUEST['action'] ?? '');

if ($action === 'set') {
    preparer_dossier();
    limiter_debit();

    $code = trim((string) ($_REQUEST['code'] ?? ''));
    if (!code_valide($code)) {
        erreur('bad_code');
    }

    $image = (string) ($_REQUEST['image'] ?? '');
    if ($image === '') {
        erreur('missing_image');
    }

    $fichier_jeton = chemin_jeton($code);
    $jeton_fourni = (string) ($_REQUEST['token'] ?? '');

    // Le dossier de stockage est SERVI PAR LE WEB : n'importe qui peut
    // ouvrir <code>.token dans un navigateur. On n'y ecrit donc jamais le
    // jeton lui-meme, seulement son empreinte — inutilisable pour
    // s'authentifier, exactement comme un mot de passe hache.
    if (is_file($fichier_jeton)) {
        $empreinte = trim((string) @file_get_contents($fichier_jeton));
        if ($empreinte === '' ||
            !hash_equals($empreinte, hash('sha256', $jeton_fourni))) {
            erreur('forbidden', 403);
        }
        $jeton = $jeton_fourni;
    } else {
        // Première fois : on revendique le code. Le jeton en clair n'est
        // renvoye qu'ici, une seule fois ; le serveur n'en garde que le hash.
        $jeton = bin2hex(random_bytes(20));
        if (@file_put_contents($fichier_jeton, hash('sha256', $jeton)) === false) {
            erreur('storage_unavailable', 500);
        }
    }

    $jpeg = normaliser_image($image);
    if (@file_put_contents(chemin_image($code), $jpeg) === false) {
        erreur('storage_unavailable', 500);
    }

    repondre([
        'ok'    => true,
        'token' => $jeton,
        // Le paramètre v force le contournement du cache navigateur.
        'url'   => URL_PUBLIQUE . $code . '&v=' . time(),
    ]);
}

if ($action === 'get') {
    $brut = (string) ($_REQUEST['codes'] ?? '');
    if ($brut === '') {
        repondre(['ok' => true, 'avatars' => (object) []]);
    }

    $codes = array_slice(array_unique(array_filter(
        array_map('trim', explode(',', $brut)),
        'code_valide'
    )), 0, MAX_CODES);

    $avatars = [];
    foreach ($codes as $code) {
        $fichier = chemin_image($code);
        if (is_file($fichier)) {
            $avatars[$code] = URL_PUBLIQUE . $code . '&v=' . filemtime($fichier);
        }
    }

    repondre(['ok' => true, 'avatars' => $avatars ?: (object) []]);
}

if ($action === 'img') {
    $code = trim((string) ($_REQUEST['code'] ?? ''));
    if (!code_valide($code)) {
        erreur('bad_code');
    }
    $fichier = chemin_image($code);
    if (!is_file($fichier)) {
        header('Content-Type: application/json; charset=utf-8');
        http_response_code(404);
        echo '{"ok":false,"error":"not_found"}';
        exit;
    }

    $empreinte = '"' . md5($code . '-' . filemtime($fichier)) . '"';
    if (trim((string) ($_SERVER['HTTP_IF_NONE_MATCH'] ?? '')) === $empreinte) {
        http_response_code(304);
        exit;
    }

    header('Content-Type: image/jpeg');
    header('Content-Length: ' . filesize($fichier));
    header('Cache-Control: public, max-age=300');
    header('ETag: ' . $empreinte);
    readfile($fichier);
    exit;
}

if ($action === 'delete') {
    $code = trim((string) ($_REQUEST['code'] ?? ''));
    if (!code_valide($code)) {
        erreur('bad_code');
    }
    $fichier_jeton = chemin_jeton($code);
    $empreinte = is_file($fichier_jeton)
        ? trim((string) @file_get_contents($fichier_jeton)) : '';
    if ($empreinte === '' ||
        !hash_equals($empreinte, hash('sha256', (string) ($_REQUEST['token'] ?? '')))) {
        erreur('forbidden', 403);
    }
    @unlink(chemin_image($code));
    repondre(['ok' => true]);
}

erreur('unknown_action', 404);

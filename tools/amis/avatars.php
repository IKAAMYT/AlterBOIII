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
// Quota d'écriture par IP sur une fenêtre glissante. Assez large pour une
// famille ou un CGNAT, assez serré pour qu'on ne remplisse pas le disque.
const FENETRE_ENVOI      = 300;               // 5 minutes
const MAX_ENVOIS_FENETRE = 12;                // écritures autorisées dedans

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

/** memory_limit en octets ; 0 si illimite ou illisible. */
function memoire_limite(): int
{
    $brut = trim((string) ini_get('memory_limit'));
    if ($brut === '' || $brut === '-1') {
        return 0;
    }
    $unite = strtolower(substr($brut, -1));
    $n = (int) $brut;
    if ($unite === 'g') { return $n * 1024 * 1024 * 1024; }
    if ($unite === 'm') { return $n * 1024 * 1024; }
    if ($unite === 'k') { return $n * 1024; }
    return $n;
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
    // Garde-fou contre les images « bombe ».
    //
    // 4000 px etait BEAUCOUP trop permissif : GD decompresse en vraies
    // couleurs, soit largeur x hauteur x 4 octets. Une image 4000x4000
    // pesant 300 Ko compresses reclame ~64 Mo de RAM une fois decodee, et
    // quelques envois simultanes suffisaient a epuiser le pool PHP du
    // mutualise — donc a mettre TOUT ikaam.fr a genoux avec un fichier
    // parfaitement valide. 1600 px reste dix fois la taille affichee.
    if ($info[0] > 1600 || $info[1] > 1600) {
        erreur('image_too_large', 413);
    }

    // Ceinture et bretelles : on refuse aussi ce qui ne tiendrait pas dans
    // la memoire restante, quelles que soient les dimensions.
    $besoin = $info[0] * $info[1] * 4 + 2 * 1024 * 1024;
    $limite = memoire_limite();
    if ($limite > 0 && (memory_get_usage(true) + $besoin) > $limite) {
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

/**
 * Limite le débit par IP, pour que le dossier ne serve pas de dépotoir.
 *
 * Le marqueur vit dans NOTRE dossier, plus dans sys_get_temp_dir() : chez
 * IONOS le temporaire est partage entre sites et purge sans preavis, ce qui
 * revenait a desactiver la limite au hasard.
 */
function limiter_debit(): void
{
    $seau = DOSSIER . '/.debit';
    if (!is_dir($seau)) {
        @mkdir($seau, 0755, true);
    }
    // Un simple delai fixe entre deux envois punissait les innocents : deux
    // joueurs sous la meme box, ou n'importe qui derriere un CGNAT
    // d'operateur, partagent une IP et se bloquaient mutuellement. On
    // compte donc les envois sur une fenetre glissante : une rafale
    // normale passe, un flot continu non.
    $ip = $_SERVER['REMOTE_ADDR'] ?? '0.0.0.0';
    $marqueur = $seau . '/' . sha1($ip);
    $maintenant = time();

    $horodatages = [];
    $brut = @file_get_contents($marqueur);
    if (is_string($brut) && $brut !== '') {
        foreach (explode(',', $brut) as $t) {
            $t = (int) $t;
            if ($t > 0 && ($maintenant - $t) < FENETRE_ENVOI) {
                $horodatages[] = $t;
            }
        }
    }

    if (count($horodatages) >= MAX_ENVOIS_FENETRE) {
        header('Retry-After: ' . FENETRE_ENVOI);
        erreur('too_many_requests', 429);
    }

    $horodatages[] = $maintenant;
    @file_put_contents($marqueur, implode(',', $horodatages), LOCK_EX);

    // Menage occasionnel : sans ca le dossier grossit indefiniment.
    if (random_int(1, 50) === 1) {
        foreach ((array) @scandir($seau) as $f) {
            if ($f === '.' || $f === '..') { continue; }
            $c = $seau . '/' . $f;
            if (is_file($c) && (time() - (int) @filemtime($c)) > 3600) {
                @unlink($c);
            }
        }
    }
}

/** Écriture atomique : personne ne doit lire un fichier à moitié écrit. */
function ecrire_atomique(string $cible, string $contenu): bool
{
    $tmp = $cible . '.' . bin2hex(random_bytes(6)) . '.tmp';
    if (@file_put_contents($tmp, $contenu, LOCK_EX) === false) {
        return false;
    }
    if (!@rename($tmp, $cible)) {
        @unlink($tmp);
        return false;
    }
    return true;
}

// ── Routage ─────────────────────────────────────────────────────────
$action = (string) ($_REQUEST['action'] ?? '');

if ($action === 'set') {
    // POST obligatoire. Avec $_REQUEST, un GET etait accepte : le code ET le
    // jeton se retrouvaient alors en clair dans la query string, donc dans
    // les journaux d'acces IONOS, dans l'historique et dans l'en-tete
    // Referer envoye aux sites tiers. Le jeton est l'unique preuve de
    // propriete d'un code : il ne doit jamais transiter par une URL.
    if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
        erreur('post_required', 405);
    }

    preparer_dossier();
    limiter_debit();

    $code = trim((string) ($_POST['code'] ?? ''));
    if (!code_valide($code)) {
        erreur('bad_code');
    }

    $image = (string) ($_POST['image'] ?? '');
    if ($image === '') {
        erreur('missing_image');
    }

    $fichier_jeton = chemin_jeton($code);
    $jeton_fourni = (string) ($_POST['token'] ?? '');
    $deja_revendique = is_file($fichier_jeton);

    // Le dossier de stockage est SERVI PAR LE WEB : n'importe qui peut
    // ouvrir <code>.token dans un navigateur. On n'y ecrit donc jamais le
    // jeton lui-meme, seulement son empreinte — inutilisable pour
    // s'authentifier, exactement comme un mot de passe hache.
    //
    // On verifie le jeton AVANT de toucher a l'image : inutile de faire
    // travailler GD pour quelqu'un qu'on va refuser.
    if ($deja_revendique) {
        $empreinte = trim((string) @file_get_contents($fichier_jeton));
        if ($empreinte === '' ||
            !hash_equals($empreinte, hash('sha256', $jeton_fourni))) {
            erreur('forbidden', 403);
        }
        $jeton = $jeton_fourni;
    }

    // L'image est validee AVANT la revendication. Dans l'ordre inverse, un
    // envoi rate (fichier corrompu, format exotique) consommait quand meme
    // la revendication du code : le joueur se retrouvait proprietaire d'un
    // avatar inexistant, sans jeton en poche puisque la reponse etait une
    // erreur. Il ne pouvait alors PLUS JAMAIS mettre sa photo.
    $jpeg = normaliser_image($image);

    if (!$deja_revendique) {
        // Première fois : on revendique le code. Le jeton en clair n'est
        // renvoye qu'ici, une seule fois ; le serveur n'en garde que le hash.
        $jeton = bin2hex(random_bytes(20));
        if (!ecrire_atomique($fichier_jeton, hash('sha256', $jeton))) {
            erreur('storage_unavailable', 500);
        }
    }

    if (!ecrire_atomique(chemin_image($code), $jpeg)) {
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
    // Meme raison que pour 'set' : le jeton ne doit jamais passer par l'URL.
    if (($_SERVER['REQUEST_METHOD'] ?? '') !== 'POST') {
        erreur('post_required', 405);
    }
    preparer_dossier();
    // La limite de debit manquait ici : sans elle, 'delete' offrait un banc
    // d'essai illimite pour deviner un jeton. hash_equals protege du timing,
    // pas du nombre d'essais.
    limiter_debit();

    $code = trim((string) ($_POST['code'] ?? ''));
    if (!code_valide($code)) {
        erreur('bad_code');
    }
    $fichier_jeton = chemin_jeton($code);
    $empreinte = is_file($fichier_jeton)
        ? trim((string) @file_get_contents($fichier_jeton)) : '';
    if ($empreinte === '' ||
        !hash_equals($empreinte, hash('sha256', (string) ($_POST['token'] ?? '')))) {
        erreur('forbidden', 403);
    }
    @unlink(chemin_image($code));
    repondre(['ok' => true]);
}

erreur('unknown_action', 404);

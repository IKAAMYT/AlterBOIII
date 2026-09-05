<?php
/**
 * AlterCOD — présence des amis (IKAAM)
 * ------------------------------------------------------------------
 * Module autonome à inclure depuis amis/api.php. Il ne suppose RIEN de ton
 * schéma existant : il lui faut seulement
 *   - un PDO déjà connecté,
 *   - l'id de l'utilisateur authentifié par le token (ce que fait déjà api.php),
 *   - la liste des ids d'amis acceptés (ce que ton action=list calcule déjà).
 *
 * Table à créer une fois :
 *
 *   CREATE TABLE altercod_presence (
 *     user_id     INT UNSIGNED NOT NULL PRIMARY KEY,
 *     status      TINYINT UNSIGNED NOT NULL DEFAULT 0,
 *     server      VARCHAR(64)  NOT NULL DEFAULT '',
 *     map         VARCHAR(64)  NOT NULL DEFAULT '',
 *     gametype    VARCHAR(64)  NOT NULL DEFAULT '',
 *     mode        INT          NOT NULL DEFAULT 0,
 *     mod_id      VARCHAR(64)  NOT NULL DEFAULT '',
 *     updated_at  INT UNSIGNED NOT NULL DEFAULT 0,
 *     INDEX (updated_at)
 *   ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
 */

// Au-delà de ce délai sans heartbeat, le joueur repasse hors ligne.
// Le jeu et le launcher battent toutes les 30 s.
const ALTERCOD_PRESENCE_TTL = 95;

/**
 * action=heartbeat — appelé par le launcher ET par le jeu.
 *
 * Le jeu envoie en plus map/gametype/mode/mod : ce sont ces champs qui
 * permettent à un ami de rejoindre directement le bon lobby au lieu d'un
 * simple "connect ip:port".
 */
function altercod_heartbeat(PDO $db, int $user_id): array
{
    $server   = trim((string)($_REQUEST['server']   ?? ''));
    $map      = trim((string)($_REQUEST['map']      ?? ''));
    $gametype = trim((string)($_REQUEST['gametype'] ?? ''));
    $mod_id   = trim((string)($_REQUEST['mod']      ?? ''));
    $mode     = (int)($_REQUEST['mode'] ?? 0);
    $status   = (int)($_REQUEST['status'] ?? 1);

    // On ne fait confiance à personne sur le format de l'adresse : ip:port,
    // sinon on la jette (elle finit dans un "connect" chez les autres joueurs).
    if ($server !== '' && !preg_match('/^\d{1,3}(\.\d{1,3}){3}:\d{1,5}$/', $server)) {
        $server = '';
    }

    // En partie seulement si on a vraiment une adresse joignable.
    $status = ($status === 2 && $server !== '') ? 2 : 1;
    if ($status !== 2) {
        $server = $map = $gametype = $mod_id = '';
        $mode = 0;
    }

    // Longueurs bornées pour coller aux colonnes.
    $map      = mb_substr($map, 0, 64);
    $gametype = mb_substr($gametype, 0, 64);
    $mod_id   = mb_substr($mod_id, 0, 64);

    $stmt = $db->prepare(
        'INSERT INTO altercod_presence
            (user_id, status, server, map, gametype, mode, mod_id, updated_at)
         VALUES (:uid, :st, :srv, :map, :gt, :mode, :mod, :now)
         ON DUPLICATE KEY UPDATE
            status = VALUES(status), server = VALUES(server),
            map = VALUES(map), gametype = VALUES(gametype),
            mode = VALUES(mode), mod_id = VALUES(mod_id),
            updated_at = VALUES(updated_at)'
    );
    $stmt->execute([
        ':uid'  => $user_id,
        ':st'   => $status,
        ':srv'  => $server,
        ':map'  => $map,
        ':gt'   => $gametype,
        ':mode' => $mode,
        ':mod'  => $mod_id,
        ':now'  => time(),
    ]);

    return ['ok' => true];
}

/**
 * À appeler depuis action=list, avec les ids d'amis déjà calculés.
 * Retourne  [user_id => ['status' => int, 'server' => string, 'game_info' => string]]
 *
 * game_info reprend exactement le format enrichi d'Ezz :
 *     addr|map|gametype|mode|mod
 * C'est ce que connect_to_friend() sait rejoindre côté client.
 */
function altercod_presence_for(PDO $db, array $friend_ids): array
{
    if (empty($friend_ids)) {
        return [];
    }

    $friend_ids = array_values(array_unique(array_map('intval', $friend_ids)));
    $holders = implode(',', array_fill(0, count($friend_ids), '?'));

    $stmt = $db->prepare(
        "SELECT user_id, status, server, map, gametype, mode, mod_id, updated_at
           FROM altercod_presence
          WHERE user_id IN ($holders)"
    );
    $stmt->execute($friend_ids);

    $now = time();
    $out = [];

    foreach ($stmt->fetchAll(PDO::FETCH_ASSOC) as $row) {
        $fresh = ($now - (int)$row['updated_at']) <= ALTERCOD_PRESENCE_TTL;
        if (!$fresh) {
            continue; // heartbeat périmé : l'ami est considéré hors ligne
        }

        $status = (int)$row['status'];
        $server = (string)$row['server'];

        $game_info = '';
        if ($status === 2 && $server !== '') {
            $game_info = implode('|', [
                $server,
                (string)$row['map'],
                (string)$row['gametype'],
                (string)(int)$row['mode'],
                (string)$row['mod_id'],
            ]);
        }

        $out[(int)$row['user_id']] = [
            'status'    => $status,
            'server'    => $status === 2 ? $server : '',
            'game_info' => $game_info,
        ];
    }

    return $out;
}

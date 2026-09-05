#pragma once

#include <cstdint>
#include <string>

// AlterCOD friends (IKAAM)
//
// Ezz builds its friend presence on Steam identities + the UDP rendezvous
// master servers (nat::refresh_friends). AlterBO3 accounts are forum accounts
// on ikaam.fr, so this component provides the same thing over our own HTTP
// API: the game announces where it is playing, polls the friend list, and
// feeds the result into the existing friends:: presence model. The in-game
// social menu, friends.json and the join path stay untouched.
namespace altercod_friends {
// Stable 64-bit id derived from a pseudo. MUST stay byte-for-byte identical
// to pseudoToLocalId() in data/launcher/main.js, otherwise the launcher and
// the game write two different entries for the same person.
uint64_t pseudo_to_id(std::string pseudo);

// True when this id belongs to an AlterCOD account we track. Used by
// friends::refresh_presence() so the Ezz rendezvous lookup does not mark our
// friends offline (they never answer it — they are not Steam users).
bool owns(uint64_t id);

// Enriched "addr|map|gametype|mode|mod" string for a friend, when our API
// reported one. Empty when the friend is offline or the backend only returns
// a plain address. Replaces steam_proxy::get_friend_rich_presence for our
// accounts.
std::string get_game_info(uint64_t id);

// Ask for an out-of-band refresh (menu opened, presence reset). Cheap and
// non-blocking: the work happens on the async scheduler.
void request_sync();

// True once the launcher has written a session file we could read.
bool is_logged_in();
} // namespace altercod_friends

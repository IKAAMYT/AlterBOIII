#include <std_include.hpp>
#include <loader/component_loader.hpp>

#include "altercod_friends.hpp"

#include "friends.hpp"
#include "game/game.hpp"
#include "game/utils.hpp"
#include "scheduler.hpp"
#include "workshop.hpp"

#include <utils/finally.hpp>
#include <utils/http.hpp>
#include <utils/io.hpp>
#include <utils/string.hpp>

#include <rapidjson/document.h>

#include <unordered_map>
#include <unordered_set>

namespace altercod_friends {
namespace {
constexpr const char *SESSION_FILE = "boiii_players/user/altercod_session.json";
constexpr const char *API = "https://ikaam.fr/amis/api.php";
constexpr auto HEARTBEAT_INTERVAL = 30s;
constexpr auto PRESENCE_INTERVAL = 30s;
constexpr uint32_t HTTP_TIMEOUT = 8;

// ── session ────────────────────────────────────────────────────────────────
std::mutex session_mutex;
std::string session_token;
std::string session_pseudo;

// ── local snapshot of what our API told us ─────────────────────────────────
std::mutex state_mutex;
std::unordered_set<uint64_t> known_ids;             // AlterCOD accounts
std::unordered_map<uint64_t, std::string> game_info; // id -> enriched string

// ── what the game thread last observed about us ────────────────────────────
struct self_presence {
  std::string address;
  std::string mapname;
  std::string gametype;
  int mode{};
  std::string mod_id;
};

std::mutex self_mutex;
self_presence self_state;

std::atomic_bool sync_running{};

std::string url_encode(const std::string &value) {
  static const char *hex = "0123456789ABCDEF";
  std::string out;
  out.reserve(value.size() * 3);
  for (const unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out.push_back(static_cast<char>(c));
    } else {
      out.push_back('%');
      out.push_back(hex[c >> 4]);
      out.push_back(hex[c & 0x0F]);
    }
  }
  return out;
}

bool read_session() {
  std::string data;
  if (!utils::io::read_file(SESSION_FILE, &data) || data.empty()) {
    std::lock_guard lock(session_mutex);
    session_token.clear();
    session_pseudo.clear();
    return false;
  }

  rapidjson::Document doc;
  if (doc.Parse(data.c_str()).HasParseError() || !doc.IsObject()) {
    return false;
  }

  std::string token;
  std::string pseudo;
  if (const auto it = doc.FindMember("token");
      it != doc.MemberEnd() && it->value.IsString()) {
    token = it->value.GetString();
  }
  if (const auto it = doc.FindMember("pseudo");
      it != doc.MemberEnd() && it->value.IsString()) {
    pseudo = it->value.GetString();
  }

  std::lock_guard lock(session_mutex);
  session_token = token;
  session_pseudo = pseudo;
  return !session_token.empty();
}

std::string get_token() {
  std::lock_guard lock(session_mutex);
  return session_token;
}

// Snapshot our own presence. MUST run on the main game thread: the address
// walk dereferences live client memory (*cl::clientConnections).
void refresh_self_presence() {
  self_presence next{};
  try {
    if (game::com::Com_IsInGame()) {
      next.address = friends::get_connect_address();
      next.mapname = std::string(game::get_mapname().value_or(""));
      next.gametype = std::string(game::get_g_gametype().value_or(""));
      next.mode = static_cast<int>(game::com::Com_SessionMode_GetMode());
      next.mod_id = workshop::get_mod_publisher_id();
    }
  } catch (...) {
    next = {};
  }

  std::lock_guard lock(self_mutex);
  self_state = std::move(next);
}

// ── heartbeat: tell the API where we are ───────────────────────────────────
void send_heartbeat() {
  const std::string token = get_token();
  if (token.empty()) {
    return; // not logged in through the launcher
  }

  self_presence self;
  {
    std::lock_guard lock(self_mutex);
    self = self_state;
  }

  std::string body = "action=heartbeat&token=" + url_encode(token) +
                     "&server=" + url_encode(self.address);

  // Extra fields so a friend can be joined straight into the right lobby,
  // exactly like Ezz's enriched rich-presence string. A backend that does not
  // know these keys simply ignores them.
  if (!self.address.empty()) {
    body += "&map=" + url_encode(self.mapname) +
            "&gametype=" + url_encode(self.gametype) +
            "&mode=" + std::to_string(self.mode) +
            "&mod=" + url_encode(self.mod_id) +
            "&status=2";
  } else {
    body += "&status=1";
  }

  utils::http::post_data(API, body, HTTP_TIMEOUT);
}

// ── presence: ask the API about our friends ────────────────────────────────
struct api_friend {
  uint64_t id{};
  std::string pseudo;
  int status{};
  std::string address;
  std::string game_info;
};

std::string read_string(const rapidjson::Value &object, const char *key) {
  const auto it = object.FindMember(key);
  if (it == object.MemberEnd()) {
    return {};
  }
  if (it->value.IsString()) {
    return it->value.GetString();
  }
  return {};
}

int read_int(const rapidjson::Value &object, const char *key) {
  const auto it = object.FindMember(key);
  if (it == object.MemberEnd()) {
    return 0;
  }
  if (it->value.IsInt()) {
    return it->value.GetInt();
  }
  if (it->value.IsString()) {
    return std::atoi(it->value.GetString());
  }
  return 0;
}

// Rebuild the enriched string the join path understands. When the backend
// only stores a bare address we still return it — connect_to_friend falls
// back to a raw connect in that case.
std::string build_game_info(const api_friend &entry) {
  if (entry.address.empty()) {
    return {};
  }
  if (!entry.game_info.empty()) {
    return entry.game_info;
  }
  return entry.address;
}

// nullopt = the request did not give us a usable answer (bad JSON, ok:false,
// expired token). An empty vector = the account genuinely has no friends.
// The difference matters: the first must never wipe the in-game list.
std::optional<std::vector<api_friend>>
parse_friend_list(const std::string &response) {
  std::vector<api_friend> result;

  rapidjson::Document doc;
  if (doc.Parse(response.c_str()).HasParseError() || !doc.IsObject()) {
    return std::nullopt;
  }

  const auto ok = doc.FindMember("ok");
  if (ok == doc.MemberEnd() || !ok->value.IsBool() || !ok->value.GetBool()) {
    return std::nullopt;
  }

  const auto list = doc.FindMember("friends");
  if (list == doc.MemberEnd() || !list->value.IsArray()) {
    return std::nullopt;
  }

  for (const auto &item : list->value.GetArray()) {
    if (!item.IsObject()) {
      continue;
    }

    api_friend entry{};
    entry.pseudo = read_string(item, "pseudo");
    if (entry.pseudo.empty()) {
      continue;
    }
    entry.id = pseudo_to_id(entry.pseudo);
    entry.status = read_int(item, "status");
    entry.address = read_string(item, "server");
    entry.game_info = read_string(item, "game_info");
    result.push_back(std::move(entry));
  }

  return result;
}

void apply_friend_list(const std::vector<api_friend> &entries) {
  std::unordered_set<uint64_t> ids;
  std::unordered_map<uint64_t, std::string> infos;
  ids.reserve(entries.size());

  // 1. the roster: our API is authoritative for AlterCOD accounts.
  for (const auto &entry : entries) {
    ids.insert(entry.id);
    if (!friends::is_friend(entry.id)) {
      friends::add_friend(entry.id, entry.pseudo);
    }
  }

  std::unordered_set<uint64_t> stale;
  {
    std::lock_guard lock(state_mutex);
    for (const uint64_t id : known_ids) {
      if (!ids.contains(id)) {
        stale.insert(id);
      }
    }
  }
  for (const uint64_t id : stale) {
    friends::remove_friend(id); // unfriended on the website
  }

  // 2. the presence, straight into the model the social menu already reads.
  for (const auto &entry : entries) {
    const std::string info = build_game_info(entry);
    if (entry.status == 2 && !entry.address.empty()) {
      // join_token stays empty: NAT punching is an Ezz rendezvous feature and
      // does not apply to our own network.
      friends::set_master_presence(entry.id, entry.address, {});
      if (!info.empty()) {
        infos[entry.id] = info;
      }
    } else {
      friends::clear_master_presence(entry.id);
    }
  }

  {
    std::lock_guard lock(state_mutex);
    known_ids = std::move(ids);
    game_info = std::move(infos);
  }

  friends::notify_presence_changed();
}

void sync_now() {
  if (sync_running.exchange(true)) {
    return; // one request in flight is enough
  }

  const auto _ = utils::finally([] { sync_running = false; });

  read_session();
  const std::string token = get_token();
  if (token.empty()) {
    return;
  }

  const auto response = utils::http::post_data(
      API, "action=list&token=" + url_encode(token), HTTP_TIMEOUT);
  if (!response.has_value() || response->empty()) {
    return; // offline: keep the last known state rather than blanking it
  }

  const auto parsed = parse_friend_list(*response);
  if (!parsed.has_value()) {
    return; // token expired or garbage answer: same, we keep what we have
  }

  apply_friend_list(*parsed);
}
} // namespace

uint64_t pseudo_to_id(std::string pseudo) {
  // djb2, mirrored from pseudoToLocalId() in main.js:
  //   h = 5381; h = ((h * 33) + charCodeAt(i)) % 900719925474; return '99' + h
  //
  // Two details decide whether the launcher and the game agree on an id, and
  // both bite on accented pseudos (Rémi, Léo, Noé...):
  //   - charCodeAt() yields UTF-16 code units, NOT the UTF-8 bytes the API
  //     sends us. "é" must hash as one unit 0x00E9, not as 0xC3 0xA9.
  //   - JS toLowerCase() is Unicode-aware; ::tolower() is not. "É" has to
  //     fold to "é", which CharLowerW does and the CRT does not.
  // Both sides stay below 2^53, so the JS double arithmetic is exact.
  std::wstring wide;
  if (!pseudo.empty()) {
    const int needed = MultiByteToWideChar(
        CP_UTF8, 0, pseudo.data(), static_cast<int>(pseudo.size()), nullptr, 0);
    if (needed > 0) {
      wide.resize(static_cast<size_t>(needed));
      MultiByteToWideChar(CP_UTF8, 0, pseudo.data(),
                          static_cast<int>(pseudo.size()), wide.data(), needed);
    } else {
      // Not valid UTF-8: fall back to a byte-wise widening so we still
      // produce a stable id instead of an empty one.
      wide.assign(pseudo.begin(), pseudo.end());
    }
  }

  if (!wide.empty()) {
    CharLowerW(wide.data()); // Unicode lowercase, like JS toLowerCase()
  }

  uint64_t hash = 5381;
  for (const wchar_t c : wide) {
    hash = ((hash * 33) + static_cast<uint16_t>(c)) % 900719925474ull;
  }

  return std::strtoull(("99" + std::to_string(hash)).c_str(), nullptr, 10);
}

bool owns(const uint64_t id) {
  std::lock_guard lock(state_mutex);
  return known_ids.contains(id);
}

std::string get_game_info(const uint64_t id) {
  std::lock_guard lock(state_mutex);
  const auto found = game_info.find(id);
  return found == game_info.end() ? std::string{} : found->second;
}

void request_sync() {
  scheduler::once([] { sync_now(); }, scheduler::async);
}

bool is_logged_in() { return !get_token().empty(); }

struct component final : client_component {
  void post_unpack() override {
    read_session();

    // Our own position is read on the game thread, never from the workers.
    scheduler::loop([] { refresh_self_presence(); }, scheduler::main, 5s);

    scheduler::once([] { sync_now(); }, scheduler::async, 8s);
    scheduler::loop([] { send_heartbeat(); }, scheduler::async,
                    HEARTBEAT_INTERVAL);
    scheduler::loop([] { sync_now(); }, scheduler::async, PRESENCE_INTERVAL);
  }
};
} // namespace altercod_friends

REGISTER_COMPONENT(altercod_friends::component)

#include "MyMesh.h"

#ifdef CACHE_INTERACTIVE_FEATURES
#include <helpers/CacheCommandHelpers.h>
#endif

#define REPLY_DELAY_MILLIS          1500
#define PUSH_NOTIFY_DELAY_MILLIS    2000
#define SYNC_PUSH_INTERVAL          1200

#define PUSH_ACK_TIMEOUT_FLOOD      12000
#define PUSH_TIMEOUT_BASE           4000
#define PUSH_ACK_TIMEOUT_FACTOR     2000

#define POST_SYNC_DELAY_SECS        6

#define FIRMWARE_VER_LEVEL       1

#define REQ_TYPE_GET_STATUS         0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE         0x02
#define REQ_TYPE_GET_TELEMETRY_DATA 0x03
#define REQ_TYPE_GET_ACCESS_LIST    0x05

#define RESP_SERVER_LOGIN_OK        0 // response to ANON_REQ

#define LAZY_CONTACTS_WRITE_DELAY    5000

struct ServerStats {
  uint16_t batt_milli_volts;
  uint16_t curr_tx_queue_len;
  int16_t noise_floor;
  int16_t last_rssi;
  uint32_t n_packets_recv;
  uint32_t n_packets_sent;
  uint32_t total_air_time_secs;
  uint32_t total_up_time_secs;
  uint32_t n_sent_flood, n_sent_direct;
  uint32_t n_recv_flood, n_recv_direct;
  uint16_t err_events; // was 'n_full_events'
  int16_t last_snr;    // x 4
  uint16_t n_direct_dups, n_flood_dups;
  uint16_t n_posted, n_post_push;
};

#ifdef CACHE_INTERACTIVE_FEATURES
static const char* CACHE_POST_FILE_A = "/cache_posts_a";
static const char* CACHE_POST_FILE_B = "/cache_posts_b";
static const char* CACHE_SETTINGS_FILE = "/cache_settings";
static const char* CACHE_VISITORS_FILE = "/cache_visitors";
static const char* CACHE_LIMIT_FILE = "/cache_limit";
static const uint32_t CACHE_POST_MAGIC = 0x43505354;  // CPST
static const uint32_t CACHE_SETTINGS_MAGIC = 0x43525353;  // CRSS

struct __attribute__((packed)) CachePostFileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
  uint32_t sequence;
  uint32_t checksum;
};

struct __attribute__((packed)) CachePostRecord {
  uint32_t timestamp;
  uint8_t author[PUB_KEY_SIZE];
  char text[MAX_POST_TEXT_LEN + 1];
  uint32_t checksum;
};

struct __attribute__((packed)) CacheSettingsRecord {
  uint32_t magic;
  uint16_t version;
  int8_t near_rssi;
  int8_t far_rssi;
  int8_t limit_rssi;
  uint8_t calibrated;
  uint32_t checksum;
};

struct __attribute__((packed)) CacheVisitorFileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t count;
  uint32_t checksum;
};

static const uint32_t CACHE_VISITOR_MAGIC = 0x43564953; // CVIS

uint32_t MyMesh::cacheChecksum(const uint8_t* data, size_t len) const {
  uint32_t hash = 2166136261UL;
  for (size_t i = 0; i < len; i++) {
    hash ^= data[i];
    hash *= 16777619UL;
  }
  return hash;
}

File MyMesh::openCacheWrite(const char* path) {
  _fs->remove(path);
#if defined(NRF52_PLATFORM)
  return _fs->open(path, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return _fs->open(path, "w");
#else
  return _fs->open(path, "w", true);
#endif
}

bool MyMesh::inspectCachePostFile(const char* path, uint32_t& sequence, uint16_t& count) {
  if (!_fs->exists(path)) return false;
  File file = _fs->open(path);
  if (!file) return false;

  CachePostFileHeader header;
  bool valid = file.read((uint8_t*)&header, sizeof(header)) == sizeof(header);
  valid = valid && header.magic == CACHE_POST_MAGIC && header.version == 1 && header.count <= MAX_UNSYNCED_POSTS;
  valid = valid && header.checksum == cacheChecksum((const uint8_t*)&header, sizeof(header) - sizeof(header.checksum));

  CachePostRecord record;
  for (uint16_t i = 0; valid && i < header.count; i++) {
    valid = file.read((uint8_t*)&record, sizeof(record)) == sizeof(record);
    valid = valid && record.checksum == cacheChecksum((const uint8_t*)&record, sizeof(record) - sizeof(record.checksum));
    valid = valid && record.timestamp != 0 && record.text[MAX_POST_TEXT_LEN] == 0;
  }
  file.close();
  if (!valid) return false;
  sequence = header.sequence;
  count = header.count;
  return true;
}

bool MyMesh::loadCachePostFile(const char* path) {
  File file = _fs->open(path);
  if (!file) return false;
  CachePostFileHeader header;
  if (file.read((uint8_t*)&header, sizeof(header)) != sizeof(header)) {
    file.close();
    return false;
  }

  memset(posts, 0, sizeof(posts));
  CachePostRecord record;
  for (uint16_t i = 0; i < header.count; i++) {
    if (file.read((uint8_t*)&record, sizeof(record)) != sizeof(record)) {
      file.close();
      memset(posts, 0, sizeof(posts));
      return false;
    }
    posts[i].post_timestamp = record.timestamp;
    posts[i].author = mesh::Identity(record.author);
    StrHelper::strncpy(posts[i].text, record.text, sizeof(posts[i].text));
  }
  file.close();
  next_post_idx = header.count % MAX_UNSYNCED_POSTS;
  cache_store_sequence = header.sequence;
  return true;
}

void MyMesh::loadCachePosts() {
  uint32_t seq_a = 0, seq_b = 0;
  uint16_t count_a = 0, count_b = 0;
  bool valid_a = inspectCachePostFile(CACHE_POST_FILE_A, seq_a, count_a);
  bool valid_b = inspectCachePostFile(CACHE_POST_FILE_B, seq_b, count_b);

  if (valid_b && (!valid_a || seq_b > seq_a)) {
    cache_store_is_b = true;
    loadCachePostFile(CACHE_POST_FILE_B);
  } else if (valid_a) {
    cache_store_is_b = false;
    loadCachePostFile(CACHE_POST_FILE_A);
  }
}

uint16_t MyMesh::cachePostCount() const {
  uint16_t count = 0;
  for (uint16_t i = 0; i < MAX_UNSYNCED_POSTS; i++) {
    if (posts[i].post_timestamp != 0) count++;
  }
  return count;
}

PostInfo* MyMesh::cachePostAt(uint16_t chronological_idx) {
  uint16_t count = cachePostCount();
  if (chronological_idx >= count) return NULL;
  uint16_t start = count == MAX_UNSYNCED_POSTS ? next_post_idx : 0;
  return &posts[(start + chronological_idx) % MAX_UNSYNCED_POSTS];
}

PostInfo* MyMesh::findCachePostByAuthor(const mesh::Identity& author) {
  for (uint16_t i = cachePostCount(); i > 0; i--) {
    PostInfo* post = cachePostAt(i - 1);
    if (post && post->author.matches(author)) return post;
  }
  return NULL;
}

bool MyMesh::replaceCachePost(ClientInfo* client, const char* replacement) {
  PostInfo* post = client ? findCachePostByAuthor(client->id) : NULL;
  if (!post || !replacement) return false;

  char previous[MAX_POST_TEXT_LEN + 1];
  StrHelper::strncpy(previous, post->text, sizeof(previous));
  char fallback[10];
  snprintf(post->text, sizeof(post->text), "%s: %s",
           cacheVisitorName(client->id, fallback), replacement);
  if (saveCachePosts()) return true;
  StrHelper::strncpy(post->text, previous, sizeof(post->text));
  return false;
}

void MyMesh::sendCachePrivateText(ClientInfo* client, const char* text) {
  if (!client || !text || !text[0]) return;
  uint8_t data[MAX_PACKET_PAYLOAD];
  uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
  memcpy(data, &timestamp, 4);
  data[4] = (TXT_TYPE_PLAIN << 2);
  size_t text_len = strlen(text);
  if (text_len > sizeof(data) - 5) text_len = sizeof(data) - 5;
  memcpy(&data[5], text, text_len);
  mesh::Packet* reply = createDatagram(PAYLOAD_TYPE_TXT_MSG, client->id,
                                       client->shared_secret, data, 5 + text_len);
  if (!reply) return;
  sendFloodScoped(default_scope, reply, 0, _prefs.path_hash_mode + 1);
}

void MyMesh::notifyFirstCacheAdmin(ClientInfo* finder, const char* finder_name) {
  if (!finder || finder->isAdmin()) return;
  ClientInfo* admin = NULL;
  for (int i = 0; i < acl.getNumClients(); i++) {
    ClientInfo* candidate = acl.getClientByIdx(i);
    if (candidate->isAdmin()) {
      admin = candidate;
      break;
    }
  }
  if (!admin) return;
  char message[MAX_POST_TEXT_LEN + 1];
  snprintf(message, sizeof(message), "Cache found by %s. Total finds: %u.",
           finder_name, (unsigned int)cachePostCount());
  sendCachePrivateText(admin, message);
}

bool MyMesh::saveCachePosts() {
  const char* next_path = cache_store_is_b ? CACHE_POST_FILE_A : CACHE_POST_FILE_B;
  File file = openCacheWrite(next_path);
  if (!file) return false;

  CachePostFileHeader header;
  header.magic = CACHE_POST_MAGIC;
  header.version = 1;
  header.count = cachePostCount();
  header.sequence = cache_store_sequence + 1;
  header.checksum = cacheChecksum((const uint8_t*)&header, sizeof(header) - sizeof(header.checksum));
  bool valid = file.write((const uint8_t*)&header, sizeof(header)) == sizeof(header);

  for (uint16_t i = 0; valid && i < header.count; i++) {
    PostInfo* post = cachePostAt(i);
    CachePostRecord record;
    memset(&record, 0, sizeof(record));
    record.timestamp = post->post_timestamp;
    memcpy(record.author, post->author.pub_key, PUB_KEY_SIZE);
    StrHelper::strncpy(record.text, post->text, sizeof(record.text));
    record.checksum = cacheChecksum((const uint8_t*)&record, sizeof(record) - sizeof(record.checksum));
    valid = file.write((const uint8_t*)&record, sizeof(record)) == sizeof(record);
  }
  file.close();
  if (valid) {
    cache_store_sequence = header.sequence;
    cache_store_is_b = !cache_store_is_b;
  }
  return valid;
}

void MyMesh::loadCacheSettings() {
  cache_rssi_near = cache_rssi_far = cache_rssi_limit = 0;
  cache_rssi_calibrated = false;
  if (!_fs->exists(CACHE_SETTINGS_FILE)) return;
  File file = _fs->open(CACHE_SETTINGS_FILE);
  if (!file) return;
  CacheSettingsRecord settings;
  bool valid = file.read((uint8_t*)&settings, sizeof(settings)) == sizeof(settings);
  file.close();
  valid = valid && settings.magic == CACHE_SETTINGS_MAGIC && settings.version == 1;
  valid = valid && settings.checksum == cacheChecksum((const uint8_t*)&settings, sizeof(settings) - sizeof(settings.checksum));
  if (valid) {
    cache_rssi_near = settings.near_rssi;
    cache_rssi_far = settings.far_rssi;
    cache_rssi_limit = settings.limit_rssi;
    cache_rssi_calibrated = settings.calibrated != 0;
  }
}

bool MyMesh::saveCacheSettings() {
  File file = openCacheWrite(CACHE_SETTINGS_FILE);
  if (!file) return false;
  CacheSettingsRecord settings;
  settings.magic = CACHE_SETTINGS_MAGIC;
  settings.version = 1;
  settings.near_rssi = cache_rssi_near;
  settings.far_rssi = cache_rssi_far;
  settings.limit_rssi = cache_rssi_limit;
  settings.calibrated = cache_rssi_calibrated ? 1 : 0;
  settings.checksum = cacheChecksum((const uint8_t*)&settings, sizeof(settings) - sizeof(settings.checksum));
  bool valid = file.write((const uint8_t*)&settings, sizeof(settings)) == sizeof(settings);
  file.close();
  return valid;
}

bool MyMesh::loadCacheVisitors() {
  cache_visitor_count = 0;
  memset(cache_visitors, 0, sizeof(cache_visitors));
  if (!_fs->exists(CACHE_VISITORS_FILE)) return true;
  File file = _fs->open(CACHE_VISITORS_FILE);
  if (!file) return false;
  CacheVisitorFileHeader header;
  bool ok = file.read((uint8_t*)&header, sizeof(header)) == sizeof(header);
  ok = ok && header.magic == CACHE_VISITOR_MAGIC && header.version == 1 && header.count <= MAX_CLIENTS;
  ok = ok && header.checksum == cacheChecksum((const uint8_t*)&header, sizeof(header) - sizeof(header.checksum));
  if (ok) ok = file.read((uint8_t*)cache_visitors, header.count * sizeof(CacheVisitor)) == header.count * sizeof(CacheVisitor);
  file.close();
  if (ok) cache_visitor_count = header.count;
  return ok;
}

bool MyMesh::saveCacheVisitors() {
  File file = openCacheWrite(CACHE_VISITORS_FILE);
  if (!file) return false;
  CacheVisitorFileHeader header = {CACHE_VISITOR_MAGIC, 1, cache_visitor_count, 0};
  header.checksum = cacheChecksum((const uint8_t*)&header, sizeof(header) - sizeof(header.checksum));
  bool ok = file.write((const uint8_t*)&header, sizeof(header)) == sizeof(header);
  if (ok) ok = file.write((const uint8_t*)cache_visitors, cache_visitor_count * sizeof(CacheVisitor)) == cache_visitor_count * sizeof(CacheVisitor);
  file.close();
  return ok;
}

void MyMesh::loadCachePostInterval() {
  cache_post_interval = 86400;
  if (!_fs->exists(CACHE_LIMIT_FILE)) return;
  File file = _fs->open(CACHE_LIMIT_FILE);
  if (!file) return;
  uint32_t stored[2];
  bool ok = file.read((uint8_t*)stored, sizeof(stored)) == sizeof(stored);
  file.close();
  if (ok && stored[1] == cacheChecksum((const uint8_t*)&stored[0], sizeof(stored[0]))) cache_post_interval = stored[0];
}

bool MyMesh::saveCachePostInterval() {
  File file = openCacheWrite(CACHE_LIMIT_FILE);
  if (!file) return false;
  uint32_t stored[2] = {cache_post_interval, cacheChecksum((const uint8_t*)&cache_post_interval, sizeof(cache_post_interval))};
  bool ok = file.write((const uint8_t*)stored, sizeof(stored)) == sizeof(stored);
  file.close();
  return ok;
}

MyMesh::CacheVisitor* MyMesh::findCacheVisitor(const uint8_t* pub_key, bool create) {
  for (uint8_t i = 0; i < cache_visitor_count; i++) if (memcmp(cache_visitors[i].pub_key, pub_key, PUB_KEY_SIZE) == 0) return &cache_visitors[i];
  if (!create || cache_visitor_count >= MAX_CLIENTS) return NULL;
  CacheVisitor* visitor = &cache_visitors[cache_visitor_count++];
  memset(visitor, 0, sizeof(*visitor));
  memcpy(visitor->pub_key, pub_key, PUB_KEY_SIZE);
  return visitor;
}

const char* MyMesh::cacheVisitorName(const mesh::Identity& id, char* fallback) {
  CacheVisitor* visitor = findCacheVisitor(id.pub_key, false);
  if (visitor && visitor->name[0]) return visitor->name;
  snprintf(fallback, 10, "%02x%02x", id.pub_key[0], id.pub_key[1]);
  return fallback;
}

bool MyMesh::clearCachePosts() {
  memset(posts, 0, sizeof(posts));
  next_post_idx = 0;
  _num_posted = 0;
  for (uint8_t i = 0; i < cache_visitor_count; i++) {
    cache_visitors[i].last_post = 0;
    cache_visitors[i].sync_since = 0;
  }
  saveCacheVisitors();
  return saveCachePosts();
}
#endif

void MyMesh::addPost(ClientInfo *client, const char *postData) {
#ifdef CACHE_INTERACTIVE_FEATURES
  char fallback[10], rendered[MAX_POST_TEXT_LEN + 1];
  const char* visitor_name = cacheVisitorName(client->id, fallback);
  snprintf(rendered, sizeof(rendered), "%s: %s", visitor_name, postData);
  storePost(client->id, rendered);
  CacheVisitor* visitor = findCacheVisitor(client->id.pub_key, true);
  if (visitor) { visitor->last_post = getRTCClock()->getCurrentTime(); saveCacheVisitors(); }
  notifyFirstCacheAdmin(client, visitor_name);
#else
  storePost(client->id, postData);
#endif
}

void MyMesh::addSystemPost(const char *postData) {
  if (!postData || postData[0] == 0) return;

  MESH_DEBUG_PRINTLN("room.post: addSystemPost: %s", postData);

  storePost(self_id, postData);
}

void MyMesh::storePost(const mesh::Identity &author, const char *postData) {
  int idx = next_post_idx;
  // TODO: suggested postData format: <title>/<descrption>
  posts[idx].author = author; // add to cyclic queue
  StrHelper::strncpy(posts[idx].text, postData, MAX_POST_TEXT_LEN);

  posts[idx].post_timestamp = getRTCClock()->getCurrentTimeUnique();
  MESH_DEBUG_PRINTLN("room.post: storePost idx=%d text=%s", idx, posts[idx].text);
  MESH_DEBUG_PRINTLN("room.post: timestamp=%u", posts[idx].post_timestamp);
  next_post_idx = (next_post_idx + 1) % MAX_UNSYNCED_POSTS;

#ifdef CACHE_INTERACTIVE_FEATURES
  saveCachePosts();
#else
  next_push = futureMillis(PUSH_NOTIFY_DELAY_MILLIS);
#endif
  _num_posted++; // stats
  MESH_DEBUG_PRINTLN("room.post: next_post_idx=%d num_posted=%d push scheduled", next_post_idx, _num_posted);
}

void MyMesh::pushPostToClient(ClientInfo *client, PostInfo &post) {
#ifdef CACHE_INTERACTIVE_FEATURES
  client->extra.room.cache_pending_advances_sync = 1;
#endif
  MESH_DEBUG_PRINTLN("room.post: pushPostToClient text=%s", post.text);
  int len = 0;
  memcpy(&reply_data[len], &post.post_timestamp, 4);
  len += 4; // this is a PAST timestamp... but should be accepted by client

  uint8_t attempt;
  getRNG()->random(&attempt, 1); // need this for re-tries, so packet hash (and ACK) will be different
  reply_data[len++] = (TXT_TYPE_SIGNED_PLAIN << 2) | (attempt & 3); // 'signed' plain text

  // encode prefix of post.author.pub_key
  #ifdef CACHE_INTERACTIVE_FEATURES
  memcpy(&reply_data[len], self_id.pub_key, 4);
  #else
  memcpy(&reply_data[len], post.author.pub_key, 4);
  #endif
  len += 4; // just first 4 bytes

  int text_len = strlen(post.text);
  memcpy(&reply_data[len], post.text, text_len);
  len += text_len;

  // calc expected ACK reply
  mesh::Utils::sha256((uint8_t *)&client->extra.room.pending_ack, 4, reply_data, len, client->id.pub_key, PUB_KEY_SIZE);
  client->extra.room.push_post_timestamp = post.post_timestamp;

  auto reply = createDatagram(PAYLOAD_TYPE_TXT_MSG, client->id, client->shared_secret, reply_data, len);
  if (reply) {
    if (client->out_path_len == OUT_PATH_UNKNOWN) {
      unsigned long delay_millis = 0;
      sendFloodScoped(default_scope, reply, delay_millis, _prefs.path_hash_mode + 1); // REVISIT
      client->extra.room.ack_timeout = futureMillis(PUSH_ACK_TIMEOUT_FLOOD);
    } else {
      sendDirect(reply, client->out_path, client->out_path_len);

      uint8_t path_hash_count = client->out_path_len & 63;
      client->extra.room.ack_timeout = futureMillis(PUSH_TIMEOUT_BASE + PUSH_ACK_TIMEOUT_FACTOR * (path_hash_count + 1));
    }
    _num_post_pushes++; // stats
  } else {
    client->extra.room.pending_ack = 0;
    MESH_DEBUG_PRINTLN("Unable to push post to client");
  }
}

uint8_t MyMesh::getUnsyncedCount(ClientInfo *client) {
  uint8_t count = 0;
  for (int k = 0; k < MAX_UNSYNCED_POSTS; k++) {
    if (posts[k].post_timestamp > client->extra.room.sync_since // is new post for this Client?
#ifdef CACHE_INTERACTIVE_FEATURES
        && posts[k].post_timestamp <= client->extra.room.cache_sync_until
#else
        && !posts[k].author.matches(client->id)   // don't push posts to the author
#endif
        ) {
      count++;
    }
  }
  return count;
}

bool MyMesh::processAck(const uint8_t *data) {
  for (int i = 0; i < acl.getNumClients(); i++) {
    auto client = acl.getClientByIdx(i);
    if (client->extra.room.pending_ack && memcmp(data, &client->extra.room.pending_ack, 4) == 0) { // got an ACK from Client!
      client->extra.room.pending_ack = 0; // clear this, so next push can happen
      client->extra.room.push_failures = 0;
#ifdef CACHE_INTERACTIVE_FEATURES
      if (client->extra.room.cache_pending_advances_sync) {
        client->extra.room.sync_since = client->extra.room.push_post_timestamp;
        CacheVisitor* visitor = findCacheVisitor(client->id.pub_key, true);
        if (visitor && client->extra.room.sync_since > visitor->sync_since) {
          visitor->sync_since = client->extra.room.sync_since;
          saveCacheVisitors();
        }
      }
      client->extra.room.cache_pending_advances_sync = 0;
#else
      client->extra.room.sync_since = client->extra.room.push_post_timestamp; // advance Client's SINCE timestamp, to sync next post
#endif
      return true;
    }
  }
  return false;
}

mesh::Packet *MyMesh::createSelfAdvert() {
  uint8_t app_data[MAX_ADVERT_DATA_SIZE];
  uint8_t app_data_len = _cli.buildAdvertData(ADV_TYPE_ROOM, app_data);

  return createAdvert(self_id, app_data, app_data_len);
}

File MyMesh::openAppend(const char *fname) {
#if defined(NRF52_PLATFORM)
  return _fs->open(fname, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return _fs->open(fname, "a");
#else
  return _fs->open(fname, "a", true);
#endif
}

#ifdef CACHE_INTERACTIVE_FEATURES
bool MyMesh::isCacheDirectPacket(const mesh::Packet* packet, const ClientInfo* client) const {
  if (packet->isRouteFlood()) return packet->getPathHashCount() == 0;
  if (!packet->isRouteDirect()) return false;
  if (client && client->out_path_len != OUT_PATH_UNKNOWN) {
    return (client->out_path_len & 63) == 0;
  }
  return packet->getPathHashCount() == 0;
}

bool MyMesh::passesCacheRssi() const {
  if (!cache_rssi_calibrated) return true;
  return (int)radio_driver.getLastRSSI() >= (int)cache_rssi_limit;
}

void MyMesh::configureCachePage(ClientInfo* client, uint16_t offset) {
  uint16_t count = cachePostCount();
  if (count == 0) {
    client->extra.room.sync_since = 0;
    client->extra.room.cache_sync_until = 0;
    client->extra.room.cache_page_offset = 0;
    return;
  }
  if (offset >= count) offset = count > 0 ? count - 1 : 0;
  uint16_t end = count > offset ? count - offset : 0;
  uint16_t start = end > 3 ? end - 3 : 0;
  PostInfo* first = cachePostAt(start);
  PostInfo* last = cachePostAt(end - 1);
  PostInfo* previous = start > 0 ? cachePostAt(start - 1) : NULL;
  client->extra.room.sync_since = previous ? previous->post_timestamp : 0;
  client->extra.room.cache_sync_until = last ? last->post_timestamp : 0;
  client->extra.room.cache_page_offset = offset;
  client->extra.room.pending_ack = 0;
  client->extra.room.push_failures = 0;
  (void)first;
  next_push = futureMillis(PUSH_NOTIFY_DELAY_MILLIS);
}

void MyMesh::configureCacheUnreadPage(ClientInfo* client, uint32_t sync_since) {
  client->extra.room.sync_since = sync_since;
  client->extra.room.cache_sync_until = sync_since;
  client->extra.room.cache_page_offset = 0;
  client->extra.room.pending_ack = 0;
  client->extra.room.push_failures = 0;
  uint8_t unread = 0;
  for (uint16_t i = 0, count = cachePostCount(); i < count && unread < 3; i++) {
    PostInfo* post = cachePostAt(i);
    if (post && post->post_timestamp > sync_since) {
      client->extra.room.cache_sync_until = post->post_timestamp;
      unread++;
    }
  }
  next_push = futureMillis(PUSH_NOTIFY_DELAY_MILLIS);
}

void MyMesh::pushCacheInstructions(ClientInfo* client) {
  PostInfo instructions;
  instructions.author = self_id;
  instructions.post_timestamp = getRTCClock()->getCurrentTimeUnique();
  snprintf(instructions.text, sizeof(instructions.text),
           "Welcome to %s. You may leave one log entry every 24 hours, up to 151 characters. Send !help for commands.",
           _prefs.node_name);
  pushPostToClient(client, instructions);
  client->extra.room.cache_pending_advances_sync = 0;
}

int8_t MyMesh::medianClientRssi(const ClientInfo* client) const {
  uint8_t count = client ? client->extra.room.cache_recent_rssi_count : 0;
  if (count == 0) return (int8_t)radio_driver.getLastRSSI();
  int8_t values[5];
  for (uint8_t i = 0; i < count; i++) values[i] = client->extra.room.cache_recent_rssi[i];
  for (uint8_t i = 1; i < count; i++) {
    int8_t value = values[i];
    int j = i - 1;
    while (j >= 0 && values[j] > value) {
      values[j + 1] = values[j];
      j--;
    }
    values[j + 1] = value;
  }
  return values[count / 2];
}

bool MyMesh::handleCachePageCommand(ClientInfo* client, const char* text, char* reply) {
  if (strcmp(text, "!help") == 0) {
    strcpy(reply, "Leave one log entry every 24h. Browse: !older !newer !latest. Edit yours: !edit <text>. Count finds: !found.");
    return true;
  }
  if (strcmp(text, "!found") == 0) {
    uint16_t count = cachePostCount();
    if (count == 0) strcpy(reply, "This cache has not been found yet.");
    else if (count == 1) strcpy(reply, "This cache has been found 1 time since its first find.");
    else snprintf(reply, MAX_POST_TEXT_LEN + 1,
                  "This cache has been found %u times since its first find.",
                  (unsigned int)count);
    return true;
  }

  PostInfo* existing = findCachePostByAuthor(client->id);
  const char* replacement = NULL;
  cache::EditCommandResult edit = cache::parseEditCommand(text, existing != NULL, &replacement);
  if (edit == cache::EDIT_MISSING_TEXT) {
    strcpy(reply, "Add your new log entry after !edit. Example: !edit Great cache!");
    return true;
  }
  if (edit == cache::EDIT_TOO_LONG) {
    strcpy(reply, "That replacement is too long. Log entries may contain at most 151 characters.");
    return true;
  }
  if (edit == cache::EDIT_NO_ENTRY) {
    strcpy(reply, "You do not have a recent log entry to edit.");
    return true;
  }
  if (edit == cache::EDIT_READY) {
    strcpy(reply, replaceCachePost(client, replacement)
                    ? "Your existing log entry was edited."
                    : "Unable to edit your log entry.");
    return true;
  }
  if (strcmp(text, "!older") == 0) {
    configureCachePage(client, client->extra.room.cache_page_offset + 3);
    return true;
  }
  if (strcmp(text, "!newer") == 0) {
    uint16_t offset = client->extra.room.cache_page_offset;
    configureCachePage(client, offset > 3 ? offset - 3 : 0);
    return true;
  }
  if (strcmp(text, "!latest") == 0) {
    configureCachePage(client, 0);
    return true;
  }
  return false;
}

bool MyMesh::handleCacheCLI(uint32_t sender_timestamp, ClientInfo* sender, const char* command, char* reply) {
  if (strcmp(command, "cache clear") == 0) {
    if (sender && !sender->isAdmin()) strcpy(reply, "ERR admin only");
    else strcpy(reply, clearCachePosts() ? "OK log memory cleared" : "ERR clear failed");
    return true;
  }
  if (strcmp(command, "cache limit") == 0) {
    if (cache_post_interval == 0) strcpy(reply, "Posting limit off");
    else snprintf(reply, MAX_POST_TEXT_LEN, "Posting limit: %lu hours", (unsigned long)(cache_post_interval / 3600));
    return true;
  }
  if (strcmp(command, "cache limit off") == 0) {
    cache_post_interval = 0;
    strcpy(reply, saveCachePostInterval() ? "OK posting limit off" : "ERR save failed");
    return true;
  }
  if (strncmp(command, "cache limit ", 12) == 0) {
    int hours = atoi(command + 12);
    if (hours < 1 || hours > 168) strcpy(reply, "ERR hours must be 1..168 or off");
    else {
      cache_post_interval = (uint32_t)hours * 3600UL;
      strcpy(reply, saveCachePostInterval() ? "OK" : "ERR save failed");
    }
    return true;
  }
  if (strcmp(command, "rssi") == 0) {
    if (cache_rssi_calibrated) {
      snprintf(reply, MAX_POST_TEXT_LEN, "Near %d | Far %d | Limit %d dBm",
               cache_rssi_near, cache_rssi_far, cache_rssi_limit);
    } else {
      strcpy(reply, "RSSI not calibrated. Use rssi near, then rssi far.");
    }
    return true;
  }
  if (strcmp(command, "rssi reset") == 0) {
    if (sender_timestamp != 0) {
      strcpy(reply, "ERR USB only");
    } else {
      cache_rssi_near = cache_rssi_far = cache_rssi_limit = 0;
      cache_rssi_calibrated = false;
      saveCacheSettings();
      strcpy(reply, "OK");
    }
    return true;
  }
  if (strcmp(command, "rssi near") == 0 || strcmp(command, "rssi far") == 0) {
    if (sender_timestamp == 0 || sender == NULL) {
      strcpy(reply, "ERR radio only");
      return true;
    }
    int8_t reading = medianClientRssi(sender);
    if (strcmp(command, "rssi near") == 0) {
      cache_rssi_near = reading;
      cache_rssi_calibrated = false;
      saveCacheSettings();
      snprintf(reply, MAX_POST_TEXT_LEN, "Near %d dBm. Move to far point.", reading);
    } else {
      if (cache_rssi_near == 0) {
        strcpy(reply, "ERR run rssi near first");
      } else {
        cache_rssi_far = reading;
        cache_rssi_limit = reading - 3;
        cache_rssi_calibrated = true;
        saveCacheSettings();
        snprintf(reply, MAX_POST_TEXT_LEN, "Far %d | Limit %d dBm", reading, cache_rssi_limit);
      }
    }
    return true;
  }
  return false;
}
#endif

int MyMesh::handleRequest(ClientInfo *sender, uint32_t sender_timestamp, uint8_t *payload,
                          size_t payload_len) {
  // uint32_t now = getRTCClock()->getCurrentTimeUnique();
  // memcpy(reply_data, &now, 4);   // response packets always prefixed with timestamp
  memcpy(reply_data, &sender_timestamp, 4); // reflect sender_timestamp back in response packet (kind of like a 'tag')

  if (payload[0] == REQ_TYPE_GET_STATUS) {
    ServerStats stats;
    stats.batt_milli_volts = board.getBattMilliVolts();
    stats.curr_tx_queue_len = _mgr->getOutboundTotal();
    stats.noise_floor = (int16_t)_radio->getNoiseFloor();
    stats.last_rssi = (int16_t)radio_driver.getLastRSSI();
    stats.n_packets_recv = radio_driver.getPacketsRecv();
    stats.n_packets_sent = radio_driver.getPacketsSent();
    stats.total_air_time_secs = getTotalAirTime() / 1000;
    stats.total_up_time_secs = uptime_millis / 1000;
    stats.n_sent_flood = getNumSentFlood();
    stats.n_sent_direct = getNumSentDirect();
    stats.n_recv_flood = getNumRecvFlood();
    stats.n_recv_direct = getNumRecvDirect();
    stats.err_events = _err_flags;
    stats.last_snr = (int16_t)(radio_driver.getLastSNR() * 4);
    stats.n_direct_dups = ((SimpleMeshTables *)getTables())->getNumDirectDups();
    stats.n_flood_dups = ((SimpleMeshTables *)getTables())->getNumFloodDups();
    stats.n_posted = _num_posted;
    stats.n_post_push = _num_post_pushes;

    memcpy(&reply_data[4], &stats, sizeof(stats));
    return 4 + sizeof(stats);
  }
  if (payload[0] == REQ_TYPE_GET_TELEMETRY_DATA) {
    uint8_t perm_mask = ~(payload[1]); // NEW: first reserved byte (of 4), is now inverse mask to apply to permissions

    telemetry.reset();
    telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
    // query other sensors -- target specific
    if ((sender->permissions & PERM_ACL_ROLE_MASK) == PERM_ACL_GUEST) {
      perm_mask = 0x00;  // just base telemetry allowed
    }
    sensors.querySensors(perm_mask, telemetry);

    // This default temperature will be overridden by external sensors (if any)
    float temperature = board.getMCUTemperature();
    if(!isnan(temperature)) { // Supported boards with built-in temperature sensor. ESP32-C3 may return NAN
      telemetry.addTemperature(TELEM_CHANNEL_SELF, temperature); // Built-in MCU Temperature
    }

    uint8_t tlen = telemetry.getSize();
    memcpy(&reply_data[4], telemetry.getBuffer(), tlen);
    return 4 + tlen; // reply_len
  }
  if (payload[0] == REQ_TYPE_GET_ACCESS_LIST && sender->isAdmin()) {
    uint8_t res1 = payload[1];   // reserved for future  (extra query params)
    uint8_t res2 = payload[2];
    if (res1 == 0 && res2 == 0) {
      uint8_t ofs = 4;
      for (int i = 0; i < acl.getNumClients() && ofs + 7 <= sizeof(reply_data) - 4; i++) {
        auto c = acl.getClientByIdx(i);
        if (!c->isAdmin()) continue;  // skip non-Admin entries
        memcpy(&reply_data[ofs], c->id.pub_key, 6); ofs += 6;  // just 6-byte pub_key prefix
        reply_data[ofs++] = c->permissions;
      }
      return ofs;
    }
  }
  return 0; // unknown command
}

void MyMesh::logRxRaw(float snr, float rssi, const uint8_t raw[], int len) {
#if MESH_PACKET_LOGGING
  Serial.print(getLogDateTime());
  Serial.print(" RAW: ");
  mesh::Utils::printHex(Serial, raw, len);
  Serial.println();
#endif
}

void MyMesh::logRx(mesh::Packet *pkt, int len, float score) {
  if (_logging) {
    File f = openAppend(PACKET_LOG_FILE);
    if (f) {
      f.print(getLogDateTime());
      f.printf(": RX, len=%d (type=%d, route=%s, payload_len=%d) SNR=%d RSSI=%d score=%d", len,
               pkt->getPayloadType(), pkt->isRouteDirect() ? "D" : "F", pkt->payload_len,
               (int)_radio->getLastSNR(), (int)_radio->getLastRSSI(), (int)(score * 1000));

      if (pkt->getPayloadType() == PAYLOAD_TYPE_PATH || pkt->getPayloadType() == PAYLOAD_TYPE_REQ ||
          pkt->getPayloadType() == PAYLOAD_TYPE_RESPONSE || pkt->getPayloadType() == PAYLOAD_TYPE_TXT_MSG) {
        f.printf(" [%02X -> %02X]\n", (uint32_t)pkt->payload[1], (uint32_t)pkt->payload[0]);
      } else {
        f.printf("\n");
      }
      f.close();
    }
  }
}
void MyMesh::logTx(mesh::Packet *pkt, int len) {
  if (_logging) {
    File f = openAppend(PACKET_LOG_FILE);
    if (f) {
      f.print(getLogDateTime());
      f.printf(": TX, len=%d (type=%d, route=%s, payload_len=%d)", len, pkt->getPayloadType(),
               pkt->isRouteDirect() ? "D" : "F", pkt->payload_len);

      if (pkt->getPayloadType() == PAYLOAD_TYPE_PATH || pkt->getPayloadType() == PAYLOAD_TYPE_REQ ||
          pkt->getPayloadType() == PAYLOAD_TYPE_RESPONSE || pkt->getPayloadType() == PAYLOAD_TYPE_TXT_MSG) {
        f.printf(" [%02X -> %02X]\n", (uint32_t)pkt->payload[1], (uint32_t)pkt->payload[0]);
      } else {
        f.printf("\n");
      }
      f.close();
    }
  }
}
void MyMesh::logTxFail(mesh::Packet *pkt, int len) {
  if (_logging) {
    File f = openAppend(PACKET_LOG_FILE);
    if (f) {
      f.print(getLogDateTime());
      f.printf(": TX FAIL!, len=%d (type=%d, route=%s, payload_len=%d)\n", len, pkt->getPayloadType(),
               pkt->isRouteDirect() ? "D" : "F", pkt->payload_len);
      f.close();
    }
  }
}

int MyMesh::calcRxDelay(float score, uint32_t air_time) const {
  if (_prefs.rx_delay_base <= 0.0f) return 0;
  return (int)((pow(_prefs.rx_delay_base, 0.85f - score) - 1.0) * air_time);
}

const char *MyMesh::getLogDateTime() {
  static char tmp[32];
  uint32_t now = getRTCClock()->getCurrentTime();
  DateTime dt = DateTime(now);
  sprintf(tmp, "%02d:%02d:%02d - %d/%d/%d U", dt.hour(), dt.minute(), dt.second(), dt.day(), dt.month(),
          dt.year());
  return tmp;
}

uint32_t MyMesh::getRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * _prefs.tx_delay_factor);
  return getRNG()->nextInt(0, 5*t + 1);
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * _prefs.direct_tx_delay_factor);
  return getRNG()->nextInt(0, 5*t + 1);
}

bool MyMesh::allowPacketForward(const mesh::Packet *packet) {
  if (_prefs.disable_fwd) return false;
  if (packet->isRouteFlood()
      && mesh::isFloodHopLimitExceeded(packet, _prefs.flood_max, _prefs.flood_max_unscoped, _prefs.flood_max_advert)) {
    return false;
  }
  return true;
}

mesh::DispatcherAction MyMesh::onRecvPacket(mesh::Packet* pkt) {
  if (pkt->getRouteType() == ROUTE_TYPE_TRANSPORT_FLOOD) {
    recv_pkt_region = region_map.findMatch(pkt, REGION_DENY_FLOOD);
  } else if (pkt->getRouteType() == ROUTE_TYPE_FLOOD) {
    if (region_map.getWildcard().flags & REGION_DENY_FLOOD) {
      recv_pkt_region = NULL;
    } else {
      recv_pkt_region =  &region_map.getWildcard();
    }
  } else {
    recv_pkt_region = NULL;
  }
  return Mesh::onRecvPacket(pkt);
}

void MyMesh::onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp,
                          const uint8_t* app_data, size_t app_data_len) {
#ifdef CACHE_INTERACTIVE_FEATURES
  AdvertDataParser parser(app_data, app_data_len);
  if (!parser.isValid() || parser.getType() != ADV_TYPE_CHAT) return;
  if (packet->getPathHashCount() != 0) return;
  CacheVisitor* visitor = findCacheVisitor(id.pub_key, true);
  if (visitor && parser.hasName()) {
    StrHelper::strncpy(visitor->name, parser.getName(), sizeof(visitor->name));
    saveCacheVisitors();
  }
  cache_advert_reply_at = futureMillis(5000);
#endif
}

void MyMesh::onAnonDataRecv(mesh::Packet *packet, const uint8_t *secret, const mesh::Identity &sender,
                            uint8_t *data, size_t len) {
#ifdef CACHE_INTERACTIVE_FEATURES
  if (!isCacheDirectPacket(packet) || !passesCacheRssi()) return;
#endif
  if (packet->getPayloadType() == PAYLOAD_TYPE_ANON_REQ) { // received an initial request by a possible admin
                                                           // client (unknown at this stage)
    uint32_t sender_timestamp, sender_sync_since;
    memcpy(&sender_timestamp, data, 4);
    memcpy(&sender_sync_since, &data[4], 4); // sender's "sync messags SINCE x" timestamp

    data[len] = 0;                                        // ensure null terminator

    CacheVisitor* visitor_record = findCacheVisitor(sender.pub_key, true);
    ClientInfo* client = NULL;
    bool returning_client = visitor_record && visitor_record->sync_since != 0;
    if (data[8] == 0) {   // blank password, just check if sender is in ACL
      client = acl.getClient(sender.pub_key, PUB_KEY_SIZE);
      if (client == NULL) {
      #if MESH_DEBUG
        MESH_DEBUG_PRINTLN("Login, sender not in ACL");
      #endif
      }
    }
    if (client == NULL) {
      uint8_t perm;
      if (strcmp((char *)&data[8], _prefs.password) == 0) { // check for valid admin password
        perm = PERM_ACL_ADMIN;
      } else {
        if (strcmp((char *)&data[8], _prefs.guest_password) == 0) {   // check the room/public password
          perm = PERM_ACL_READ_WRITE;
        } else if (_prefs.allow_read_only) {
          perm = PERM_ACL_GUEST;
        } else {
          MESH_DEBUG_PRINTLN("Incorrect room password");
          return; // no response. Client will timeout
        }
      }

      client = acl.putClient(sender, 0);  // add to known clients (if not already known)
      if (sender_timestamp <= client->last_timestamp) {
        MESH_DEBUG_PRINTLN("possible replay attack!");
        return;
      }

      MESH_DEBUG_PRINTLN("Login success!");
      client->last_timestamp = sender_timestamp;
      client->extra.room.sync_since = sender_sync_since;
      client->extra.room.pending_ack = 0;
      client->extra.room.push_failures = 0;

      client->last_activity = getRTCClock()->getCurrentTime();
      client->permissions &= ~0x03;
      client->permissions |= perm;
      memcpy(client->shared_secret, secret, PUB_KEY_SIZE);

      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
    }

    if (packet->isRouteFlood()) {
      client->out_path_len = OUT_PATH_UNKNOWN;  // need to rediscover out_path
    }

#ifdef CACHE_INTERACTIVE_FEATURES
    memset(client->extra.room.cache_recent_rssi, 0, sizeof(client->extra.room.cache_recent_rssi));
    client->extra.room.cache_recent_rssi[0] = (int8_t)radio_driver.getLastRSSI();
    client->extra.room.cache_recent_rssi_count = 1;
    client->extra.room.cache_recent_rssi_next = 1;
    uint32_t delivered_since = visitor_record ? visitor_record->sync_since : 0;
    if (sender_sync_since > delivered_since) delivered_since = sender_sync_since;
    if (returning_client || sender_sync_since != 0) configureCacheUnreadPage(client, delivered_since);
    else configureCachePage(client, 0);
    client->extra.room.cache_intro_pending = (returning_client || sender_sync_since != 0) ? 0 : 1;
    saveCacheVisitors();
#endif

    uint32_t now = getRTCClock()->getCurrentTimeUnique();
    memcpy(reply_data, &now, 4); // response packets always prefixed with timestamp
    // TODO: maybe reply with count of messages waiting to be synced for THIS client?
    reply_data[4] = RESP_SERVER_LOGIN_OK;
    reply_data[5] = 0; // Legacy: was recommended keep-alive interval (secs / 16)
    reply_data[6] = (client->isAdmin() ? 1 : (client->permissions == 0 ? 2 : 0));
    // LEGACY: reply_data[7] = getUnsyncedCount(client);
    reply_data[7] = client->permissions; // NEW
    getRNG()->random(&reply_data[8], 4);   // random blob to help packet-hash uniqueness
    reply_data[12] = FIRMWARE_VER_LEVEL;  // New field

    next_push = futureMillis(PUSH_NOTIFY_DELAY_MILLIS); // delay next push, give RESPONSE packet time to arrive first

    if (packet->isRouteFlood()) {
      // let this sender know path TO here, so they can use sendDirect(), and ALSO encode the response
      mesh::Packet *path = createPathReturn(sender, client->shared_secret, packet->path, packet->path_len,
                                            PAYLOAD_TYPE_RESPONSE, reply_data, 13);
      if (path) sendFloodReply(path, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
    } else {
      mesh::Packet *reply = createDatagram(PAYLOAD_TYPE_RESPONSE, sender, client->shared_secret, reply_data, 13);
      if (reply) {
        if (client->out_path_len != OUT_PATH_UNKNOWN) { // we have an out_path, so send DIRECT
          sendDirect(reply, client->out_path, client->out_path_len, SERVER_RESPONSE_DELAY);
        } else {
          sendFloodReply(reply, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
        }
      }
    }
  }
}

int MyMesh::searchPeersByHash(const uint8_t *hash) {
  int n = 0;
  for (int i = 0; i < acl.getNumClients(); i++) {
    if (acl.getClientByIdx(i)->id.isHashMatch(hash)) {
      matching_peer_indexes[n++] = i; // store the INDEXES of matching contacts (for subsequent 'peer' methods)
    }
  }
  return n;
}

void MyMesh::getPeerSharedSecret(uint8_t *dest_secret, int peer_idx) {
  int i = matching_peer_indexes[peer_idx];
  if (i >= 0 && i < acl.getNumClients()) {
    // lookup pre-calculated shared_secret
    memcpy(dest_secret, acl.getClientByIdx(i)->shared_secret, PUB_KEY_SIZE);
  } else {
    MESH_DEBUG_PRINTLN("getPeerSharedSecret: Invalid peer idx: %d", i);
  }
}

void MyMesh::onPeerDataRecv(mesh::Packet *packet, uint8_t type, int sender_idx, const uint8_t *secret,
                            uint8_t *data, size_t len) {
  int i = matching_peer_indexes[sender_idx];
  if (i < 0 || i >= acl.getNumClients()) { // get from our known_clients table (sender SHOULD already be known in this context)
    MESH_DEBUG_PRINTLN("onPeerDataRecv: invalid peer idx: %d", i);
    return;
  }
  auto client = acl.getClientByIdx(i);
#ifdef CACHE_INTERACTIVE_FEATURES
  if (!isCacheDirectPacket(packet, client)) return;
  uint8_t rssi_idx = client->extra.room.cache_recent_rssi_next % 5;
  client->extra.room.cache_recent_rssi[rssi_idx] = (int8_t)radio_driver.getLastRSSI();
  client->extra.room.cache_recent_rssi_next = (rssi_idx + 1) % 5;
  if (client->extra.room.cache_recent_rssi_count < 5) client->extra.room.cache_recent_rssi_count++;
#endif
  if (type == PAYLOAD_TYPE_TXT_MSG && len > 5) { // a CLI command or new Post
    uint32_t sender_timestamp;
    memcpy(&sender_timestamp, data, 4); // timestamp (by sender's RTC clock - which could be wrong)
    uint8_t flags = (data[4] >> 2);        // message attempt number, and other flags

    if (!(flags == TXT_TYPE_PLAIN || flags == TXT_TYPE_CLI_DATA)) {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: unsupported command flags received: flags=%02x", (uint32_t)flags);
    } else if (sender_timestamp >= client->last_timestamp) { // prevent replay attacks, but send Acks for retries
      bool is_retry = (sender_timestamp == client->last_timestamp);
      client->last_timestamp = sender_timestamp;

      uint32_t now = getRTCClock()->getCurrentTimeUnique();
      client->last_activity = now;
      client->extra.room.push_failures = 0; // reset so push can resume (if prev failed)

      // len can be > original length, but 'text' will be padded with zeroes
      data[len] = 0; // need to make a C string again, with null terminator

      uint32_t ack_hash; // calc truncated hash of the message timestamp + text + sender pub_key, to prove to
                         // sender that we got it
      mesh::Utils::sha256((uint8_t *)&ack_hash, 4, data, 5 + strlen((char *)&data[5]), client->id.pub_key,
                          PUB_KEY_SIZE);

      uint8_t temp[166];
      temp[5] = 0;
      bool send_ack;
      if (flags == TXT_TYPE_CLI_DATA) {
        if (client->isAdmin()) {
          if (is_retry) {
            temp[5] = 0; // no reply
          } else {
#ifdef CACHE_INTERACTIVE_FEATURES
            cache_cli_sender = client;
#endif
            handleCommand(sender_timestamp, (char *)&data[5], (char *)&temp[5]);
#ifdef CACHE_INTERACTIVE_FEATURES
            cache_cli_sender = NULL;
#endif
            temp[4] = (TXT_TYPE_CLI_DATA << 2); // attempt and flags,  (NOTE: legacy was: TXT_TYPE_PLAIN)
          }
          send_ack = false;
        } else {
          temp[5] = 0;      // no reply
          send_ack = false; // and no ACK...  user shoudn't be sending these
        }
      } else { // TXT_TYPE_PLAIN
        if ((client->permissions & PERM_ACL_ROLE_MASK) == PERM_ACL_GUEST) {
          temp[5] = 0;      // no reply
          send_ack = false; // no ACK
        } else {
          if (!is_retry) {
#ifdef CACHE_INTERACTIVE_FEATURES
            temp[4] = (TXT_TYPE_PLAIN << 2);
            if (!handleCachePageCommand(client, (const char *)&data[5], (char *)&temp[5])) {
              CacheVisitor* visitor = findCacheVisitor(client->id.pub_key, true);
              uint32_t elapsed = visitor && visitor->last_post && now > visitor->last_post ? now - visitor->last_post : cache_post_interval;
              if (!client->isAdmin() && cache_post_interval && visitor && visitor->last_post && elapsed < cache_post_interval) {
                strcpy((char *)&temp[5], "You already left a log entry in the last 24 hours. To edit it, send !edit <new log entry>.");
              } else {
                addPost(client, (const char *)&data[5]);
              }
            }
#else
            addPost(client, (const char *)&data[5]);
#endif
          }
          send_ack = true;
        }
      }

      uint32_t delay_millis;
      if (send_ack) {
        if (client->out_path_len == OUT_PATH_UNKNOWN) {
          mesh::Packet *ack = createAck(ack_hash);
          if (ack) sendFloodReply(ack, TXT_ACK_DELAY, packet->getPathHashSize());
          delay_millis = TXT_ACK_DELAY + REPLY_DELAY_MILLIS;
        } else {
          uint32_t d = TXT_ACK_DELAY;
          if (getExtraAckTransmitCount() > 0) {
            mesh::Packet *a1 = createMultiAck(ack_hash, 1);
            if (a1) sendDirect(a1, client->out_path, client->out_path_len, d);
            d += 300;
          }

          mesh::Packet *a2 = createAck(ack_hash);
          if (a2) sendDirect(a2, client->out_path, client->out_path_len, d);
          delay_millis = d + REPLY_DELAY_MILLIS;
        }
      } else {
        delay_millis = 0;
      }

      int text_len = strlen((char *)&temp[5]);
      if (text_len > 0) {
        if (now == sender_timestamp) {
          // WORKAROUND: the two timestamps need to be different, in the CLI view
          now++;
        }
        memcpy(temp, &now, 4); // mostly an extra blob to help make packet_hash unique

        // calc expected ACK reply
        // mesh::Utils::sha256((uint8_t *)&expected_ack_crc, 4, temp, 5 + text_len, self_id.pub_key,
        // PUB_KEY_SIZE);

        auto reply = createDatagram(PAYLOAD_TYPE_TXT_MSG, client->id, secret, temp, 5 + text_len);
        if (reply) {
          if (client->out_path_len == OUT_PATH_UNKNOWN) {
            sendFloodReply(reply, delay_millis + SERVER_RESPONSE_DELAY, packet->getPathHashSize());
          } else {
            sendDirect(reply, client->out_path, client->out_path_len, delay_millis + SERVER_RESPONSE_DELAY);
          }
        }
      }
    } else {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: possible replay attack detected");
    }
  } else if (type == PAYLOAD_TYPE_REQ && len >= 5) {
    uint32_t sender_timestamp;
    memcpy(&sender_timestamp, data, 4); // timestamp (by sender's RTC clock - which could be wrong)
    if (sender_timestamp < client->last_timestamp) { // prevent replay attacks
      MESH_DEBUG_PRINTLN("onPeerDataRecv: possible replay attack detected");
    } else {
      client->last_timestamp = sender_timestamp;

      uint32_t now = getRTCClock()->getCurrentTime();
      client->last_activity = now; // <-- THIS will keep client connection alive
      client->extra.room.push_failures = 0;   // reset so push can resume (if prev failed)

      if (data[4] == REQ_TYPE_KEEP_ALIVE && packet->isRouteDirect()) { // request type
        uint32_t forceSince = 0;
        if (len >= 9) {                     // optional - last post_timestamp client received
          memcpy(&forceSince, &data[5], 4); // NOTE: this may be 0, if part of decrypted PADDING!
        } else {
          memcpy(&data[5], &forceSince, 4); // make sure there are zeroes in payload (for ack_hash calc below)
        }
        if (forceSince > 0) {
          client->extra.room.sync_since = forceSince; // force-update the 'sync since'
        }

        client->extra.room.pending_ack = 0;

        // TODO: Throttle KEEP_ALIVE requests!
        // if client sends too quickly, evict()

        // RULE: only send keep_alive response DIRECT!
        if (client->out_path_len != OUT_PATH_UNKNOWN) {
          uint32_t ack_hash; // calc ACK to prove to sender that we got request
          mesh::Utils::sha256((uint8_t *)&ack_hash, 4, data, 9, client->id.pub_key, PUB_KEY_SIZE);

          auto reply = createAck(ack_hash);
          if (reply) {
            reply->payload[reply->payload_len++] = getUnsyncedCount(client); // NEW: add unsynced counter to end of ACK packet
            sendDirect(reply, client->out_path, client->out_path_len, SERVER_RESPONSE_DELAY);
          }
        }
      } else {
        int reply_len = handleRequest(client, sender_timestamp, &data[4], len - 4);
        if (reply_len > 0) { // valid command
          if (packet->isRouteFlood()) {
            // let this sender know path TO here, so they can use sendDirect(), and ALSO encode the response
            mesh::Packet *path = createPathReturn(client->id, secret, packet->path, packet->path_len,
                                                  PAYLOAD_TYPE_RESPONSE, reply_data, reply_len);
            if (path) sendFloodReply(path, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
          } else {
            mesh::Packet *reply = createDatagram(PAYLOAD_TYPE_RESPONSE, client->id, secret, reply_data, reply_len);
            if (reply) {
              if (client->out_path_len != OUT_PATH_UNKNOWN) { // we have an out_path, so send DIRECT
                sendDirect(reply, client->out_path, client->out_path_len, SERVER_RESPONSE_DELAY);
              } else {
                sendFloodReply(reply, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
              }
            }
          }
        }
      }
    }
  }
}

bool MyMesh::onPeerPathRecv(mesh::Packet *packet, int sender_idx, const uint8_t *secret, uint8_t *path,
                            uint8_t path_len, uint8_t extra_type, uint8_t *extra, uint8_t extra_len) {
  // TODO: prevent replay attacks
  int i = matching_peer_indexes[sender_idx];

  if (i >= 0 && i < acl.getNumClients()) { // get from our known_clients table (sender SHOULD already be known in this context)
#ifdef CACHE_INTERACTIVE_FEATURES
    if (!isCacheDirectPacket(packet) || (path_len & 63) != 0) return false;
#endif
    MESH_DEBUG_PRINTLN("PATH to client, path_len=%d", (uint32_t)path_len);
    auto client = acl.getClientByIdx(i);
    client->out_path_len = mesh::Packet::copyPath(client->out_path, path, path_len); // store a copy of path, for sendDirect()
    client->last_activity = getRTCClock()->getCurrentTime();
  } else {
    MESH_DEBUG_PRINTLN("onPeerPathRecv: invalid peer idx: %d", i);
  }

  if (extra_type == PAYLOAD_TYPE_ACK && extra_len >= 4) {
    // also got an encoded ACK!
    processAck(extra);
  }

  // NOTE: no reciprocal path send!!
  return false;
}

void MyMesh::onAckRecv(mesh::Packet *packet, uint32_t ack_crc) {
  if (processAck((uint8_t *)&ack_crc)) {
    packet->markDoNotRetransmit(); // ACK was for this node, so don't retransmit
  }
}

MyMesh::MyMesh(mesh::MainBoard &board, mesh::Radio &radio, mesh::MillisecondClock &ms, mesh::RNG &rng,
               mesh::RTCClock &rtc, mesh::MeshTables &tables)
    : mesh::Mesh(radio, ms, rng, rtc, *new StaticPoolPacketManager(32), tables),
      region_map(key_store), temp_map(key_store),
      _cli(board, rtc, sensors, region_map, acl, &_prefs, this),
      telemetry(MAX_PACKET_PAYLOAD - 4)
{
  last_millis = 0;
  uptime_millis = 0;
  next_local_advert = next_flood_advert = 0;
  dirty_contacts_expiry = 0;
  _logging = false;
  region_load_active = false;
  set_radio_at = revert_radio_at = 0;
  recv_pkt_region = NULL;

  // defaults
  _prefs.airtime_factor = 1.0;
  _prefs.rx_delay_base = 0.0f;   // off by default, was 10.0
  _prefs.tx_delay_factor = 0.5f; // was 0.25f;
  _prefs.direct_tx_delay_factor = 0.2f; // was zero
  StrHelper::strncpy(_prefs.node_name, ADVERT_NAME, sizeof(_prefs.node_name));
  _prefs.node_lat = ADVERT_LAT;
  _prefs.node_lon = ADVERT_LON;
  StrHelper::strncpy(_prefs.password, ADMIN_PASSWORD, sizeof(_prefs.password));
  _prefs.freq = LORA_FREQ;
  _prefs.sf = LORA_SF;
  _prefs.bw = LORA_BW;
  _prefs.cr = LORA_CR;
  _prefs.tx_power_dbm = LORA_TX_POWER;
  _prefs.disable_fwd = 1;
  _prefs.advert_interval = 1;        // default to 2 minutes for NEW installs
  _prefs.flood_advert_interval = 47; // 47 hours
  _prefs.flood_max = 64;
  _prefs.flood_max_unscoped = 64;
  _prefs.flood_max_advert = 8;
  _prefs.interference_threshold = 0; // disabled
  _prefs.cad_enabled = 0;            // hardware CAD before TX (off by default; 'set cad on')
#ifdef ROOM_PASSWORD
  StrHelper::strncpy(_prefs.guest_password, ROOM_PASSWORD, sizeof(_prefs.guest_password));
#endif

  // GPS defaults
  _prefs.gps_enabled = 0;
  _prefs.gps_interval = 0;
  _prefs.advert_loc_policy = ADVERT_LOC_PREFS;

#if defined(USE_SX1262) || defined(USE_SX1268)
#ifdef SX126X_RX_BOOSTED_GAIN
  _prefs.rx_boosted_gain = SX126X_RX_BOOSTED_GAIN;
#else
  _prefs.rx_boosted_gain = 1; // enabled by default;
#endif
#endif
  _prefs.radio_fem_rxgain = 1;
  _prefs.radio_fem_txgain = 0;

  next_post_idx = 0;
  next_client_idx = 0;
  next_push = 0;
  memset(posts, 0, sizeof(posts));
  _num_posted = _num_post_pushes = 0;

#ifdef CACHE_INTERACTIVE_FEATURES
  cache_store_sequence = 0;
  cache_store_is_b = false;
  cache_rssi_near = cache_rssi_far = cache_rssi_limit = 0;
  cache_rssi_calibrated = false;
  cache_cli_sender = NULL;
  cache_visitor_count = 0;
  memset(cache_visitors, 0, sizeof(cache_visitors));
  cache_advert_reply_at = 0;
  cache_post_interval = 86400;
#endif

  memset(default_scope.key, 0, sizeof(default_scope.key));
}

void MyMesh::begin(FILESYSTEM *fs) {
  mesh::Mesh::begin();
  _fs = fs;
  // load persisted prefs
  _cli.loadPrefs(_fs);

#ifdef CACHE_INTERACTIVE_FEATURES
  loadCachePosts();
  loadCacheSettings();
  loadCacheVisitors();
  loadCachePostInterval();
#endif

  acl.load(_fs, self_id);
  region_map.load(_fs);

  // establish default-scope
  {
    RegionEntry* r = region_map.getDefaultRegion();
    if (r) {
      region_map.getTransportKeysFor(*r, &default_scope, 1);
    } else {
#ifdef DEFAULT_FLOOD_SCOPE_NAME
      r = region_map.findByName(DEFAULT_FLOOD_SCOPE_NAME);
      if (r == NULL) {
        r = region_map.putRegion(DEFAULT_FLOOD_SCOPE_NAME, 0);  // auto-create the default scope region
        if (r) { r->flags = 0; }   // Allow-flood
      }
      if (r) {
        region_map.setDefaultRegion(r);
        region_map.getTransportKeysFor(*r, &default_scope, 1);
      }
#endif
    }
  }

  radio_driver.setParams(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
  radio_driver.setTxPower(_prefs.tx_power_dbm);
  radio_driver.setRxBoostedGainMode(_prefs.rx_boosted_gain);
  board.setLoRaFemLnaEnabled(_prefs.radio_fem_rxgain);
  board.setLoRaFemPaGainEnabled(_prefs.radio_fem_txgain);

  updateAdvertTimer();
  updateFloodAdvertTimer();

  board.setAdcMultiplier(_prefs.adc_multiplier);

#if ENV_INCLUDE_GPS == 1
  applyGpsPrefs();
#endif
}

void MyMesh::sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis, uint8_t path_hash_size) {
  if (scope.isNull()) {
    sendFlood(pkt, delay_millis, path_hash_size);
  } else {
    uint16_t codes[2];
    codes[0] = scope.calcTransportCode(pkt);
    codes[1] = 0;  // REVISIT: set to 'home' Region, for sender/return region?
    sendFlood(pkt, codes, delay_millis, path_hash_size);
  }
}

void MyMesh::sendFloodReply(mesh::Packet* packet, unsigned long delay_millis, uint8_t path_hash_size) {
  TransportKey req_scope;
  bool is_wildcard = recv_pkt_region != NULL && recv_pkt_region->isWildcard();
  bool req_scope_known = recv_pkt_region != NULL && !is_wildcard
                      && region_map.getTransportKeysFor(*recv_pkt_region, &req_scope, 1) > 0;

  switch (mesh::chooseReplyScope(req_scope_known, is_wildcard, !default_scope.isNull())) {
    case mesh::REPLY_SCOPE_REQUEST:
      sendFloodScoped(req_scope, packet, delay_millis, path_hash_size);   // reply with same scope as request
      break;
    case mesh::REPLY_SCOPE_DEFAULT:
      // requester's scope is unknown: DIRECT request (no transport codes), or code matched no Region.
      // un-scoped would be dropped at hop 0 by repeaters running flood.max.unscoped=0
      sendFloodScoped(default_scope, packet, delay_millis, path_hash_size);
      break;
    case mesh::REPLY_SCOPE_NONE:
      sendFlood(packet, delay_millis, path_hash_size);   // send un-scoped
      break;
  }
}

void MyMesh::applyTempRadioParams(float freq, float bw, uint8_t sf, uint8_t cr, int timeout_mins) {
  set_radio_at = futureMillis(2000); // give CLI reply some time to be sent back, before applying temp radio params
  pending_freq = freq;
  pending_bw = bw;
  pending_sf = sf;
  pending_cr = cr;

  revert_radio_at = futureMillis(2000 + timeout_mins * 60 * 1000); // schedule when to revert radio params
}

bool MyMesh::formatFileSystem() {
#if defined(NRF52_PLATFORM)
  return InternalFS.format();
#elif defined(RP2040_PLATFORM)
  return LittleFS.format();
#elif defined(ESP32)
  return SPIFFS.format();
#else
#error "need to implement file system erase"
  return false;
#endif
}

void MyMesh::sendSelfAdvertisement(int delay_millis, bool flood) {
  mesh::Packet *pkt = createSelfAdvert();
  if (pkt) {
    if (flood) {
      sendFloodScoped(default_scope, pkt, delay_millis, _prefs.path_hash_mode + 1);
    } else {
      sendZeroHop(pkt, delay_millis);
    }
  } else {
    MESH_DEBUG_PRINTLN("ERROR: unable to create advertisement packet!");
  }
}

void MyMesh::updateAdvertTimer() {
  if (_prefs.advert_interval > 0) { // schedule local advert timer
    next_local_advert = futureMillis((uint32_t)_prefs.advert_interval * 2 * 60 * 1000);
  } else {
    next_local_advert = 0; // stop the timer
  }
}
void MyMesh::updateFloodAdvertTimer() {
  if (_prefs.flood_advert_interval > 0) { // schedule flood advert timer
    next_flood_advert = futureMillis(((uint32_t)_prefs.flood_advert_interval) * 60 * 60 * 1000);
  } else {
    next_flood_advert = 0; // stop the timer
  }
}

void MyMesh::dumpLogFile() {
#if defined(RP2040_PLATFORM)
  File f = _fs->open(PACKET_LOG_FILE, "r");
#else
  File f = _fs->open(PACKET_LOG_FILE);
#endif
  if (f) {
    while (f.available()) {
      int c = f.read();
      if (c < 0) break;
      Serial.print((char)c);
    }
    f.close();
  }
}

void MyMesh::setTxPower(int8_t power_dbm) {
  radio_driver.setTxPower(power_dbm);
}

bool MyMesh::setRxBoostedGain(bool enable) {
  return radio_driver.setRxBoostedGainMode(enable);
}

void MyMesh::saveIdentity(const mesh::LocalIdentity &new_id) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  IdentityStore store(*_fs, "");
#elif defined(ESP32)
  IdentityStore store(*_fs, "/identity");
#elif defined(RP2040_PLATFORM)
  IdentityStore store(*_fs, "/identity");
#else
#error "need to define saveIdentity()"
#endif
  store.save("_main", new_id);
}

void MyMesh::startRegionsLoad() {
  temp_map.resetFrom(region_map);   // rebuild regions in a temp instance
  memset(load_stack, 0, sizeof(load_stack));
  load_stack[0] = &temp_map.getWildcard();
  region_load_active = true;
}

bool MyMesh::saveRegions() {
  return region_map.save(_fs);
}

void MyMesh::onDefaultRegionChanged(const RegionEntry* r) {
  if (r) {
    region_map.getTransportKeysFor(*r, &default_scope, 1);
  } else {
    memset(default_scope.key, 0, sizeof(default_scope.key));
  }
}

void MyMesh::clearStats() {
  radio_driver.resetStats();
  resetStats();
  ((SimpleMeshTables *)getTables())->resetStats();
}

void MyMesh::formatStatsReply(char *reply) {
  StatsFormatHelper::formatCoreStats(reply, board, *_ms, _err_flags, _mgr);
}

void MyMesh::formatRadioStatsReply(char *reply) {
  StatsFormatHelper::formatRadioStats(reply, _radio, radio_driver, getTotalAirTime(), getReceiveAirTime());
}

void MyMesh::formatPacketStatsReply(char *reply) {
  StatsFormatHelper::formatPacketStats(reply, radio_driver, getNumSentFlood(), getNumSentDirect(), 
                                       getNumRecvFlood(), getNumRecvDirect());
}

void MyMesh::handleCommand(uint32_t sender_timestamp, char *command, char *reply) {
  if (region_load_active) {
    if (StrHelper::isBlank(command)) {  // empty/blank line, signal to terminate 'load' operation
      region_map = temp_map;  // copy over the temp instance as new current map
      region_load_active = false;

      sprintf(reply, "OK - loaded %d regions", region_map.getCount());
    } else {
      char *np = command;
      while (*np == ' ') np++;   // skip indent
      int indent = np - command;

      char *ep = np;
      while (RegionMap::is_name_char(*ep)) ep++;
      if (*ep) { *ep++ = 0; }  // set null terminator for end of name

      while (*ep && *ep != 'F') ep++;  // look for (optional) flags

      if (indent > 0 && indent < 8 && strlen(np) > 0) {
        auto parent = load_stack[indent - 1];
        if (parent) {
          auto old = region_map.findByName(np);
          auto nw = temp_map.putRegion(np, parent->id, old ? old->id : 0);  // carry-over the current ID (if name already exists)
          if (nw) {
            nw->flags = old ? old->flags : (*ep == 'F' ? 0 : REGION_DENY_FLOOD);   // carry-over flags from curr

            load_stack[indent] = nw;  // keep pointers to parent regions, to resolve parent_id's
          }
        }
      }
      reply[0] = 0;
    }
    return;
  }

  while (*command == ' ')
    command++; // skip leading spaces

  if (strlen(command) > 4 && command[2] == '|') { // optional prefix (for companion radio CLI)
    memcpy(reply, command, 3);                    // reflect the prefix back
    reply += 3;
    command += 3;
  }

#ifdef CACHE_INTERACTIVE_FEATURES
  if (handleCacheCLI(sender_timestamp, cache_cli_sender, command, reply)) return;
#endif

  // handle ACL related commands
  if (memcmp(command, "setperm ", 8) == 0) {   // format:  setperm {pubkey-hex} {permissions-int8}
    char* hex = &command[8];
    char* sp = strchr(hex, ' ');   // look for separator char
    if (sp == NULL) {
      strcpy(reply, "Err - bad params");
    } else {
      *sp++ = 0;   // replace space with null terminator

      uint8_t pubkey[PUB_KEY_SIZE];
      int hex_len = min(sp - hex, PUB_KEY_SIZE*2);
      if (mesh::Utils::fromHex(pubkey, hex_len / 2, hex)) {
        uint8_t perms = atoi(sp);
        if (acl.applyPermissions(self_id, pubkey, hex_len / 2, perms)) {
          dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);   // trigger acl.save()
          strcpy(reply, "OK");
        } else {
          strcpy(reply, "Err - invalid params");
        }
      } else {
        strcpy(reply, "Err - bad pubkey");
      }
    }
  } else if (sender_timestamp == 0 && strcmp(command, "get acl") == 0) {
    Serial.println("ACL:");
    for (int i = 0; i < acl.getNumClients(); i++) {
      auto c = acl.getClientByIdx(i);
      if (c->permissions == 0) continue;  // skip deleted (or guest) entries

      Serial.printf("%02X ", c->permissions);
      mesh::Utils::printHex(Serial, c->id.pub_key, PUB_KEY_SIZE);
      Serial.printf("\n");
    }
    reply[0] = 0;
  } else if (strncmp(command, "room.post", 9) == 0) {
    char* msg = command + 9;
    while (*msg == ' ') msg++;
    if (*msg == 0) {
      snprintf(reply, MAX_POST_TEXT_LEN, "ERR empty message");
    } else {
      addSystemPost(msg);
      snprintf(reply, MAX_POST_TEXT_LEN, "OK");
    }
  } else{
    _cli.handleCommand(sender_timestamp, command, reply);  // common CLI commands
  }
}

bool MyMesh::saveFilter(ClientInfo* client) {
  return client->isAdmin();    // only save Admins
}

void MyMesh::loop() {
  mesh::Mesh::loop();

#ifdef CACHE_INTERACTIVE_FEATURES
  if (cache_advert_reply_at && millisHasNowPassed(cache_advert_reply_at)) {
    cache_advert_reply_at = 0;
    mesh::Packet* advert = createSelfAdvert();
    if (advert) sendZeroHop(advert);
  }
#endif

  if (millisHasNowPassed(next_push) && acl.getNumClients() > 0) {
    // check for ACK timeouts
    for (int i = 0; i < acl.getNumClients(); i++) {
      auto c = acl.getClientByIdx(i);
      if (c->extra.room.pending_ack && millisHasNowPassed(c->extra.room.ack_timeout)) {
        c->extra.room.push_failures++;
        c->extra.room.pending_ack = 0; // reset  (TODO: keep prev expected_ack's in a list, incase they arrive LATER, after we retry)
        MESH_DEBUG_PRINTLN("pending ACK timed out: push_failures: %d", (uint32_t)c->extra.room.push_failures);
      }
    }
    // check next Round-Robin client, and sync next new post
    auto client = acl.getClientByIdx(next_client_idx);
    bool did_push = false;
    if (client->extra.room.pending_ack == 0 && client->last_activity != 0 &&
        client->extra.room.push_failures < 3) { // not already waiting for ACK, AND not evicted, AND retries not max
      MESH_DEBUG_PRINTLN("loop - checking for client %02X", (uint32_t)client->id.pub_key[0]);
#ifdef CACHE_INTERACTIVE_FEATURES
      if (client->extra.room.cache_intro_pending) {
        client->extra.room.cache_intro_pending = 0;
        pushCacheInstructions(client);
        did_push = true;
      }
#endif
      uint32_t now = getRTCClock()->getCurrentTime();
      for (int k = 0, idx = next_post_idx; !did_push && k < MAX_UNSYNCED_POSTS; k++) {
        auto p = &posts[idx];
        if (now >= p->post_timestamp + POST_SYNC_DELAY_SECS &&
            p->post_timestamp > client->extra.room.sync_since // is new post for this Client?
#ifdef CACHE_INTERACTIVE_FEATURES
            && p->post_timestamp <= client->extra.room.cache_sync_until
#else
            && !p->author.matches(client->id)   // don't push posts to the author
#endif
            ) {
          // push this post to Client, then wait for ACK
          pushPostToClient(client, *p);
          did_push = true;
          MESH_DEBUG_PRINTLN("loop - pushed to client %02X: %s", (uint32_t)client->id.pub_key[0], p->text);
          break;
        }
        idx = (idx + 1) % MAX_UNSYNCED_POSTS; // wrap to start of cyclic queue
      }
    } else {
      MESH_DEBUG_PRINTLN("loop - skipping busy (or evicted) client %02X", (uint32_t)client->id.pub_key[0]);
    }
    next_client_idx = (next_client_idx + 1) % acl.getNumClients(); // round robin polling for each client

    if (did_push) {
      next_push = futureMillis(SYNC_PUSH_INTERVAL);
    } else {
      // were no unsynced posts for curr client, so process next client much quicker! (in next loop())
      next_push = futureMillis(SYNC_PUSH_INTERVAL / 8);
    }
  }

  if (next_flood_advert && millisHasNowPassed(next_flood_advert)) {
    mesh::Packet *pkt = createSelfAdvert();
    uint32_t delay_millis = 0;
    if (pkt) sendFloodScoped(default_scope, pkt, delay_millis, _prefs.path_hash_mode + 1);

    updateFloodAdvertTimer(); // schedule next flood advert
    updateAdvertTimer();      // also schedule local advert (so they don't overlap)
  } else if (next_local_advert && millisHasNowPassed(next_local_advert)) {
    mesh::Packet *pkt = createSelfAdvert();
    if (pkt) sendZeroHop(pkt);

    updateAdvertTimer(); // schedule next local advert
  }

  if (set_radio_at && millisHasNowPassed(set_radio_at)) { // apply pending (temporary) radio params
    set_radio_at = 0;                                     // clear timer
    radio_driver.setParams(pending_freq, pending_bw, pending_sf, pending_cr);
    MESH_DEBUG_PRINTLN("Temp radio params");
  }

  if (revert_radio_at && millisHasNowPassed(revert_radio_at)) { // revert radio params to orig
    revert_radio_at = 0;                                        // clear timer
    radio_driver.setParams(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
    MESH_DEBUG_PRINTLN("Radio params restored");
  }

  // is pending dirty contacts write needed?
  if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    acl.save(_fs, MyMesh::saveFilter);
    dirty_contacts_expiry = 0;
  }

  // TODO: periodically check for OLD/inactive entries in known_clients[], and evict

  // update uptime
  uint32_t now = millis();
  uptime_millis += now - last_millis;
  last_millis = now;
}

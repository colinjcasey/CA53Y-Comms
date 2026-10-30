/*
  CA53Y Comms — ESP-NOW pocket messenger for a screenless ESP32 dev module.

  Each device makes its own Wi-Fi network named CA53Ycomms-1234abC (4 digits + 3 letters,
  generated once and kept). Join it from a phone, open 192.168.4.1 (or let the sign-in sheet
  pop up), and message other CA53Y devices over ESP-NOW. No internet, router, or Bluetooth.

  - First boot: network is open, the portal asks for your name and a Wi-Fi password.
    After that it's WPA2 and only ONE phone can be connected at a time.
  - Contacts are added by device ID, or tapped from the Nearby list (devices announce
    themselves every few seconds unless you turn that off in Settings).
  - Optional shared PIN per contact: messages are AES-128-GCM encrypted with a key derived
    from the PIN (PBKDF2) and both device IDs. Other devices see only that a message passed.
  - Delivery receipts with automatic retries.
  - LED (GPIO2) blinks while you have unread messages.
  - Hold BOOT (GPIO0) for 5 seconds to erase everything (name, password, contacts).
  - Mesh relay: every board re-broadcasts messages that aren't for it (up to MAX_HOPS hops),
    so boards placed between two people extend the range. Encrypted messages stay encrypted;
    relays can't read them.
  - Relay-only boards: set RELAY_ONLY to 1 before flashing (or build with -DRELAY_ONLY=1). No Wi-Fi network or portal,
    it just listens and repeats. The LED flashes each time it relays something.

  Board: "ESP32 Dev Module" on Arduino-ESP32 core 3.x. No extra libraries.
  Every device must use the same ESPNOW_CHANNEL.
*/

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_random.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <mbedtls/gcm.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/md.h>
#include "portal.h"

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "Needs Arduino-ESP32 core 3.x: Tools > Board > Boards Manager > esp32 by Espressif > 3.x"
#endif

// ---------- settings ----------
#ifndef RELAY_ONLY
#define RELAY_ONLY     0      // 0 = messenger with phone portal, 1 = relay-only board (no portal)
#endif
#define ESPNOW_CHANNEL 1      // must match on every device
#define MAX_HOPS       8      // how many boards a message may pass through
#define LED_PIN        2      // onboard LED on most ESP32 dev modules
#define RESET_BTN_PIN       0      // BOOT button
#define MAX_CONTACTS   16
#define MAX_NEARBY     20
#define MAX_MSGS       60     // kept in RAM, cleared on restart
#define MAX_OUT        8      // messages waiting for a delivery receipt
#define MAX_TEXT       160    // bytes (UTF-8)
#define SEND_TRIES     6
#define RETRY_MS       1200   // long enough for a receipt to come back over several hops
#define BEACON_MS      3000
#define NEARBY_TTL_MS  15000
#define KDF_ITERS      2000

static const char ID_PREFIX[] = "CA53Ycomms-";   // 11 chars
#define PREFIX_LEN 11
#define ID_LEN     18

// ---------- wire format ----------
enum : uint8_t { T_BEACON = 1, T_MSG = 2, T_ACK = 3 };
#define F_ENC 0x01

struct __attribute__((packed)) PktHdr {
  uint8_t m0, m1, ver, type, flags;
  char from[20];
  char to[20];
  uint32_t mid;
  uint8_t ttl;     // hops left; relays decrement it (not authenticated)
  uint8_t tries;   // resend counter so relays pass on retries too (not authenticated)
};
#define HDR_LEN  ((int)sizeof(PktHdr))
#define AUTH_LEN (HDR_LEN - 2)   // ttl + tries change in flight, so they're left out of the GCM check
#define PROTO_VER 2
#define IV_LEN  12
#define TAG_LEN 16

// ---------- state ----------
struct Contact   { char id[20]; char nick[24]; char pin[33]; };          // saved to flash
struct ContactRt { uint8_t key[16]; bool hasKey; uint16_t unread; bool isNew; };
struct Nearby    { char id[20]; char nick[24]; int8_t rssi; uint32_t seen; bool used; };
struct Msg       { uint32_t uid, seq, mid, t; char peer[20]; char text[MAX_TEXT + 4]; uint8_t dir, st, enc; };
// st: 0 sending, 1 delivered, 2 failed, 3 delivered but recipient couldn't decrypt, 10 received, 11 received but locked
struct Out       { bool used; uint32_t mid, uid, next; uint8_t tries, len; uint8_t buf[250]; };
struct RxItem    { uint8_t len; int8_t rssi; uint8_t data[250]; };
struct Seen      { uint32_t mid; char from[20]; uint8_t ackSt, ackTries; };
struct RelayKey  { uint32_t mid; uint8_t type, tries; char from[20]; };
struct RelayOut  { bool used; uint32_t at; uint8_t len; uint8_t buf[250]; };

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

Preferences prefs;
WebServer server(80);
DNSServer dns;
QueueHandle_t rxq;

char myId[20], myNick[24], apPw[64];
bool isSetup = false, discoverable = true;
Contact contacts[MAX_CONTACTS];
ContactRt crt[MAX_CONTACTS];
int nContacts = 0;
Nearby nearby[MAX_NEARBY];
Msg msgs[MAX_MSGS];
int msgHead = 0, msgCount = 0;
Out outq[MAX_OUT];
Seen seen[32];
int seenPos = 0;
uint32_t gSeq = 0, gUid = 0, bootId = 0, lastBeacon = 0, restartAt = 0;
RelayKey relaySeen[64];
int relayPos = 0;
RelayOut relayq[10];
uint32_t relayedCount = 0, lastRelayAt = 0;
#if RELAY_ONLY
#define NOW_IF WIFI_IF_STA
#else
#define NOW_IF WIFI_IF_AP
#endif

// ---------- helpers ----------
bool normalizeId(String s, char *out) {
  s.trim();
  if (s.length() == 7) s = String(ID_PREFIX) + s;
  if (s.length() != ID_LEN) return false;
  if (strncasecmp(s.c_str(), ID_PREFIX, PREFIX_LEN) != 0) return false;
  for (int i = PREFIX_LEN; i < PREFIX_LEN + 4; i++) if (!isdigit((unsigned char)s[i])) return false;
  for (int i = PREFIX_LEN + 4; i < ID_LEN; i++) if (!isalpha((unsigned char)s[i])) return false;
  memcpy(out, ID_PREFIX, PREFIX_LEN);
  memcpy(out + PREFIX_LEN, s.c_str() + PREFIX_LEN, 7);
  out[ID_LEN] = 0;
  return true;
}

// copy text, drop control chars (keep newline), cut on a UTF-8 boundary
void cleanText(char *dst, size_t cap, const char *src, size_t n, bool allowNewline) {
  if (n > cap - 1) {
    n = cap - 1;
    while (n > 0 && ((uint8_t)src[n] & 0xC0) == 0x80) n--;
  }
  size_t j = 0;
  for (size_t i = 0; i < n && src[i]; i++) {
    char c = src[i];
    if ((uint8_t)c < 0x20) c = (allowNewline && c == '\n') ? '\n' : ' ';
    dst[j++] = c;
  }
  dst[j] = 0;
}

void jesc(String &o, const char *s) {
  o += '"';
  for (; *s; s++) {
    uint8_t c = *s;
    if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if (c == '\n') o += "\\n";
    else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
    else o += (char)c;
  }
  o += '"';
}

int findContact(const char *id) {
  for (int i = 0; i < nContacts; i++) if (!strcasecmp(contacts[i].id, id)) return i;
  return -1;
}

Nearby *findNearby(const char *id) {
  for (auto &n : nearby) if (n.used && !strcasecmp(n.id, id)) return &n;
  return nullptr;
}

void deriveKey(int ci) {
  Contact &c = contacts[ci];
  crt[ci].hasKey = false;
  if (!c.pin[0]) return;
  // salt = both IDs, lowercase, sorted, so both sides derive the same key
  char a[20], b[20];
  for (int i = 0; i < 20; i++) { a[i] = tolower(myId[i]); b[i] = tolower(c.id[i]); }
  String salt = "CA53Ycomms|v1|";
  if (strcmp(a, b) < 0) { salt += a; salt += '|'; salt += b; }
  else                  { salt += b; salt += '|'; salt += a; }
  int r = mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256,
            (const unsigned char *)c.pin, strlen(c.pin),
            (const unsigned char *)salt.c_str(), salt.length(),
            KDF_ITERS, 16, crt[ci].key);
  crt[ci].hasKey = (r == 0);
}

void saveContacts() {
  prefs.putBytes("contacts", contacts, sizeof(Contact) * nContacts);
  if (nContacts == 0) prefs.remove("contacts");
}

int addContact(const char *id, const char *nick) {
  if (nContacts >= MAX_CONTACTS) return -1;
  int i = nContacts++;
  memset(&contacts[i], 0, sizeof(Contact));
  memset(&crt[i], 0, sizeof(ContactRt));
  strlcpy(contacts[i].id, id, sizeof contacts[i].id);
  if (nick) strlcpy(contacts[i].nick, nick, sizeof contacts[i].nick);
  return i;
}

Msg *newMsg() {
  int idx;
  if (msgCount < MAX_MSGS) { idx = (msgHead + msgCount) % MAX_MSGS; msgCount++; }
  else { idx = msgHead; msgHead = (msgHead + 1) % MAX_MSGS; }
  Msg *m = &msgs[idx];
  memset(m, 0, sizeof(Msg));
  m->uid = ++gUid;
  m->seq = ++gSeq;
  m->t = millis();
  return m;
}

Msg *findMsg(uint32_t uid) {
  for (int i = 0; i < msgCount; i++) {
    Msg *m = &msgs[(msgHead + i) % MAX_MSGS];
    if (m->uid == uid) return m;
  }
  return nullptr;
}

void setMsgStatus(uint32_t uid, uint8_t st) {
  Msg *m = findMsg(uid);
  if (m && m->st != st) { m->st = st; m->seq = ++gSeq; }
}

void fillHdr(uint8_t *buf, uint8_t type, const char *to, uint32_t mid) {
  PktHdr *h = (PktHdr *)buf;
  memset(h, 0, HDR_LEN);
  h->m0 = 0xCA; h->m1 = 0x53; h->ver = PROTO_VER; h->type = type;
  h->ttl = (type == T_BEACON) ? 0 : MAX_HOPS;
  strlcpy(h->from, myId, sizeof h->from);
  if (to) strlcpy(h->to, to, sizeof h->to);
  h->mid = mid;
}

bool gcmEncrypt(const uint8_t *key, const uint8_t *aad, const uint8_t *pt, size_t n,
                uint8_t *iv, uint8_t *tag, uint8_t *ct) {
  mbedtls_gcm_context g;
  mbedtls_gcm_init(&g);
  esp_fill_random(iv, IV_LEN);
  int r = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 128);
  if (!r) r = mbedtls_gcm_crypt_and_tag(&g, MBEDTLS_GCM_ENCRYPT, n, iv, IV_LEN, aad, AUTH_LEN, pt, ct, TAG_LEN, tag);
  mbedtls_gcm_free(&g);
  return r == 0;
}

bool gcmDecrypt(const uint8_t *key, const uint8_t *aad, const uint8_t *iv, const uint8_t *tag,
                const uint8_t *ct, size_t n, uint8_t *pt) {
  mbedtls_gcm_context g;
  mbedtls_gcm_init(&g);
  int r = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 128);
  if (!r) r = mbedtls_gcm_auth_decrypt(&g, n, iv, IV_LEN, aad, AUTH_LEN, tag, TAG_LEN, ct, pt);
  mbedtls_gcm_free(&g);
  return r == 0;
}

void sendBeacon() {
  uint8_t buf[HDR_LEN + 24];
  fillHdr(buf, T_BEACON, nullptr, esp_random());
  memset(buf + HDR_LEN, 0, 24);
  strlcpy((char *)buf + HDR_LEN, myNick, 24);
  esp_now_send(BCAST, buf, sizeof buf);
}

void sendAck(const char *to, uint32_t mid, uint8_t st, uint8_t tries) {
  uint8_t buf[HDR_LEN + 1];
  fillHdr(buf, T_ACK, to, mid);
  ((PktHdr *)buf)->tries = tries;
  buf[HDR_LEN] = st;
  esp_now_send(BCAST, buf, sizeof buf);
}

// ---------- mesh relay ----------
bool relayedBefore(const PktHdr &h, const char *from) {
  for (auto &k : relaySeen)
    if (k.mid == h.mid && k.type == h.type && k.tries == h.tries && !strcasecmp(k.from, from)) return true;
  RelayKey &k = relaySeen[relayPos];
  relayPos = (relayPos + 1) % 64;
  k.mid = h.mid; k.type = h.type; k.tries = h.tries;
  strlcpy(k.from, from, sizeof k.from);
  return false;
}

void maybeRelay(const RxItem &it, const PktHdr &h, const char *from) {
  if (h.ttl == 0 || (h.type != T_MSG && h.type != T_ACK)) return;
  if (relayedBefore(h, from)) return;
  for (auto &r : relayq) {
    if (r.used) continue;
    memcpy(r.buf, it.data, it.len);
    r.buf[offsetof(PktHdr, ttl)] = h.ttl - 1;
    r.len = it.len;
    r.at = millis() + 5 + esp_random() % 45;   // random delay so relays don't talk over each other
    r.used = true;
    return;
  }
}

// ---------- ESP-NOW receive (runs in the Wi-Fi task: just queue it) ----------
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len < HDR_LEN || len > 250 || data[0] != 0xCA || data[1] != 0x53) return;
  RxItem it;
  it.len = len;
  it.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  memcpy(it.data, data, len);
  xQueueSend(rxq, &it, 0);
}

void handlePacket(RxItem &it) {
  PktHdr h;
  memcpy(&h, it.data, HDR_LEN);
  if (h.ver != PROTO_VER) return;
  char from[20], to[20];
  h.from[19] = 0; h.to[19] = 0;
  if (!normalizeId(String(h.from), from)) return;
  if (!strcasecmp(from, myId)) return;
  const uint8_t *pl = it.data + HDR_LEN;
  int pn = it.len - HDR_LEN;
  uint32_t now = millis();

  // remember who's in direct range (beacons, or messages that haven't been relayed)
  bool direct = h.type == T_BEACON || h.ttl == MAX_HOPS;
  Nearby *nb = findNearby(from);
  if (direct && !nb) {
    Nearby *slot = nullptr;
    for (auto &n : nearby) if (!n.used || now - n.seen > NEARBY_TTL_MS) { slot = &n; break; }
    if (!slot) { slot = &nearby[0]; for (auto &n : nearby) if (n.seen < slot->seen) slot = &n; }
    memset(slot, 0, sizeof(Nearby));
    slot->used = true;
    strlcpy(slot->id, from, sizeof slot->id);
    nb = slot;
  }
  if (direct) { nb->seen = now; nb->rssi = it.rssi; }

  if (h.type == T_BEACON) {
    cleanText(nb->nick, sizeof nb->nick, (const char *)pl, min(pn, 23), false);
    return;
  }
  if (!normalizeId(String(h.to), to)) return;
  if (strcasecmp(to, myId)) { maybeRelay(it, h, from); return; }   // not for us: pass it on
  if (!isSetup || RELAY_ONLY) return;

  if (h.type == T_ACK) {
    if (pn < 1) return;
    for (auto &o : outq) {
      if (!o.used || o.mid != h.mid) continue;
      if (strcasecmp(((PktHdr *)o.buf)->to, from)) continue;
      setMsgStatus(o.uid, pl[0] ? 3 : 1);
      o.used = false;
    }
    return;
  }

  if (h.type == T_MSG) {
    // duplicate (our receipt got lost and they resent): just receipt again
    for (auto &s : seen) {
      if (s.mid == h.mid && !strcasecmp(s.from, from)) { sendAck(from, h.mid, s.ackSt, ++s.ackTries); return; }
    }
    int ci = findContact(from);
    if (ci < 0) {
      ci = addContact(from, nb ? nb->nick : "");
      if (ci < 0) return;                 // contact list full, ignore strangers
      crt[ci].isNew = true;
      saveContacts();
    }
    Msg *m = newMsg();
    m->dir = 0;
    m->mid = h.mid;
    strlcpy(m->peer, contacts[ci].id, sizeof m->peer);
    uint8_t ackSt = 0;
    if (h.flags & F_ENC) {
      m->enc = 1;
      int n = pn - IV_LEN - TAG_LEN;
      uint8_t pt[MAX_TEXT + 1];
      if (n > 0 && n <= MAX_TEXT && crt[ci].hasKey &&
          gcmDecrypt(crt[ci].key, it.data, pl, pl + IV_LEN, pl + IV_LEN + TAG_LEN, n, pt)) {
        cleanText(m->text, sizeof m->text, (const char *)pt, n, true);
        m->st = 10;
      } else {
        m->st = 11;
        ackSt = 1;
      }
    } else {
      cleanText(m->text, sizeof m->text, (const char *)pl, min(pn, MAX_TEXT), true);
      m->st = 10;
    }
    crt[ci].unread++;
    Seen &s = seen[seenPos];
    seenPos = (seenPos + 1) % 32;
    s.mid = h.mid; s.ackSt = ackSt; s.ackTries = 0;
    strlcpy(s.from, from, sizeof s.from);
    sendAck(from, h.mid, ackSt, 0);
    Serial.printf("[rx] %s: %s\n", from, m->st == 10 ? m->text : "(locked)");
  }
}

// ---------- web ----------
void noStore() { server.sendHeader("Cache-Control", "no-store"); }
void sendErr(int code, const char *msg) { noStore(); server.send(code, "text/plain", msg); }
void sendOk() { noStore(); server.send(200, "text/plain", "ok"); }

bool validPw(const String &pw) {
  if (pw.length() < 8 || pw.length() > 63) return false;
  for (size_t i = 0; i < pw.length(); i++) if (pw[i] < 0x20 || pw[i] > 0x7E) return false;
  return true;
}

void handleRoot() {
  noStore();
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleState() {
  uint32_t since = strtoul(server.arg("since").c_str(), nullptr, 10);
  uint32_t now = millis();
  String o;
  o.reserve(6000);
  o += "{\"boot\":"; o += bootId;
  o += ",\"seq\":"; o += gSeq;
  o += ",\"setup\":"; o += isSetup ? "true" : "false";
  o += ",\"me\":{\"id\":"; jesc(o, myId);
  o += ",\"nick\":"; jesc(o, myNick);
  o += ",\"disc\":"; o += discoverable ? "true" : "false";
  o += "},\"contacts\":[";
  for (int i = 0; i < nContacts; i++) {
    if (i) o += ',';
    o += "{\"id\":"; jesc(o, contacts[i].id);
    o += ",\"nick\":"; jesc(o, contacts[i].nick);
    o += ",\"pin\":"; o += contacts[i].pin[0] ? "true" : "false";
    o += ",\"unread\":"; o += crt[i].unread;
    o += ",\"new\":"; o += crt[i].isNew ? "true" : "false";
    o += '}';
  }
  o += "],\"nearby\":[";
  bool first = true;
  for (auto &n : nearby) {
    if (!n.used || now - n.seen > NEARBY_TTL_MS) continue;
    if (!first) o += ',';
    first = false;
    o += "{\"id\":"; jesc(o, n.id);
    o += ",\"nick\":"; jesc(o, n.nick);
    o += ",\"rssi\":"; o += (int)n.rssi;
    o += '}';
  }
  o += "],\"msgs\":[";
  first = true;
  for (int i = 0; i < msgCount; i++) {
    Msg &m = msgs[(msgHead + i) % MAX_MSGS];
    if (m.seq <= since) continue;
    if (!first) o += ',';
    first = false;
    o += "{\"u\":"; o += m.uid;
    o += ",\"p\":"; jesc(o, m.peer);
    o += ",\"d\":"; o += m.dir;
    o += ",\"x\":"; jesc(o, m.text);
    o += ",\"st\":"; o += m.st;
    o += ",\"e\":"; o += m.enc;
    o += ",\"age\":"; o += (uint32_t)(now - m.t);
    o += '}';
  }
  o += "]}";
  noStore();
  server.send(200, "application/json", o);
}

void handleSetup() {
  if (isSetup) return sendErr(403, "This device is already set up. Use Settings to change the password.");
  String pw = server.arg("pw");
  if (!validPw(pw)) return sendErr(400, "Passwords are 8 to 63 characters.");
  String nick = server.arg("nick");
  cleanText(myNick, sizeof myNick, nick.c_str(), nick.length(), false);
  prefs.putString("nick", myNick);
  prefs.putString("pw", pw);
  prefs.putBool("setup", true);
  sendOk();
  restartAt = millis() + 1500;
}

void handleSettings() {
  if (!isSetup) return sendErr(403, "Set up the device first.");
  String pw = server.arg("pw");
  if (pw.length() && !validPw(pw)) return sendErr(400, "Passwords are 8 to 63 characters.");
  String nick = server.arg("nick");
  cleanText(myNick, sizeof myNick, nick.c_str(), nick.length(), false);
  discoverable = server.arg("disc") == "1";
  prefs.putString("nick", myNick);
  prefs.putBool("disc", discoverable);
  if (pw.length()) { prefs.putString("pw", pw); restartAt = millis() + 1500; }
  sendOk();
}

void handleContact() {
  if (!isSetup) return sendErr(403, "Set up the device first.");
  char id[20];
  if (!normalizeId(server.arg("id"), id)) return sendErr(400, "Device IDs look like CA53Ycomms-1234abC (4 numbers, then 3 letters).");
  if (!strcasecmp(id, myId)) return sendErr(400, "That's this device's own ID.");
  String mode = server.arg("pinmode"), pin = server.arg("pin");
  if (mode == "set" && (pin.length() < 4 || pin.length() > 32)) return sendErr(400, "PINs are 4 to 32 characters.");
  int ci = findContact(id);
  if (ci < 0) {
    Nearby *n = findNearby(id);
    ci = addContact(id, n ? n->nick : "");
    if (ci < 0) return sendErr(400, "Your contact list is full (16). Delete one first.");
  }
  String nick = server.arg("nick");
  if (nick.length()) cleanText(contacts[ci].nick, sizeof contacts[ci].nick, nick.c_str(), nick.length(), false);
  if (mode == "set")   strlcpy(contacts[ci].pin, pin.c_str(), sizeof contacts[ci].pin);
  if (mode == "clear") contacts[ci].pin[0] = 0;
  crt[ci].isNew = false;
  deriveKey(ci);
  saveContacts();
  sendOk();
}

void handleContactDel() {
  char id[20];
  if (!normalizeId(server.arg("id"), id)) return sendErr(400, "Unknown contact.");
  int ci = findContact(id);
  if (ci < 0) return sendErr(404, "Unknown contact.");
  for (int i = ci; i < nContacts - 1; i++) { contacts[i] = contacts[i + 1]; crt[i] = crt[i + 1]; }
  nContacts--;
  saveContacts();
  sendOk();
}

void handleRead() {
  char id[20];
  if (normalizeId(server.arg("id"), id)) {
    int ci = findContact(id);
    if (ci >= 0) { crt[ci].unread = 0; crt[ci].isNew = false; }
  }
  sendOk();
}

void handleSend() {
  if (!isSetup) return sendErr(403, "Set up the device first.");
  char to[20];
  if (!normalizeId(server.arg("to"), to)) return sendErr(400, "Unknown device ID.");
  int ci = findContact(to);
  if (ci < 0) return sendErr(400, "Add this device as a contact first.");
  String text = server.arg("text");
  text.trim();
  if (!text.length()) return sendErr(400, "Type a message first.");
  if (text.length() > MAX_TEXT) return sendErr(400, "Messages can be up to 160 characters.");
  Out *o = nullptr;
  for (auto &x : outq) if (!x.used) { o = &x; break; }
  if (!o) return sendErr(503, "Still sending earlier messages. Try again in a moment.");

  char clean[MAX_TEXT + 1];
  cleanText(clean, sizeof clean, text.c_str(), text.length(), true);
  size_t n = strlen(clean);
  uint32_t mid = esp_random();
  fillHdr(o->buf, T_MSG, contacts[ci].id, mid);
  if (crt[ci].hasKey) {
    ((PktHdr *)o->buf)->flags = F_ENC;                    // set before encrypting: header is authenticated
    uint8_t *iv = o->buf + HDR_LEN, *tag = iv + IV_LEN, *ct = tag + TAG_LEN;
    if (!gcmEncrypt(crt[ci].key, o->buf, (const uint8_t *)clean, n, iv, tag, ct))
      return sendErr(500, "Encryption failed.");
    o->len = HDR_LEN + IV_LEN + TAG_LEN + n;
  } else {
    memcpy(o->buf + HDR_LEN, clean, n);
    o->len = HDR_LEN + n;
  }
  Msg *m = newMsg();
  m->dir = 1;
  m->st = 0;
  m->mid = mid;
  m->enc = crt[ci].hasKey;
  strlcpy(m->peer, contacts[ci].id, sizeof m->peer);
  strlcpy(m->text, clean, sizeof m->text);
  o->used = true;
  o->mid = mid;
  o->uid = m->uid;
  o->tries = 0;
  o->next = millis();
  sendOk();
}

void handleNotFound() {   // captive portal: send every stray request to the portal
  server.sendHeader("Location", "http://192.168.4.1/", true);
  server.send(302, "text/plain", "");
}

// ---------- setup / loop ----------
void makeId() {
  char suf[8];
  for (int i = 0; i < 4; i++) suf[i] = '0' + esp_random() % 10;
  const char *L = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  for (int i = 4; i < 7; i++) suf[i] = L[esp_random() % 52];
  suf[7] = 0;
  snprintf(myId, sizeof myId, "%s%s", ID_PREFIX, suf);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(RESET_BTN_PIN, INPUT_PULLUP);
  bootId = esp_random() | 1;

  prefs.begin("ca53y", false);
  String id = prefs.getString("id", "");
  if (!normalizeId(id, myId)) { makeId(); prefs.putString("id", myId); }
  strlcpy(myNick, prefs.getString("nick", "").c_str(), sizeof myNick);
  strlcpy(apPw, prefs.getString("pw", "").c_str(), sizeof apPw);
  isSetup = prefs.getBool("setup", false) && strlen(apPw) >= 8;
  discoverable = prefs.getBool("disc", true);
  size_t blen = prefs.getBytesLength("contacts");
  if (blen && blen % sizeof(Contact) == 0 && blen <= sizeof contacts) {
    prefs.getBytes("contacts", contacts, blen);
    nContacts = blen / sizeof(Contact);
  }
  for (int i = 0; i < nContacts; i++) deriveKey(i);

  rxq = xQueueCreate(12, sizeof(RxItem));

#if RELAY_ONLY
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
#else
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(myId, isSetup ? apPw : nullptr, ESPNOW_CHANNEL, 0, 1);   // max 1 phone
#endif
  esp_wifi_set_ps(WIFI_PS_NONE);

  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed, restarting"); delay(2000); ESP.restart(); }
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 0;            // current channel (ESPNOW_CHANNEL)
  peer.ifidx = NOW_IF;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

#if RELAY_ONLY
  Serial.printf("\nCA53Y Comms RELAY  %s  channel %d  max hops %d\n", myId, ESPNOW_CHANNEL, MAX_HOPS);
  return;
#endif
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/setup", HTTP_POST, handleSetup);
  server.on("/api/settings", HTTP_POST, handleSettings);
  server.on("/api/contact", HTTP_POST, handleContact);
  server.on("/api/contact/del", HTTP_POST, handleContactDel);
  server.on("/api/read", HTTP_POST, handleRead);
  server.on("/api/send", HTTP_POST, handleSend);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.printf("\nCA53Y Comms  %s  (%s)  channel %d  contacts %d\n", myId,
                isSetup ? "password set" : "OPEN - waiting for setup", ESPNOW_CHANNEL, nContacts);
  Serial.println("Join that Wi-Fi network and open http://192.168.4.1");
}

void loop() {
#if !RELAY_ONLY
  dns.processNextRequest();
  server.handleClient();
#endif

  RxItem it;
  while (xQueueReceive(rxq, &it, 0) == pdTRUE) handlePacket(it);

  uint32_t now = millis();

  // pass along messages meant for someone else
  for (auto &r : relayq) {
    if (!r.used || (int32_t)(now - r.at) < 0) continue;
    esp_now_send(BCAST, r.buf, r.len);
    r.used = false;
    relayedCount++;
    lastRelayAt = now;
  }

#if RELAY_ONLY
  static uint32_t lastLog = 0;
  if (now - lastLog > 60000) { lastLog = now; Serial.printf("[relay] %lu packets relayed\n", (unsigned long)relayedCount); }
  digitalWrite(LED_PIN, lastRelayAt && now - lastRelayAt < 120);   // flash on each relay
  return;
#endif

  // send / retry queued messages until a receipt comes back
  for (auto &o : outq) {
    if (!o.used || (int32_t)(now - o.next) < 0) continue;
    if (o.tries >= SEND_TRIES) { setMsgStatus(o.uid, 2); o.used = false; continue; }
    ((PktHdr *)o.buf)->tries = o.tries;
    esp_now_send(BCAST, o.buf, o.len);
    o.tries++;
    o.next = now + RETRY_MS + esp_random() % 250;
  }

  if (isSetup && discoverable && now - lastBeacon > BEACON_MS) {
    lastBeacon = now + esp_random() % 400;   // jitter so devices don't collide
    sendBeacon();
  }

  // LED: blink while there are unread messages
  bool unread = false;
  for (int i = 0; i < nContacts; i++) if (crt[i].unread) unread = true;
  static uint32_t pressStart = 0;
  bool held = digitalRead(RESET_BTN_PIN) == LOW;
  if (held) {
    if (!pressStart) pressStart = now;
    if (now - pressStart > 5000) {
      Serial.println("Factory reset");
      for (int i = 0; i < 10; i++) { digitalWrite(LED_PIN, i & 1); delay(80); }
      prefs.clear();
      ESP.restart();
    }
  } else pressStart = 0;
  if (held && now - pressStart > 1500) digitalWrite(LED_PIN, (now / 100) & 1);
  else digitalWrite(LED_PIN, unread && (now % 1500) < 150);

  if (restartAt && (int32_t)(now - restartAt) >= 0) ESP.restart();
}

/*
  CA53Y Comms — relay node for an ESP32-S3-MINI-1U board with an external 2.4 GHz antenna.

  Listens on the CA53Y channel and re-broadcasts every message and delivery receipt that
  passes by, so phones on CA53Y Comms boards can reach each other through it. No Wi-Fi
  network, no portal, nothing to set up. Encrypted messages are passed on untouched; the
  relay can't read them.

  Must match the messenger firmware: same ESPNOW_CHANNEL, protocol version 2.

  Arduino IDE settings:
    Board:            ESP32S3 Dev Module
    USB CDC On Boot:  Enabled   (so the serial monitor works over the USB-C port)
    Flash Size:       4MB       (works on 4 MB and 8 MB modules)
  Serial monitor at 115200 prints a line for every relayed packet.

  ANTENNA: never power this board without the antenna attached. Transmitting into an open
  connector can damage the radio.
*/

#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_random.h>

#if !defined(ESP_ARDUINO_VERSION_MAJOR) || ESP_ARDUINO_VERSION_MAJOR < 3
#error "Needs Arduino-ESP32 core 3.x"
#endif

// ---------- settings ----------
#define ESPNOW_CHANNEL 1     // must match every CA53Y board
#define TX_POWER_QDBM  80    // transmit power in 0.25 dBm steps (80 = 20 dBm, the S3's maximum)
#define LED_PIN        -1    // plain LED GPIO to flash on each relay, or -1 for none
#define RGB_LED_PIN    -1    // WS2812 RGB LED GPIO (often 48 on S3 boards), or -1 for none
#define VERBOSE        1     // 1 = print every relayed packet

// ---------- wire format (identical to the messenger firmware) ----------
enum : uint8_t { T_BEACON = 1, T_MSG = 2, T_ACK = 3 };
struct __attribute__((packed)) PktHdr {
  uint8_t m0, m1, ver, type, flags;
  char from[20];
  char to[20];
  uint32_t mid;
  uint8_t ttl;
  uint8_t tries;
};
#define HDR_LEN   ((int)sizeof(PktHdr))
#define PROTO_VER 2

struct RxItem   { uint8_t len; int8_t rssi; uint8_t data[250]; };
struct RelayKey { uint32_t mid; uint8_t type, tries; char from[20]; };
struct RelayOut { bool used; uint32_t at; uint8_t len; uint8_t buf[250]; };

static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
QueueHandle_t rxq;
RelayKey seenKeys[128];
int seenPos = 0;
RelayOut relayq[16];
uint32_t relayedCount = 0, heardCount = 0, lastRelayAt = 0, lastLog = 0;

void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  if (len < HDR_LEN || len > 250 || data[0] != 0xCA || data[1] != 0x53) return;
  RxItem it;
  it.len = len;
  it.rssi = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  memcpy(it.data, data, len);
  xQueueSend(rxq, &it, 0);
}

bool seenBefore(const PktHdr &h) {
  for (auto &k : seenKeys)
    if (k.mid == h.mid && k.type == h.type && k.tries == h.tries && !strncasecmp(k.from, h.from, 19)) return true;
  RelayKey &k = seenKeys[seenPos];
  seenPos = (seenPos + 1) % 128;
  k.mid = h.mid; k.type = h.type; k.tries = h.tries;
  strlcpy(k.from, h.from, sizeof k.from);
  return false;
}

void handle(const RxItem &it) {
  PktHdr h;
  memcpy(&h, it.data, HDR_LEN);
  h.from[19] = 0; h.to[19] = 0;
  if (h.ver != PROTO_VER) return;
  heardCount++;
  if (h.ttl == 0 || (h.type != T_MSG && h.type != T_ACK)) return;   // beacons stay local
  if (seenBefore(h)) return;
  for (auto &r : relayq) {
    if (r.used) continue;
    memcpy(r.buf, it.data, it.len);
    r.buf[offsetof(PktHdr, ttl)] = h.ttl - 1;
    r.len = it.len;
    r.at = millis() + 5 + esp_random() % 45;   // random delay so relays don't talk over each other
    r.used = true;
    if (VERBOSE)
      Serial.printf("[relay] %s %s -> %s  hops left %d  signal %d dBm\n",
                    h.type == T_MSG ? "msg    " : "receipt", h.from, h.to, h.ttl - 1, it.rssi);
    return;
  }
  Serial.println("[relay] queue full, dropped a packet");
}

void setLed(bool on) {
#if LED_PIN >= 0
  digitalWrite(LED_PIN, on);
#endif
#if RGB_LED_PIN >= 0
  static bool last = false;
  if (on != last) { rgbLedWrite(RGB_LED_PIN, 0, on ? 40 : 0, on ? 25 : 0); last = on; }
#endif
}

void setup() {
  Serial.begin(115200);
#if LED_PIN >= 0
  pinMode(LED_PIN, OUTPUT);
#endif
  rxq = xQueueCreate(24, sizeof(RxItem));

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);                                  // radio always listening
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_max_tx_power(TX_POWER_QDBM);

  if (esp_now_init() != ESP_OK) { Serial.println("ESP-NOW init failed, restarting"); delay(2000); ESP.restart(); }
  esp_now_register_recv_cb(onRecv);
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BCAST, 6);
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  int8_t pwr = 0;
  esp_wifi_get_max_tx_power(&pwr);
  delay(1500);   // give the USB serial port a moment to connect
  Serial.printf("\nCA53Y Comms relay (S3)  channel %d  tx power %.1f dBm\n", ESPNOW_CHANNEL, pwr / 4.0);
  Serial.println("Listening. Each relayed message prints a line here.");

  for (int i = 0; i < 3; i++) { setLed(true); delay(120); setLed(false); delay(120); }   // "I'm alive"
}

void loop() {
  RxItem it;
  while (xQueueReceive(rxq, &it, 0) == pdTRUE) handle(it);

  uint32_t now = millis();
  for (auto &r : relayq) {
    if (!r.used || (int32_t)(now - r.at) < 0) continue;
    esp_now_send(BCAST, r.buf, r.len);
    r.used = false;
    relayedCount++;
    lastRelayAt = now;
  }
  setLed(lastRelayAt && now - lastRelayAt < 150);

  if (now - lastLog > 60000) {
    lastLog = now;
    Serial.printf("[status] up %lu min, heard %lu CA53Y packets, relayed %lu\n",
                  (unsigned long)(now / 60000), (unsigned long)heardCount, (unsigned long)relayedCount);
  }
  delay(1);
}

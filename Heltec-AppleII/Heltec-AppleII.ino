/*
  Heltec-AppleII  -  an Apple IIe / ][+ on a Heltec WiFi LoRa 32 V3 (ESP32-S3), played in a web browser.

  - Emulator (65C02 + IIe memory + Disk II + speaker) runs on CPU core 1
  - Wi-Fi, web pages (port 80), screen+sound stream (WebSocket, port 81) and the OLED run on core 0
  - Open http://<board-ip>/ (or http://apple2.local/) in any browser: screen, sound, keyboard, joystick
  - Your own Apple IIe (16 KB) or ][+ (12 KB) ROM, uploaded from the settings page
  - .dsk / .do / .po floppy images on flash in two Disk II drives; programs can save back to them
  - OLED shows the address to open; PRG button: tap = switch OLED view, hold 2 s = Ctrl-Reset

  Board: "Heltec WiFi LoRa 32(V3)" (esp32 Arduino core 3.x). partitions.csv in this folder
  gives a 1.9 MB app + ~6 MB of flash storage for disk images.
*/
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <Wire.h>
#include <vector>
#include <algorithm>
#include "apple2.h"
#include "stream.h"
#include "font.h"
#include "webui.h"
#include "player.h"

// ---------------- Heltec V3 pins
#define PIN_VEXT 36
#define PIN_OLED_RST 21
#define PIN_OLED_SDA 17
#define PIN_OLED_SCL 18
#define PIN_LED 35
#define PIN_BUTTON 0

static const char* AP_SSID = "AppleII-Setup";
static const char* AP_PASS = "appleiie";
static const char* HOSTNAME = "apple2";
static const char* ROM_PATH = "/rom.bin";

static Apple2 apple;
static InputQueue input;
static StreamServer stream(apple, input);
static WebServer web(80);
static WiFiServer streamListener(81);
static Preferences prefs;

static bool apMode = false;
static volatile bool warp = false;
static volatile int speedPct = 0;
static volatile int oledPage = 0;
static volatile bool cmdReset = false, cmdPower = false, cmdRom = false;

static String romName = "No ROM";
static uint32_t romCrc = 0;
static bool romCustom = false;

static uint32_t crc32(const uint8_t* d, size_t n, uint32_t c = 0xFFFFFFFF) {
  for (size_t i = 0; i < n; i++) {
    c ^= d[i];
    for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320 & -(int32_t)(c & 1));
  }
  return c;
}

// ---------------- disk images on LittleFS (read/written a track at a time)
struct FsDisk : DiskImage {
  File f;
  String name;
  bool readTrack(int t, uint8_t* buf) override {
    if (!f || !f.seek((uint32_t)t * 4096)) return false;
    return f.read(buf, 4096) == 4096;
  }
  bool writeTrack(int t, const uint8_t* buf) override {
    if (!f || !f.seek((uint32_t)t * 4096)) return false;
    bool ok = f.write(buf, 4096) == 4096;
    f.flush();
    return ok;
  }
};
static FsDisk disks[2];

static String cleanName(String n) {
  int s = n.lastIndexOf('/');
  if (s >= 0) n = n.substring(s + 1);
  String o;
  for (char ch : n) if (isalnum((unsigned char)ch) || strchr(" -_.!+()", ch)) o += ch;
  o.trim();
  if (o.length() > 40) o = o.substring(o.length() - 40);
  return o;
}
static bool isPo(const String& n) { String l = n; l.toLowerCase(); return l.endsWith(".po"); }

static void listDisks(std::vector<std::pair<String, uint32_t>>& out) {
  File dir = LittleFS.open("/disks");
  if (!dir) return;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    String n = f.name();
    if (!n.endsWith(".tmp")) out.push_back({n, (uint32_t)f.size()});
    f.close();
  }
  std::sort(out.begin(), out.end(), [](const std::pair<String, uint32_t>& a, const std::pair<String, uint32_t>& b) {
    return a.first.compareTo(b.first) < 0; });
}

// insert requests are carried out by the emulator task
static portMUX_TYPE cmdMux = portMUX_INITIALIZER_UNLOCKED;
static String wantName[2];
static volatile bool wantPending[2] = {false, false}, wantBoot = false;
static volatile bool wpWant[2] = {false, false};

static void doInsert(int d, const String& name) {
  apple.insertDisk(d, nullptr);  // flushes the old image
  if (disks[d].f) disks[d].f.close();
  disks[d].name = "";
  if (!name.length()) return;
  File f = LittleFS.open("/disks/" + name, "r+");
  if (!f || f.size() < 143360) { if (f) f.close(); return; }
  disks[d].f = f;
  disks[d].name = name;
  disks[d].prodosOrder = isPo(name);
  if (!disks[d].prodosOrder) {  // a .dsk that is really ProDOS-ordered: volume directory at block 2
    uint8_t b[64];
    if (f.seek(0x400) && f.read(b, 64) == 64 && b[0] == 0 && b[1] == 0 && (b[4] >> 4) == 0xF && b[0x23] == 0x27 && b[0x24] == 0x0D)
      disks[d].prodosOrder = true;
  }
  disks[d].writeProtected = wpWant[d];
  apple.insertDisk(d, &disks[d]);
}

static void requestInsert(int d, const String& name, bool boot) {
  portENTER_CRITICAL(&cmdMux);
  wantName[d] = name;
  wantPending[d] = true;
  if (boot) wantBoot = true;
  portEXIT_CRITICAL(&cmdMux);
  prefs.putString(d ? "d2" : "d1", name);
}

// ---------------- ROM
static void loadRom() {
  File f = LittleFS.open(ROM_PATH, "r");
  std::vector<uint8_t> buf;
  if (f) { buf.resize(f.size()); f.read(buf.data(), buf.size()); f.close(); }
  Apple2::Model m = apple.setRom(buf.data(), buf.size());
  romCustom = m != Apple2::NO_ROM;
  romCrc = romCustom ? ~crc32(buf.data(), buf.size()) : 0;
  if (m == Apple2::APPLE2E) {
    // the enhanced IIe ROM has 65C02 code; tell the two apart by the version byte at $FBC0
    uint8_t v = buf[buf.size() - 16384 + 0x3BC0];
    romName = v == 0xE0 ? "Apple IIe enhanced" : v == 0xEA ? "Apple IIe" : "Apple IIe-compatible";
  } else if (m == Apple2::APPLE2PLUS) romName = "Apple ][+";
  else romName = "No ROM";
}

// ---------------- OLED (SSD1306 128x64)
struct Oled {
  uint8_t buf[1024];
  bool ok = false;
  void cmd(uint8_t c) { Wire.beginTransmission(0x3C); Wire.write(0x00); Wire.write(c); Wire.endTransmission(); }
  void begin() {
    pinMode(PIN_VEXT, OUTPUT); digitalWrite(PIN_VEXT, LOW);
    delay(50);
    pinMode(PIN_OLED_RST, OUTPUT);
    digitalWrite(PIN_OLED_RST, LOW); delay(20); digitalWrite(PIN_OLED_RST, HIGH); delay(20);
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL, 400000);
    Wire.beginTransmission(0x3C);
    ok = Wire.endTransmission() == 0;
    if (!ok) return;
    const uint8_t init[] = {0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40, 0x8D, 0x14, 0x20, 0x00, 0xA1, 0xC8,
                            0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
    for (uint8_t c : init) cmd(c);
    clear(); show();
  }
  void clear() { memset(buf, 0, sizeof buf); }
  void pset(int x, int y) { if (x >= 0 && x < 128 && y >= 0 && y < 64) buf[(y >> 3) * 128 + x] |= 1 << (y & 7); }
  void text(int col, int row, const char* s, bool inv = false) {  // 6-pixel cells: 21 columns
    for (; *s && col < 21; s++, col++) {
      int ch = (unsigned char)*s;
      if (ch < 0x20 || ch > 0x7F) ch = '?';
      for (int y = 0; y < 8; y++) {
        uint8_t bits = A2_FONT[(ch - 0x20) * 8 + y];
        if (inv) bits ^= 0x7F;
        for (int x = 0; x < 6; x++) if (bits & (0x40 >> x)) pset(col * 6 + x, row * 8 + y);
      }
    }
  }
  void show() {
    if (!ok) return;
    cmd(0x21); cmd(0); cmd(127); cmd(0x22); cmd(0); cmd(7);
    for (int i = 0; i < 1024; i += 16) {
      Wire.beginTransmission(0x3C);
      Wire.write(0x40);
      Wire.write(buf + i, 16);
      Wire.endTransmission();
    }
  }
} oled;

static void drawOled() {
  oled.clear();
  if (oledPage == 1) {  // miniature of the Apple screen
    uint16_t vm = apple.videoMode();
    const uint8_t* mem = apple.mainMem();
    bool p2 = (vm & VM_PAGE2) && !(vm & VM_80STORE);
    for (int oy = 0; oy < 64; oy++) {
      int y = oy * 3;
      for (int ox = 0; ox < 128; ox++) {
        int x = ox * 280 / 128;
        bool on;
        if (!(vm & VM_TEXT) && !((vm & VM_MIXED) && y >= 160)) {
          if (vm & VM_HIRES) {
            uint16_t ad = (p2 ? 0x4000 : 0x2000) + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28 + x / 7;
            on = ((mem[ad] >> (x % 7)) & 1) || ((mem[ad] >> ((x + 1) % 7)) & 1);
          } else {
            int r = y >> 3;
            uint8_t b = mem[(p2 ? 0x800 : 0x400) + (r & 7) * 0x80 + (r >> 3) * 0x28 + x / 7];
            on = ((y & 7) < 4 ? b & 15 : b >> 4) != 0;
          }
        } else {
          int r = y >> 3;
          uint8_t b = mem[(p2 ? 0x800 : 0x400) + (r & 7) * 0x80 + (r >> 3) * 0x28 + x / 7];
          int c = b & 0x7F;
          if (c < 0x20) c += 0x40;
          if (!(b & 0x80) && b < 0x40) c = (b & 0x3F) < 0x20 ? (b & 0x3F) + 0x40 : (b & 0x3F);
          uint8_t bits = A2_FONT[(c - 0x20) * 8 + (y & 7)];
          if (b < 0x40) bits ^= 0x7F;
          on = bits & (0x40 >> (x % 7));
        }
        if (on) oled.pset(ox, oy);
      }
    }
  } else {
    char line[32];
    oled.text(0, 0, "   HELTEC-APPLE ][   ", true);
    if (apMode) {
      oled.text(0, 2, "WI-FI SETUP:");
      oled.text(0, 3, AP_SSID);
      oled.text(0, 4, "password: appleiie");
      oled.text(0, 5, "192.168.4.1/settings");
    } else {
      oled.text(0, 2, "Open in a browser:");
      oled.text(0, 3, WiFi.localIP().toString().c_str());
      oled.text(0, 4, WiFi.status() == WL_CONNECTED ? "or apple2.local" : "Wi-Fi lost...");
    }
    snprintf(line, sizeof line, "%s %d%%", stream.active() ? (stream.soundOn() ? "Playing+snd" : "Playing") : "No viewer", speedPct);
    oled.text(0, 6, line);
    if (!romCustom) oled.text(0, 7, "Upload a ROM!");
    else {
      String d = disks[0].name.length() ? "D1:" + disks[0].name : String("D1 empty");
      oled.text(0, 7, d.c_str());
    }
  }
  oled.show();
}

// ---------------- stream transport over WiFiClient
struct WifiConn : NetConn {
  WiFiClient c;
  explicit WifiConn(WiFiClient cl) : c(cl) { c.setNoDelay(true); }
  int read(uint8_t* b, int n) override {
    int a = c.available();
    if (a <= 0) return 0;
    return c.read(b, a < n ? a : n);
  }
  bool write(const uint8_t* b, int n) override {
    int idle = 0;
    while (n > 0) {
      size_t w = c.write(b, n);
      if (w == 0) {
        if (!c.connected() || ++idle > 3000) return false;
        delay(1);
        continue;
      }
      idle = 0; b += w; n -= w;
    }
    return true;
  }
  bool connected() override { return c.connected(); }
  void close() override { c.stop(); }
};

// ---------------- emulator task (core 1)
static void emuTask(void*) {
  int64_t next = esp_timer_get_time();
  volatile bool resetReq = false;
  for (;;) {
    if (cmdRom) { cmdRom = false; loadRom(); cmdPower = true; }
    for (int d = 0; d < 2; d++) {
      if (wantPending[d]) {
        String n;
        portENTER_CRITICAL(&cmdMux);
        n = wantName[d]; wantPending[d] = false;
        portEXIT_CRITICAL(&cmdMux);
        doInsert(d, n);
      }
      if (disks[d].name.length() && disks[d].writeProtected != wpWant[d]) {
        apple.flushDisks();
        disks[d].writeProtected = wpWant[d];
      }
    }
    if (wantBoot) { wantBoot = false; cmdPower = true; }
    if (cmdPower) { cmdPower = false; apple.flushDisks(); apple.powerOn(); }
    input.apply(apple, resetReq);
    if (cmdReset || resetReq) { cmdReset = false; resetReq = false; apple.reset(); }
    apple.runFrame();
    int64_t now = esp_timer_get_time();
    next += (int64_t)A2_FRAME_US;
    // warp: run flat out (the stream still shows every frame it can); also speeds up disk loading
    if (warp) next = now;
    if (now > next + 200000) next = now;
    while ((now = esp_timer_get_time()) < next) {
      if (next - now > 1500) vTaskDelay(1);
      else taskYIELD();
    }
    if (warp && (apple.frameCount & 7) == 0) vTaskDelay(1);  // let the watchdog breathe
  }
}

// ---------------- web API
static String jsonEsc(const String& s) {
  String o;
  for (char ch : s) {
    if (ch == '"' || ch == '\\') { o += '\\'; o += ch; }
    else if ((uint8_t)ch < 0x20) o += ' ';
    else o += ch;
  }
  return o;
}

static void setupWeb() {
  web.on("/", HTTP_GET, [] { web.send_P(200, "text/html", PLAYER_HTML); });
  web.on("/settings", HTTP_GET, [] { web.send_P(200, "text/html", SETTINGS_HTML); });

  web.on("/api/status", HTTP_GET, [] {
    char crc[12];
    snprintf(crc, sizeof crc, "%08X", (unsigned)romCrc);
    String j = "{";
    j += "\"ip\":\"" + (apMode ? String("192.168.4.1") : WiFi.localIP().toString()) + "\",";
    j += "\"mode\":\"" + String(apMode ? "ap" : "sta") + "\",";
    j += "\"ssid\":\"" + jsonEsc(WiFi.SSID()) + "\",\"rssi\":" + String(WiFi.RSSI()) + ",";
    j += "\"viewer\":" + String(stream.active() ? "true" : "false") + ",\"sound\":" + String(stream.soundOn() ? "true" : "false") + ",";
    j += "\"speed\":" + String(speedPct) + ",\"warp\":" + String(warp ? "true" : "false") + ",";
    j += "\"romName\":\"" + jsonEsc(romName) + "\",\"romCustom\":" + String(romCustom ? "true" : "false") + ",\"romCrc\":\"" + crc + "\",";
    j += "\"drives\":[";
    for (int d = 0; d < 2; d++)
      j += String(d ? "," : "") + "{\"name\":\"" + jsonEsc(disks[d].name) + "\",\"wp\":" + (wpWant[d] ? "true" : "false") +
           ",\"track\":" + String(apple.driveTrack(d)) + ",\"busy\":" + (apple.driveActive(d) ? "true" : "false") + "}";
    j += "],\"disks\":[";
    std::vector<std::pair<String, uint32_t>> files;
    listDisks(files);
    for (size_t i = 0; i < files.size(); i++)
      j += String(i ? "," : "") + "{\"n\":\"" + jsonEsc(files[i].first) + "\",\"s\":" + String(files[i].second) + "}";
    j += "],\"fsfree\":" + String((uint32_t)(LittleFS.totalBytes() - LittleFS.usedBytes()));
    j += ",\"heap\":" + String(ESP.getFreeHeap()) + "}";
    web.send(200, "application/json", j);
  });

  web.on("/api/reset", HTTP_POST, [] { cmdReset = true; web.send(200, "text/plain", "ok"); });
  web.on("/api/power", HTTP_POST, [] { cmdPower = true; web.send(200, "text/plain", "ok"); });
  web.on("/api/warp", HTTP_POST, [] { warp = web.arg("on") == "1"; web.send(200, "text/plain", "ok"); });
  web.on("/api/type", HTTP_POST, [] { apple.typeText(web.arg("plain").c_str()); web.send(200, "text/plain", "ok"); });

  // ---- ROM upload (12 / 16 / 20 / 32 KB)
  static std::vector<uint8_t> romBuf;
  static bool romTooBig = false;
  web.on("/api/rom", HTTP_POST, [] {
    size_t n = romBuf.size();
    if (romTooBig || !(n == 12288 || n == 16384 || n == 20480 || n == 32768)) {
      romBuf.clear();
      web.send(400, "text/plain", "That doesn't look like an Apple II ROM: use a 16 KB Apple IIe or 12 KB ][+ file (got " + String(n) + " bytes)");
      return;
    }
    File f = LittleFS.open(ROM_PATH, "w");
    if (!f) { web.send(500, "text/plain", "Flash full?"); return; }
    f.write(romBuf.data(), n);
    f.close();
    romBuf.clear();
    romBuf.shrink_to_fit();
    cmdRom = true;
    web.send(200, "text/plain", "ok");
  }, [] {
    HTTPUpload& u = web.upload();
    if (u.status == UPLOAD_FILE_START) { romBuf.clear(); romTooBig = false; }
    else if (u.status == UPLOAD_FILE_WRITE) {
      if (romBuf.size() + u.currentSize > 32768) romTooBig = true;
      else romBuf.insert(romBuf.end(), u.buf, u.buf + u.currentSize);
    }
  });
  web.on("/api/romdel", HTTP_POST, [] { LittleFS.remove(ROM_PATH); cmdRom = true; web.send(200, "text/plain", "ok"); });

  // ---- disk images, streamed to LittleFS
  static File up;
  static String upName;
  static bool upOk = false;
  web.on("/api/disk", HTTP_POST, [] {
    size_t sz = up ? up.size() : 0;
    if (up) up.close();
    if (!upOk || sz != 143360) {
      LittleFS.remove("/disks/upload.tmp");
      web.send(400, "text/plain", upOk ? "Not a 140 KB .dsk/.do/.po image (" + String(sz) + " bytes; .nib/.woz aren't supported)" : "Flash full?");
      return;
    }
    for (int d = 0; d < 2; d++)
      if (disks[d].name == upName) { requestInsert(d, "", false); delay(60); }
    LittleFS.remove("/disks/" + upName);
    LittleFS.rename("/disks/upload.tmp", "/disks/" + upName);
    if (web.arg("boot") == "1") requestInsert(0, upName, true);
    web.send(200, "text/plain", "ok");
  }, [] {
    HTTPUpload& u = web.upload();
    if (u.status == UPLOAD_FILE_START) {
      upName = cleanName(u.filename);
      String l = upName; l.toLowerCase();
      if (!(l.endsWith(".dsk") || l.endsWith(".do") || l.endsWith(".po"))) upName += ".dsk";
      if (upName.length() < 5) upName = "DISK.dsk";
      up = LittleFS.open("/disks/upload.tmp", "w");
      upOk = (bool)up;
    } else if (u.status == UPLOAD_FILE_WRITE) {
      if (upOk && (up.size() + u.currentSize > 143360 || up.write(u.buf, u.currentSize) != u.currentSize)) upOk = false;
    }
  });
  web.on("/api/insert", HTTP_POST, [] {
    int d = web.arg("drive") == "1" ? 1 : 0;
    String n = cleanName(web.arg("name"));
    if (n.length() && !LittleFS.exists("/disks/" + n)) { web.send(404, "text/plain", "No such disk image"); return; }
    int other = d ^ 1;
    if (n.length() && disks[other].name == n) requestInsert(other, "", false);  // one image, one drive
    requestInsert(d, n, web.arg("boot") == "1");
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/wp", HTTP_POST, [] {
    int d = web.arg("drive") == "1" ? 1 : 0;
    wpWant[d] = web.arg("on") == "1";
    prefs.putBool(d ? "wp2" : "wp1", wpWant[d]);
    web.send(200, "text/plain", "ok");
  });
  web.on("/api/diskdel", HTTP_POST, [] {
    String n = cleanName(web.arg("name"));
    for (int d = 0; d < 2; d++)
      if (disks[d].name == n) requestInsert(d, "", false);
    delay(80);  // let the emulator task close it
    LittleFS.remove("/disks/" + n);
    web.send(200, "text/plain", "ok");
  });
  web.onNotFound([] {
    String u = web.uri();
    if (u.startsWith("/api/dl/")) {
      String n = cleanName(web.urlDecode(u.substring(8)));
      apple.flushDisks();
      File f = LittleFS.open("/disks/" + n, "r");
      if (f) {
        web.sendHeader("Content-Disposition", "attachment; filename=\"" + n + "\"");
        web.streamFile(f, "application/octet-stream");
        f.close();
        return;
      }
    }
    if (apMode) { web.sendHeader("Location", "http://192.168.4.1/settings"); web.send(302); return; }
    web.send(404, "text/plain", "Not found");
  });

  // ---- Wi-Fi
  web.on("/api/scan", HTTP_GET, [] {
    int n = WiFi.scanNetworks();
    String j = "[";
    for (int i = 0; i < n; i++) j += String(i ? "," : "") + "{\"s\":\"" + jsonEsc(WiFi.SSID(i)) + "\",\"r\":" + String(WiFi.RSSI(i)) + "}";
    j += "]";
    WiFi.scanDelete();
    web.send(200, "application/json", j);
  });
  web.on("/api/wifi", HTTP_POST, [] {
    prefs.putString("ssid", web.arg("ssid"));
    prefs.putString("pass", web.arg("pass"));
    web.send(200, "text/plain", "ok");
    apple.flushDisks();
    delay(600);
    ESP.restart();
  });
  web.begin();
}

// ---------------- network task (core 0)
static void netTask(void*) {
  uint32_t lastOled = 0, lastSpeed = millis(), lastFrames = apple.frameCount;
  uint32_t btnDown = 0;
  bool btnHeld = false;
  for (;;) {
    web.handleClient();
    WiFiClient nc = streamListener.accept();
    if (nc) stream.attach(new WifiConn(nc));
    stream.poll();
    digitalWrite(PIN_LED, stream.active() ? HIGH : LOW);

    uint32_t now = millis();
    if (now - lastSpeed >= 1000) {
      uint32_t f = apple.frameCount;
      speedPct = (int)((f - lastFrames) * 100000.0 / (now - lastSpeed) / 59.92 + 0.5);
      lastFrames = f; lastSpeed = now;
    }
    bool down = digitalRead(PIN_BUTTON) == LOW;
    if (down && !btnDown) { btnDown = now ? now : 1; btnHeld = false; }
    if (down && btnDown && !btnHeld && now - btnDown > 2000) { btnHeld = true; cmdReset = true; }
    if (!down && btnDown) {
      if (!btnHeld && now - btnDown > 40) oledPage ^= 1;
      btnDown = 0;
      lastOled = 0;
    }
    if (now - lastOled > (oledPage ? 150u : 1000u)) { drawOled(); lastOled = now; }
    delay(1);
  }
}

// ---------------- setup
void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  if (!LittleFS.begin(true)) Serial.println("LittleFS mount failed");
  LittleFS.mkdir("/disks");
  LittleFS.remove("/disks/upload.tmp");
  prefs.begin("apple2", false);

  loadRom();
  wpWant[0] = prefs.getBool("wp1", false);
  wpWant[1] = prefs.getBool("wp2", false);
  doInsert(0, prefs.getString("d1", ""));
  doInsert(1, prefs.getString("d2", ""));
  apple.powerOn();

  oled.begin();
  oled.clear(); oled.text(0, 0, "   HELTEC-APPLE ][   ", true); oled.text(0, 3, "Joining Wi-Fi..."); oled.show();

  String ssid = prefs.getString("ssid", ""), pass = prefs.getString("pass", "");
  WiFi.setHostname(HOSTNAME);
  if (ssid.length()) {
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(ssid.c_str(), pass.c_str());
    for (int i = 0; i < 150 && WiFi.status() != WL_CONNECTED; i++) delay(100);
  }
  if (WiFi.status() != WL_CONNECTED) {
    apMode = true;
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
  } else {
    WiFi.setAutoReconnect(true);
  }
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  Serial.printf("Heltec-AppleII ready: http://%s/  (free heap %u)\n", apMode ? "192.168.4.1" : WiFi.localIP().toString().c_str(), ESP.getFreeHeap());

  stream.onPaste = [](const std::string& t) { apple.typeText(t.c_str()); };
  streamListener.begin();
  streamListener.setNoDelay(true);
  setupWeb();

  xTaskCreatePinnedToCore(emuTask, "apple2", 8192, nullptr, 3, nullptr, 1);
  xTaskCreatePinnedToCore(netTask, "net", 12288, nullptr, 2, nullptr, 0);
}

void loop() { vTaskDelete(nullptr); }

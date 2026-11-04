#include "stream.h"
#include <string.h>
#include <ctype.h>

// ---------------------------------------------------------------- SHA-1 + base64 (WebSocket handshake)
static void sha1(const uint8_t* data, size_t len, uint8_t out[20]) {
  uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
  std::vector<uint8_t> m(data, data + len);
  uint64_t bits = (uint64_t)len * 8;
  m.push_back(0x80);
  while (m.size() % 64 != 56) m.push_back(0);
  for (int i = 7; i >= 0; i--) m.push_back((uint8_t)(bits >> (i * 8)));
  auto rol = [](uint32_t x, int n) { return (x << n) | (x >> (32 - n)); };
  for (size_t off = 0; off < m.size(); off += 64) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++)
      w[i] = (m[off + i * 4] << 24) | (m[off + i * 4 + 1] << 16) | (m[off + i * 4 + 2] << 8) | m[off + i * 4 + 3];
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
      uint32_t f, k;
      if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
      else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
      else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
      else { f = b ^ c ^ d; k = 0xCA62C1D6; }
      uint32_t t = rol(a, 5) + f + e + k + w[i];
      e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
  }
  for (int i = 0; i < 5; i++) for (int j = 0; j < 4; j++) out[i * 4 + j] = (uint8_t)(h[i] >> (24 - j * 8));
}

static std::string base64(const uint8_t* d, size_t n) {
  static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  for (size_t i = 0; i < n; i += 3) {
    uint32_t v = d[i] << 16 | (i + 1 < n ? d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
    o += T[(v >> 18) & 63];
    o += T[(v >> 12) & 63];
    o += i + 1 < n ? T[(v >> 6) & 63] : '=';
    o += i + 2 < n ? T[v & 63] : '=';
  }
  return o;
}

std::string wsAcceptKey(const std::string& key) {
  std::string s = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  uint8_t h[20];
  sha1((const uint8_t*)s.data(), s.size(), h);
  return base64(h, 20);
}

// ---------------------------------------------------------------- connection
void StreamServer::attach(NetConn* c) {
  if (conn) {
    if (state == OPEN) {
      const char* t = "\x04" "Another viewer took over";
      sendWs((const uint8_t*)t, strlen(t));
    }
    drop();
  }
  conn = c;
  state = HANDSHAKE;
  in.clear();
  sound = false;
  inFlight = 0;
  lastFrame = 0xFFFFFFFF;
  lastMode = 0xFFFF;
}

void StreamServer::drop() {
  if (!conn) return;
  conn->close();
  delete conn;
  conn = nullptr;
  sound = false;
  input.push(0x7FFFFFFF);  // release anything the viewer was holding
}

void StreamServer::poll() {
  if (!conn) return;
  if (!conn->connected()) { drop(); return; }
  uint8_t buf[512];
  for (int guard = 0; guard < 16; guard++) {
    int n = conn->read(buf, sizeof buf);
    if (n <= 0) break;
    in.insert(in.end(), buf, buf + n);
    if (in.size() > 70000) { drop(); return; }
  }
  if (state == HANDSHAKE && !doHandshake()) return;
  while (conn && readFrame()) {}
  if (!conn) return;
  sendAudio();
  if (conn) sendVideo();
}

bool StreamServer::doHandshake() {
  std::string req(in.begin(), in.end());
  size_t end = req.find("\r\n\r\n");
  if (end == std::string::npos) {
    if (in.size() > 4096) drop();
    return false;
  }
  std::string lower = req.substr(0, end);
  for (auto& ch : lower) ch = tolower(ch);
  size_t k = lower.find("sec-websocket-key:");
  if (k == std::string::npos) {
    const char* r = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
    conn->write((const uint8_t*)r, strlen(r));
    drop();
    return false;
  }
  k += 18;
  size_t e = req.find("\r\n", k);
  std::string key = req.substr(k, e - k);
  while (!key.empty() && key.front() == ' ') key.erase(0, 1);
  while (!key.empty() && key.back() == ' ') key.pop_back();
  std::string resp = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " +
                     wsAcceptKey(key) + "\r\n\r\n";
  conn->write((const uint8_t*)resp.data(), resp.size());
  in.erase(in.begin(), in.begin() + end + 4);
  state = OPEN;
  uint8_t hello[6] = {0, (uint8_t)m.model(), (uint8_t)(A2_AUDIO_RATE & 255), (uint8_t)((A2_AUDIO_RATE >> 8) & 255), 0, 0};
  sendWs(hello, sizeof hello);
  m.markAllDirty();
  return true;
}

bool StreamServer::readFrame() {
  if (in.size() < 2) return false;
  uint8_t op = in[0] & 0x0F;
  bool masked = in[1] & 0x80;
  uint64_t len = in[1] & 0x7F;
  size_t hdr = 2;
  if (len == 126) { if (in.size() < 4) return false; len = (in[2] << 8) | in[3]; hdr = 4; }
  else if (len == 127) {
    if (in.size() < 10) return false;
    len = 0;
    for (int i = 0; i < 8; i++) len = (len << 8) | in[2 + i];
    hdr = 10;
  }
  if (len > 65536) { drop(); return false; }
  size_t total = hdr + (masked ? 4 : 0) + len;
  if (in.size() < total) return false;
  uint8_t* p = in.data() + hdr + (masked ? 4 : 0);
  if (masked) {
    const uint8_t* mk = in.data() + hdr;
    for (uint64_t i = 0; i < len; i++) p[i] ^= mk[i & 3];
  }
  if (op == 8) { drop(); return false; }
  if (op == 9) {
    std::vector<uint8_t> pong = {0x8A, (uint8_t)len};
    pong.insert(pong.end(), p, p + len);
    conn->write(pong.data(), pong.size());
  } else if (op == 1 || op == 2) {
    handleMessage(p, (size_t)len);
  }
  if (conn) in.erase(in.begin(), in.begin() + total);
  return conn != nullptr;
}

void StreamServer::handleMessage(const uint8_t* d, size_t n) {
  if (n < 1) return;
  switch (d[0]) {
    case 0x10:
      if (n >= 6) {
        uint32_t code = d[2] | (d[3] << 8) | (d[4] << 16) | ((uint32_t)d[5] << 24);
        input.push((code & 0xFFFF) | (d[1] ? 0x80000000u : 0));
      }
      break;
    case 0x11:
      if (onPaste) onPaste(std::string((const char*)d + 1, n - 1));
      break;
    case 0x12: m.markAllDirty(); lastMode = 0xFFFF; break;
    case 0x13: if (inFlight > 0) inFlight--; break;
    case 0x14:
      if (n >= 2) {
        sound = d[1] != 0;
        m.audioEnabled = sound;
        int16_t junk[256];
        while (m.readAudio(junk, 256) > 0) {}
      }
      break;
    case 0x16:
      if (n >= 3) { m.paddle[0] = d[1]; m.paddle[1] = d[2]; }
      break;
    case 0x17:
      if (n >= 2) m.capsLock = d[1] != 0;
      break;
  }
}

void StreamServer::sendWs(const uint8_t* data, size_t n) {
  if (!conn) return;
  uint8_t h[10];
  int hl = 0;
  h[hl++] = 0x82;
  if (n < 126) h[hl++] = (uint8_t)n;
  else if (n < 65536) { h[hl++] = 126; h[hl++] = n >> 8; h[hl++] = n & 255; }
  else { h[hl++] = 127; for (int i = 7; i >= 0; i--) h[hl++] = (uint8_t)((uint64_t)n >> (i * 8)); }
  bytesSent += hl + n;
  if (n < 512) {  // small: one write
    uint8_t b[522];
    memcpy(b, h, hl); memcpy(b + hl, data, n);
    if (!conn->write(b, hl + n)) drop();
    return;
  }
  if (!conn->write(h, hl) || !conn->write(data, n)) drop();
}

void StreamServer::sendAudio() {
  if (!sound) return;
  if (m.audioAvailable() < 367) return;  // about one frame of sound per message
  static int16_t smp[1024];
  int n = m.readAudio(smp, 1024);
  msg.assign(1 + n * 2, 0);
  msg[0] = 0x02;
  for (int i = 0; i < n; i++) { msg[1 + i * 2] = smp[i] & 255; msg[2 + i * 2] = (uint16_t)smp[i] >> 8; }
  sendWs(msg.data(), msg.size());
}

static bool takeDirty(Apple2& m, int id) {
  uint32_t bitm = 1u << (id & 31);
  if (!(m.dirty[id >> 5] & bitm)) return false;
  __atomic_fetch_and(&m.dirty[id >> 5], ~bitm, __ATOMIC_SEQ_CST);
  return true;
}

void StreamServer::sendVideo() {
  if (inFlight >= 2) return;
  uint32_t fc = m.frameCount;
  if (fc == lastFrame) return;
  lastFrame = fc;
  uint16_t mode = m.videoMode();
  uint8_t flags = (m.driveActive(0) ? 1 : 0) | (m.driveActive(1) ? 2 : 0);

  // which pages are on screen now go first; everything else follows within a per-frame budget
  bool p2 = (mode & VM_PAGE2) && !(mode & VM_80STORE);
  int tp = p2 ? 0x08 : 0x04, hp = p2 ? 0x40 : 0x20;
  bool showText = (mode & VM_TEXT) || (mode & VM_MIXED) || !(mode & VM_HIRES);
  bool showHires = !(mode & VM_TEXT) && (mode & VM_HIRES);
  bool showAux = (mode & VM_80COL) || (mode & VM_DHIRES);
  std::vector<int> order;
  order.reserve(160);
  for (int bank = 0; bank < 2; bank++) {
    if (bank && !showAux) continue;
    if (showText) for (int p = tp; p < tp + 4; p++) order.push_back(bank * 256 + p);
    if (showHires) for (int p = hp; p < hp + 32; p++) order.push_back(bank * 256 + p);
  }
  size_t firstOther = order.size();
  for (int bank = 0; bank < 2; bank++) {
    for (int p = 0x04; p < 0x0C; p++) order.push_back(bank * 256 + p);
    for (int p = 0x20; p < 0x60; p++) order.push_back(bank * 256 + p);
  }
  int budget = 64, count = 0;
  msg.assign(2, 0);
  msg[0] = 0x01;
  uint8_t sent[64] = {0};
  for (size_t i = 0; i < order.size() && budget > 0 && conn; i++) {
    int id = order[i];
    if (i >= firstOther && (sent[id >> 3] & (1 << (id & 7)))) continue;
    if (!takeDirty(m, id)) continue;
    sent[id >> 3] |= 1 << (id & 7);
    const uint8_t* src = (id >= 256 ? m.auxMem() : m.mainMem()) + (id & 255) * 256;
    bool solid = true;
    for (int k = 1; k < 256; k++) if (src[k] != src[0]) { solid = false; break; }
    msg.push_back(id & 255);
    msg.push_back((id >> 8) | (solid ? 0x80 : 0));
    if (solid) msg.push_back(src[0]);
    else msg.insert(msg.end(), src, src + 256);
    count++;
    budget -= solid ? 0 : 1;
    if (count == 8) {  // keep messages small (~2 KB) to save RAM on the ESP32
      msg[1] = (uint8_t)count;
      sendWs(msg.data(), msg.size());
      msg.assign(2, 0); msg[0] = 0x01; count = 0;
    }
  }
  if (count && conn) { msg[1] = (uint8_t)count; sendWs(msg.data(), msg.size()); }
  if (!conn) return;
  uint8_t end[4] = {0x03, (uint8_t)(mode & 255), (uint8_t)(mode >> 8), flags};
  sendWs(end, 4);
  inFlight++;
  lastMode = mode;
  lastFlags = flags;
}

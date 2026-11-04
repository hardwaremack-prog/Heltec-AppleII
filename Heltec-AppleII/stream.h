// Streams the Apple II to a web browser over a WebSocket (port 81).
// The board sends the Apple's video memory (only the 256-byte pages that changed) plus the
// display soft switches; the browser draws text / lo-res / hi-res / double hi-res itself.
// Transport is abstract so the same code runs on the ESP32 (WiFiClient) and on a PC (sockets).
//
// Server -> browser (binary messages, little endian)
//   0x00 hello      : model u8 (0 none, 1 II+, 2 IIe), sampleRate u32
//   0x01 pages      : count u8, then per page: id u16 (bank*256+page; bit 15 = solid: 1 byte follows),
//                     then 256 bytes (or 1 byte when solid)
//   0x02 audio      : int16 samples
//   0x03 frame end  : videoMode u16, flags u8 (bit0 drive 1 busy, bit1 drive 2 busy)
//                     (browser draws, then sends 0x13 ack)
//   0x04 info       : utf-8 text
// Browser -> server
//   0x10 key        : down u8, code u32 (7-bit Apple key code; 0x100 open apple, 0x101 solid apple,
//                     0x102 RESET, 0x103 button 3)
//   0x11 paste      : utf-8 text to type
//   0x12 refresh    : resend all video memory
//   0x13 frame ack
//   0x14 sound      : on u8
//   0x16 joystick   : x u8, y u8 (paddles 0/1, 127 = centre)
//   0x17 caps lock  : on u8
#pragma once
#include <stdint.h>
#include <string>
#include <vector>
#include <functional>
#include "apple2.h"

struct NetConn {
  virtual ~NetConn() {}
  virtual int read(uint8_t* buf, int n) = 0;          // non-blocking; bytes read (0 = none)
  virtual bool write(const uint8_t* buf, int n) = 0;  // blocking; false = connection lost
  virtual bool connected() = 0;
  virtual void close() = 0;
};

// Input events from the network side, applied by the emulator side once per frame (lock-free SPSC).
struct InputQueue {
  static const int N = 256;
  uint32_t ev[N];
  volatile int head = 0, tail = 0;
  void push(uint32_t e) {
    int n = (head + 1) % N;
    if (n == tail) return;
    ev[head] = e;
    head = n;
  }
  // events: bit 31 = down, low bits = code; 0x7FFFFFFF = release everything
  void apply(Apple2& m, volatile bool& resetReq) {
    while (tail != head) {
      uint32_t e = ev[tail];
      tail = (tail + 1) % N;
      if (e == 0x7FFFFFFF) { m.releaseAll(); continue; }
      bool down = e >> 31;
      uint32_t code = e & 0xFFFF;
      if (code < 0x80) { if (down) m.keyDown((uint8_t)code); else m.keyUp(); }
      else if (code >= 0x100 && code <= 0x101) m.buttons[code - 0x100] = down;
      else if (code == 0x103) m.buttons[2] = down;
      else if (code == 0x102 && down) resetReq = true;
    }
  }
};

class StreamServer {
 public:
  StreamServer(Apple2& a, InputQueue& q) : m(a), input(q) {}
  void attach(NetConn* conn);   // takes ownership; replaces any current viewer
  void poll();                  // call often (every ~1-5 ms)
  bool active() const { return conn != nullptr && state == OPEN; }
  bool soundOn() const { return active() && sound; }
  std::function<void(const std::string&)> onPaste;
  uint32_t bytesSent = 0;

 private:
  enum State { HANDSHAKE, OPEN };
  Apple2& m;
  InputQueue& input;
  NetConn* conn = nullptr;
  State state = HANDSHAKE;
  std::vector<uint8_t> in, msg;
  bool sound = false;
  int inFlight = 0;
  uint32_t lastFrame = 0xFFFFFFFF;
  uint16_t lastMode = 0xFFFF;
  uint8_t lastFlags = 0xFF;

  void drop();
  bool doHandshake();
  bool readFrame();
  void handleMessage(const uint8_t* m, size_t n);
  void sendWs(const uint8_t* data, size_t n);
  void sendVideo();
  void sendAudio();
};

std::string wsAcceptKey(const std::string& clientKey);

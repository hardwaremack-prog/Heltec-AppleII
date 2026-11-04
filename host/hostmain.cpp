// PC test harness for Heltec-AppleII: same emulator + stream code as the ESP32 build.
//   ./a2host --rom apple2e.rom --disk game.dsk --frames 600 --text --ppm out.ppm   (headless)
//   ./a2host --rom apple2e.rom --disk game.dsk --serve 8181 --web 8180   (stream to the real web page)
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
#include <string>
#include <vector>
#include "../Heltec-AppleII/apple2.h"
#ifdef WITH_STREAM
#include "../Heltec-AppleII/stream.h"
#endif

static std::vector<uint8_t> loadFile(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), {});
}

struct FileDisk : DiskImage {
  std::string path;
  std::vector<uint8_t> data;
  bool save = false;
  bool readTrack(int t, uint8_t* b) override {
    if ((size_t)(t + 1) * 4096 > data.size()) return false;
    memcpy(b, data.data() + t * 4096, 4096);
    return true;
  }
  bool writeTrack(int t, const uint8_t* b) override {
    if ((size_t)(t + 1) * 4096 > data.size()) return false;
    memcpy(data.data() + t * 4096, b, 4096);
    if (save) { std::ofstream f(path, std::ios::binary); f.write((const char*)data.data(), data.size()); }
    return true;
  }
};

static void dumpText(Apple2& m) {
  const uint8_t* mem = m.mainMem();
  const uint8_t* aux = m.auxMem();
  bool c80 = m.videoMode() & VM_80COL;
  for (int r = 0; r < 24; r++) {
    uint16_t b = 0x400 + (r & 7) * 0x80 + (r >> 3) * 0x28;
    std::string s;
    for (int c = 0; c < 40; c++) {
      if (c80) { uint8_t ch = aux[b + c] & 0x7F; if (ch < 0x20) ch += 0x40; s += (char)ch; }
      uint8_t ch = mem[b + c] & 0x7F;
      if (ch < 0x20) ch += 0x40;
      s += (char)ch;
    }
    printf("|%s|\n", s.c_str());
  }
}

// quick hires/mono render for tests (the real renderer is in the web page)
static void dumpPPM(Apple2& m, const char* path) {
  uint16_t vm = m.videoMode();
  FILE* f = fopen(path, "wb");
  fprintf(f, "P6 280 192 255\n");
  const uint8_t* mem = m.mainMem();
  bool p2 = (vm & VM_PAGE2) && !(vm & VM_80STORE);
  for (int y = 0; y < 192; y++)
    for (int x = 0; x < 280; x++) {
      bool on;
      if (!(vm & VM_TEXT) && (vm & VM_HIRES) && !((vm & VM_MIXED) && y >= 160)) {
        uint16_t ad = (p2 ? 0x4000 : 0x2000) + (y & 7) * 0x400 + ((y >> 3) & 7) * 0x80 + (y >> 6) * 0x28 + x / 7;
        on = (mem[ad] >> (x % 7)) & 1;
      } else {
        int r = y / 8;
        uint16_t ad = (p2 ? 0x800 : 0x400) + (r & 7) * 0x80 + (r >> 3) * 0x28 + x / 7;
        on = mem[ad] != 0xA0 && (x % 7) > 0 && (x % 7) < 6 && (y % 8) < 7;
      }
      uint8_t v = on ? 255 : 0;
      fputc(v, f); fputc(v, f); fputc(v, f);
    }
  fclose(f);
}

int main(int argc, char** argv) {
  std::string rom, disk, disk2, ppm, typed;
  int frames = 300, typeAt = 0;
  bool text = false, writeback = false;
  int serve = 0, webPort = 0;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    auto nx = [&]() { return std::string(i + 1 < argc ? argv[++i] : ""); };
    if (a == "--rom") rom = nx();
    else if (a == "--disk") disk = nx();
    else if (a == "--disk2") disk2 = nx();
    else if (a == "--frames") frames = atoi(nx().c_str());
    else if (a == "--ppm") ppm = nx();
    else if (a == "--text") text = true;
    else if (a == "--writeback") writeback = true;
    else if (a == "--type") typed = nx();
    else if (a == "--type-at") typeAt = atoi(nx().c_str());
    else if (a == "--serve") serve = atoi(nx().c_str());
    else if (a == "--web") webPort = atoi(nx().c_str());
  }
  static Apple2 m;
  if (!rom.empty()) { auto r = loadFile(rom); m.setRom(r.data(), r.size()); }
  FileDisk d1, d2;
  auto setup = [](FileDisk& d, const std::string& p) {
    d.path = p; d.data = loadFile(p);
    d.prodosOrder = p.size() > 3 && (p.substr(p.size() - 3) == ".po" || p.substr(p.size() - 3) == ".PO");
  };
  d1.save = d2.save = writeback;
  if (!disk.empty()) { setup(d1, disk); m.insertDisk(0, &d1); }
  if (!disk2.empty()) { setup(d2, disk2); m.insertDisk(1, &d2); }
  m.powerOn();
#ifdef WITH_STREAM
  if (serve) {
    extern int runServer(Apple2 & m, int wsPort, int webPort);
    return runServer(m, serve, webPort);
  }
#endif
  (void)serve; (void)webPort;
  auto t0 = std::chrono::steady_clock::now();
  for (int f = 0; f < frames; f++) {
    if (f == typeAt && !typed.empty()) {
      std::string t;
      for (size_t i = 0; i < typed.size(); i++) {
        if (typed[i] == '\\' && i + 1 < typed.size() && typed[i + 1] == 'n') { t += '\n'; i++; }
        else t += typed[i];
      }
      m.typeText(t.c_str());
    }
    m.runFrame();
  }
  double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
  fprintf(stderr, "%d frames in %.0f ms (%.1fx real time) pc=%04X mode=%03X track=%d\n", frames, ms, frames * 16.688 / ms, m.pc,
          m.videoMode(), m.driveTrack(0));
  if (text) dumpText(m);
  if (!ppm.empty()) dumpPPM(m, ppm.c_str());
  return 0;
}

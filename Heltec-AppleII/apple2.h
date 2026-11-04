// Heltec-AppleII - a small Apple II / IIe emulator core
// 65C02 (or NMOS 6502 for a II+ ROM), 128K IIe memory with all soft switches and the language card,
// speaker sound, paddles/joystick, and a Disk II controller (slot 6, two drives) with its own boot PROM.
// Portable C++: runs on the ESP32-S3 (Heltec V3) and on a PC for testing.
// The screen is not drawn here: the web page draws it from the video memory this core exposes.
#pragma once
#include <stdint.h>
#include <string.h>
#include <string>
#include <functional>

static const int A2_CYCLES_PER_LINE = 65;
static const int A2_LINES = 262;
static const int A2_CYCLES_PER_FRAME = A2_CYCLES_PER_LINE * A2_LINES;  // 17030
static const double A2_CLOCK = 1020484.0;                              // NTSC Apple II
static const double A2_FRAME_US = A2_CYCLES_PER_FRAME * 1e6 / A2_CLOCK; // ~16688 us (59.92 Hz)
static const int A2_AUDIO_RATE = 22050;
static const int A2_TRACK_NIBBLES = 6656;

// Video soft-switch bits (Apple2::videoMode())
enum {
  VM_TEXT = 1, VM_MIXED = 2, VM_PAGE2 = 4, VM_HIRES = 8, VM_80COL = 16, VM_DHIRES = 32,
  VM_ALTCHAR = 64, VM_80STORE = 128, VM_IIE = 256
};

// A floppy image the drive reads/writes one track (16 sectors x 256 bytes, file order) at a time.
struct DiskImage {
  virtual ~DiskImage() {}
  virtual bool readTrack(int track, uint8_t* buf4096) = 0;
  virtual bool writeTrack(int track, const uint8_t* buf4096) = 0;
  bool prodosOrder = false;  // .po files (else DOS 3.3 order: .dsk/.do)
  bool writeProtected = false;
};

struct Drive {
  DiskImage* img = nullptr;
  uint8_t nib[A2_TRACK_NIBBLES];
  int loadedTrack = -1;
  bool dirty = false;
  int halfTrack = 0;
  int pos = 0;
  uint8_t phases = 0;
  uint64_t lastRead = 0;
};

class Apple2 {
 public:
  enum Model { NO_ROM = 0, APPLE2PLUS = 1, APPLE2E = 2 };
  Apple2();
  ~Apple2();

  // ROM image: 12 KB = II+ ($D000-$FFFF), 16 KB = IIe ($C000-$FFFF), 32 KB = IIe (last 16 KB used).
  // Anything else falls back to a tiny built-in screen asking for a ROM. Returns the model chosen.
  Model setRom(const uint8_t* data, size_t len);
  Model model() const { return mdl; }
  void powerOn();          // cold start: clears memory, resets
  void reset();            // the RESET key (Ctrl-Reset): soft switches off, CPU jumps via $FFFC
  void runFrame();         // emulate one NTSC frame (17030 cycles)
  volatile uint32_t frameCount = 0;

  // ---- keyboard / paddles / buttons
  void keyDown(uint8_t ascii);   // a key press (7-bit Apple code); auto-repeat is up to the caller
  void keyUp();                  // a key released
  void releaseAll() { keysHeld = 0; buttons[0] = buttons[1] = buttons[2] = 0; }
  bool typeText(const char* s);  // queue text to type (paste); '\n' -> RETURN
  uint8_t paddle[4] = {127, 127, 127, 127};
  uint8_t buttons[3] = {0, 0, 0};  // open apple, solid apple, shift-key mod
  bool capsLock = true;

  // ---- disks (slot 6, drive 0/1). insert: nullptr = eject. The image object stays owned by the caller.
  void insertDisk(int drive, DiskImage* img);
  void flushDisks();               // write any changed track back to its image
  bool driveActive(int d) const { return (motorOn || cycles < motorStopAt) && curDrive == d; }
  bool motorRunning() const { return motorOn; }
  int driveTrack(int d) const { return drives[d].halfTrack / 2; }

  // ---- sound: when audioEnabled, each frame renders ~367 samples (int16) into a ring
  volatile bool audioEnabled = false;
  int readAudio(int16_t* out, int max);
  int audioAvailable() const { return (aHead - aTail + AUDIO_RING) % AUDIO_RING; }

  // ---- video memory for the viewer (main/aux 64K each) and which 256-byte pages changed
  const uint8_t* mainMem() const { return ram; }
  const uint8_t* auxMem() const { return aux; }
  uint16_t videoMode() const;
  // dirty bit per 256-byte page, index = bank*256 + page (bank 0 = main, 1 = aux)
  volatile uint32_t dirty[16];
  void markAllDirty() { for (auto& d : dirty) d = 0xFFFFFFFF; }

  // ---- test hooks
  void cpuTestSetup(const uint8_t* img64k, uint16_t startPc, bool nmos);
  int cpuStep() { return step(); }
  uint8_t read(uint16_t a);
  void write(uint16_t a, uint8_t v);

  uint16_t pc; uint8_t a, x, y, sp, p;
  uint64_t cycles = 0;
  bool nmos = false;   // true for the II+ (NMOS 6502 with its undocumented opcodes)

 private:
  Model mdl = NO_ROM;
  uint8_t* ram;               // main 64K (LC bank 1 $D000-$DFFF is kept at $C000)
  uint8_t* aux;               // aux 64K (IIe), same layout
  uint8_t rom[16384];         // $C000-$FFFF image
  uint8_t slot6[256];         // Disk II boot PROM (our own code)
  const uint8_t* rpage[256];  // fast read map (nullptr = slow path)
  uint8_t* wpage[256];        // fast write map (nullptr = slow path)
  int16_t wdirty[256];        // dirty-page index for video pages written through wpage (-1 = not video)
  uint8_t trackTmp[4096];

  // soft switches
  bool sw80store = false, ramrd = false, ramwrt = false, intcxrom = false, altzp = false, slotc3rom = false;
  bool col80 = false, altchar = false, text = true, mixed = false, page2 = false, hires = false, dhires = false;
  bool intc8rom = false, ioudis = true;
  bool lcRam = false, lcBank2 = true, lcWrite = true;
  int lcPreWrite = 0;
  // keyboard
  uint8_t kbd = 0;            // bit 7 = strobe
  int keysHeld = 0;
  char tq[1024];
  volatile int tqHead = 0, tqTail = 0;
  // paddles
  uint64_t paddleTrigger = 0;
  // speaker
  bool spk = false;
  static const int SPK_MAX = 2560;
  uint16_t spkLog[SPK_MAX];   // cycle within frame of each toggle
  int spkN = 0;
  bool spkStartLevel = false;
  float dcLevel = 0;
  static const int AUDIO_RING = 2048;
  int16_t aRing[AUDIO_RING];
  volatile int aHead = 0, aTail = 0;
  double sampleCarry = 0;
  int frameStart = 0;         // cycle count at frame start (low bits)
  uint64_t frameCycle0 = 0;
  void finishAudio();
  // disk
  Drive drives[2];
  int curDrive = 0;
  bool motorOn = false, q6 = false, q7 = false;
  uint64_t motorStopAt = 0;
  uint8_t latch = 0;
  void diskLoadTrack(Drive& d);
  void diskFlush(Drive& d);
  uint8_t diskIO(int reg, bool isWrite, uint8_t v);
  void diskTrap(int n);

  void remap();
  uint8_t readSlow(uint16_t a);
  void writeSlow(uint16_t a, uint8_t v);
  uint8_t ioRead(uint16_t a);
  void ioWrite(uint16_t a, uint8_t v);
  void softSwitch(uint16_t a, bool isWrite);
  void lcAccess(uint16_t a, bool isWrite);
  uint8_t floatingBus();
  void pumpTypeBuffer();
  void buildBuiltinRom();

  int step();
  void doIrq(uint16_t vec, bool brk);
  inline void push(uint8_t v) { write(0x100 | sp--, v); }
  inline uint8_t pop() { return read(0x100 | ++sp); }
};

// Heltec-AppleII emulator core: 65C02/6502 CPU, Apple II/IIe memory and soft switches,
// language card, keyboard, paddles, speaker, Disk II (nibble level, with write-back).
#include "apple2.h"
#include <stdlib.h>

enum { FC = 1, FZ = 2, FI = 4, FD = 8, FB = 0x10, FU = 0x20, FV = 0x40, FN = 0x80 };

// ---------------------------------------------------------------- Disk II tables
static const uint8_t DOS_ORDER[16] = {0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15};
static const uint8_t PRODOS_ORDER[16] = {0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15};
static uint8_t NIB62[64];        // 6-bit value -> disk byte
static int8_t DENIB62[256];      // disk byte -> 6-bit value (-1 = invalid)

static void buildNibTables() {
  static bool done = false;
  if (done) return;
  done = true;
  int n = 0;
  memset(DENIB62, -1, sizeof DENIB62);
  for (int b = 0x96; b <= 0xFF && n < 64; b++) {
    // valid "6 and 2" disk bytes: high bit set, at least one pair of adjacent 1 bits (bits 0-6),
    // at most one pair of adjacent 0 bits
    bool adj1 = false;
    int zeroPairs = 0;
    for (int i = 0; i < 6; i++) {
      bool b0 = (b >> i) & 1, b1 = (b >> (i + 1)) & 1;
      if (b0 && b1) adj1 = true;
      if (!b0 && !b1) zeroPairs++;
    }
    if (!adj1 || zeroPairs > 1 || b == 0xAA || b == 0xD5) continue;
    NIB62[n] = (uint8_t)b;
    DENIB62[b] = (int8_t)n;
    n++;
  }
}

// Our own Disk II boot PROM for slot 6. Same entry points as the original (boot at $C600,
// "read sector $3D into ($26)" at $C65C, $C6FF = 0), but the low-level reading is done by the
// emulator through a trap opcode ($02 nn), so no Apple code is needed.
static void buildBootProm(uint8_t* r) {
  memset(r, 0xEA, 256);
  const uint8_t boot[] = {
      0xA2, 0x20,              // C600 LDX #$20   (signature bytes $20,$00,$03,$3C at 1,3,5,7)
      0xA0, 0x00,              // C602 LDY #$00
      0xA2, 0x03,              // C604 LDX #$03
      0x86, 0x3C,              // C606 STX $3C
      0x20, 0x58, 0xFF,        // C608 JSR $FF58  (an RTS in the monitor)
      0xBA,                    // C60B TSX
      0xBD, 0x00, 0x01,        // C60C LDA $0100,X  -> $Cn
      0x0A, 0x0A, 0x0A, 0x0A,  // C60F ASL x4       -> $n0
      0x85, 0x2B,              // C613 STA $2B
      0xAA,                    // C615 TAX
      0xBD, 0x8E, 0xC0,        // C616 LDA $C08E,X  read mode
      0xBD, 0x8C, 0xC0,        // C619 LDA $C08C,X
      0xBD, 0x8A, 0xC0,        // C61C LDA $C08A,X  drive 1
      0xBD, 0x89, 0xC0,        // C61F LDA $C089,X  motor on
      0x02, 0x01,              // C622 TRAP 1       recalibrate to track 0, build nibble table at $356
      0xA9, 0x00,              // C624 LDA #0
      0x85, 0x26,              // C626 STA $26
      0x85, 0x3D,              // C628 STA $3D
      0x85, 0x41,              // C62A STA $41
      0xA9, 0x08,              // C62C LDA #$08
      0x85, 0x27,              // C62E STA $27
      0x4C, 0x5C, 0xC6,        // C630 JMP $C65C
  };
  memcpy(r, boot, sizeof boot);
  const uint8_t rd[] = {
      0x02, 0x02,              // C65C TRAP 2       read sector $3D of this track into ($26)
      0xE6, 0x27,              // C65E INC $27
      0xE6, 0x3D,              // C660 INC $3D
      0xA5, 0x3D,              // C662 LDA $3D
      0xCD, 0x00, 0x08,        // C664 CMP $0800    sectors to load (first byte of the boot sector)
      0xA6, 0x2B,              // C667 LDX $2B
      0x90, 0xF1,              // C669 BCC $C65C
      0x4C, 0x01, 0x08,        // C66B JMP $0801
  };
  memcpy(r + 0x5C, rd, sizeof rd);
  r[0xFE] = 0x00;  // ID bytes some loaders check: $CnFE = 0 (Disk II), $CnFF = 0 (16-sector)
  r[0xFF] = 0x00;
}

// ---------------------------------------------------------------- setup
Apple2::Apple2() {
  ram = (uint8_t*)malloc(65536);
  aux = (uint8_t*)malloc(65536);
  memset(ram, 0, 65536);
  memset(aux, 0, 65536);
  buildNibTables();
  buildBootProm(slot6);
  buildBuiltinRom();
  for (int i = 0; i < 16; i++) dirty[i] = 0;
  pc = 0; a = x = y = 0; sp = 0xFF; p = FU | FI;
}

Apple2::~Apple2() {
  free(ram);
  free(aux);
}

void Apple2::buildBuiltinRom() {
  memset(rom, 0, sizeof rom);
  // tiny ROM: reset -> $F800: JMP $F800 (the message itself is put on screen by powerOn)
  rom[0x3800] = 0x4C; rom[0x3801] = 0x00; rom[0x3802] = 0xF8;
  rom[0x3FFC] = 0x00; rom[0x3FFD] = 0xF8;
  rom[0x3FFE] = 0x00; rom[0x3FFF] = 0xF8;
  rom[0x3FFA] = 0x00; rom[0x3FFB] = 0xF8;
}

Apple2::Model Apple2::setRom(const uint8_t* d, size_t len) {
  if (len == 12288) {
    memset(rom, 0, 0x1000);
    memcpy(rom + 0x1000, d, 12288);
    mdl = APPLE2PLUS;
  } else if (len == 16384 || len == 32768) {
    memcpy(rom, d + (len - 16384), 16384);
    mdl = APPLE2E;
  } else if (len == 20480) {  // $B000-$FFFF style dumps with a leading 4K: use the last 16K
    memcpy(rom, d + 4096, 16384);
    mdl = APPLE2E;
  } else {
    buildBuiltinRom();
    mdl = NO_ROM;
  }
  nmos = (mdl == APPLE2PLUS);
  return mdl;
}

static void putText(uint8_t* ram, int row, int col, const char* s) {
  uint16_t base = 0x400 + (row & 7) * 0x80 + (row >> 3) * 0x28;
  for (; *s && col < 40; s++, col++) {
    uint8_t c = (uint8_t)*s;
    if (c >= 'a' && c <= 'z') c -= 32;
    ram[base + col] = c | 0x80;
  }
}

void Apple2::powerOn() {
  // Real RAM powers up with a pattern; zeroes make the ROM do a cold start (power-up byte mismatch)
  memset(ram, 0, 65536);
  memset(aux, 0, 65536);
  if (mdl == NO_ROM) {
    for (int i = 0x400; i < 0x800; i++) ram[i] = 0xA0;
    putText(ram, 2, 2, "HELTEC-APPLEII");
    putText(ram, 5, 2, "NO APPLE II ROM YET.");
    putText(ram, 7, 2, "OPEN SETTINGS (GEAR BUTTON) AND");
    putText(ram, 8, 2, "UPLOAD YOUR ROM FILE:");
    putText(ram, 10, 4, "APPLE IIE ENHANCED  16 KB");
    putText(ram, 11, 4, "APPLE IIE           16 KB");
    putText(ram, 12, 4, "APPLE ][+           12 KB");
    putText(ram, 15, 2, "THEN PUT A .DSK OR .PO DISK");
    putText(ram, 16, 2, "IN DRIVE 1 AND PRESS RESET.");
  }
  reset();
  markAllDirty();
}

void Apple2::reset() {
  sw80store = ramrd = ramwrt = intcxrom = altzp = slotc3rom = false;
  col80 = altchar = mixed = page2 = hires = dhires = false;
  text = true;
  intc8rom = false;
  ioudis = true;
  lcRam = false; lcBank2 = true; lcWrite = true; lcPreWrite = 0;
  motorOn = false; q6 = q7 = false;
  flushDisks();
  remap();
  sp = (uint8_t)(sp - 3);
  p = (p | FI | FU) & ~FD;
  pc = read(0xFFFC) | (read(0xFFFD) << 8);
  markAllDirty();
}

// ---------------------------------------------------------------- memory map
static inline bool isVideoPage(int p) { return (p >= 0x04 && p <= 0x0B) || (p >= 0x20 && p <= 0x5F); }

void Apple2::remap() {
  bool e = mdl != APPLE2PLUS;  // the no-ROM screen behaves like a IIe
  uint8_t* zpBank = (e && altzp) ? aux : ram;
  for (int p = 0; p < 2; p++) { rpage[p] = zpBank + p * 256; wpage[p] = zpBank + p * 256; }
  for (int p = 2; p < 0xC0; p++) {
    bool rAux = e && ramrd, wAux = e && ramwrt;
    if (e && sw80store) {
      if (p >= 0x04 && p <= 0x07) rAux = wAux = page2;
      if (hires && p >= 0x20 && p <= 0x3F) rAux = wAux = page2;
    }
    rpage[p] = (rAux ? aux : ram) + p * 256;
    wpage[p] = (wAux ? aux : ram) + p * 256;
  }
  for (int p = 0xC0; p < 0xD0; p++) { rpage[p] = nullptr; wpage[p] = nullptr; }
  for (int p = 0xD0; p < 0x100; p++) {
    uint32_t off = (p < 0xE0 && !lcBank2) ? (uint32_t)(p - 0x10) * 256 : (uint32_t)p * 256;
    rpage[p] = lcRam ? zpBank + off : rom + (p - 0xC0) * 256;
    wpage[p] = lcWrite ? zpBank + off : nullptr;
  }
  for (int p = 0; p < 256; p++) {
    wdirty[p] = -1;
    if (wpage[p] && isVideoPage(p)) wdirty[p] = (int16_t)((wpage[p] >= aux && wpage[p] < aux + 65536 ? 256 : 0) + p);
  }
}

uint8_t Apple2::read(uint16_t ad) {
  const uint8_t* r = rpage[ad >> 8];
  if (r) return r[ad & 255];
  return readSlow(ad);
}

void Apple2::write(uint16_t ad, uint8_t v) {
  uint8_t pg = ad >> 8;
  uint8_t* w = wpage[pg];
  if (w) {
    w[ad & 255] = v;
    int16_t di = wdirty[pg];
    if (di >= 0) dirty[di >> 5] |= 1u << (di & 31);
    return;
  }
  writeSlow(ad, v);
}

uint8_t Apple2::floatingBus() {
  // Approximate the video scanner's byte (what an empty slot or unused I/O address returns)
  uint32_t fc = (uint32_t)(cycles - frameCycle0);
  int line = fc / 65, hc = fc % 65;
  if (line >= 192) line = line % 192;
  int col = hc < 25 ? 0 : hc - 25;
  uint16_t ad;
  if (!text && hires && !(mixed && line >= 160))
    ad = (page2 && !sw80store ? 0x4000 : 0x2000) + (line & 7) * 0x400 + ((line >> 3) & 7) * 0x80 + (line >> 6) * 0x28 + col;
  else
    ad = (page2 && !sw80store ? 0x800 : 0x400) + ((line >> 3) & 7) * 0x80 + (line >> 6) * 0x28 + col;
  return ram[ad];
}

uint8_t Apple2::readSlow(uint16_t ad) {
  if (ad < 0xC100) return ioRead(ad);
  if (ad >= 0xD000) return 0xFF;  // write-only mapping never happens for reads
  bool e = mdl == APPLE2E;
  if (e) {
    if (ad == 0xCFFF) { uint8_t v = intc8rom || intcxrom ? rom[ad - 0xC000] : floatingBus(); intc8rom = false; return v; }
    if (intcxrom) return rom[ad - 0xC000];
    if (ad < 0xC800) {
      int s = (ad >> 8) & 7;
      if (s == 3 && !slotc3rom) { intc8rom = true; return rom[ad - 0xC000]; }
      if (s == 6) return slot6[ad & 255];
      return floatingBus();
    }
    if (intc8rom) return rom[ad - 0xC000];
    return floatingBus();
  }
  if (ad < 0xC800 && ((ad >> 8) & 7) == 6) return slot6[ad & 255];
  return floatingBus();
}

void Apple2::writeSlow(uint16_t ad, uint8_t v) {
  if (ad < 0xC100) { ioWrite(ad, v); return; }
  if (ad >= 0xD000) return;  // ROM
  if (mdl == APPLE2E) {
    if (ad == 0xCFFF) intc8rom = false;
    else if ((ad >> 8) == 0xC3 && !slotc3rom && !intcxrom) intc8rom = true;
  }
}

// ---------------------------------------------------------------- I/O
uint16_t Apple2::videoMode() const {
  uint16_t m = 0;
  if (text) m |= VM_TEXT;
  if (mixed) m |= VM_MIXED;
  if (page2) m |= VM_PAGE2;
  if (hires) m |= VM_HIRES;
  if (col80) m |= VM_80COL;
  if (dhires) m |= VM_DHIRES;
  if (altchar) m |= VM_ALTCHAR;
  if (sw80store) m |= VM_80STORE;
  if (mdl != APPLE2PLUS) m |= VM_IIE;
  return m;
}

void Apple2::lcAccess(uint16_t ad, bool isWrite) {
  lcBank2 = !(ad & 8);
  lcRam = ((ad & 3) == 0) || ((ad & 3) == 3);
  if (ad & 1) {
    if (!isWrite) {
      if (lcPreWrite) lcWrite = true;
      lcPreWrite = 1;
    } else {
      lcPreWrite = 0;
    }
  } else {
    lcWrite = false;
    lcPreWrite = 0;
  }
  remap();
}

void Apple2::softSwitch(uint16_t ad, bool isWrite) {
  bool e = mdl != APPLE2PLUS;
  bool dirtyMap = false;
  switch (ad) {
    case 0xC050: text = false; break;
    case 0xC051: text = true; break;
    case 0xC052: mixed = false; break;
    case 0xC053: mixed = true; break;
    case 0xC054: page2 = false; dirtyMap = sw80store; break;
    case 0xC055: page2 = true; dirtyMap = sw80store; break;
    case 0xC056: hires = false; dirtyMap = sw80store; break;
    case 0xC057: hires = true; dirtyMap = sw80store; break;
    case 0xC05E: if (e) dhires = true; break;
    case 0xC05F: if (e) dhires = false; break;
  }
  (void)isWrite;
  if (dirtyMap) remap();
}

uint8_t Apple2::ioRead(uint16_t ad) {
  bool e = mdl != APPLE2PLUS;
  uint8_t lo = ad & 0xFF;
  switch (lo >> 4) {
    case 0x0: return kbd;
    case 0x1: {
      if (lo == 0x10) {
        uint8_t v = (kbd & 0x7F) | (e && keysHeld ? 0x80 : 0);
        kbd &= 0x7F;
        return v;
      }
      if (!e) return floatingBus();
      bool f = false;
      switch (lo) {
        case 0x11: f = lcBank2; break;
        case 0x12: f = lcRam; break;
        case 0x13: f = ramrd; break;
        case 0x14: f = ramwrt; break;
        case 0x15: f = intcxrom; break;
        case 0x16: f = altzp; break;
        case 0x17: f = slotc3rom; break;
        case 0x18: f = sw80store; break;
        case 0x19: f = (cycles - frameCycle0) < 192 * 65; break;  // not in vertical blank
        case 0x1A: f = text; break;
        case 0x1B: f = mixed; break;
        case 0x1C: f = page2; break;
        case 0x1D: f = hires; break;
        case 0x1E: f = altchar; break;
        case 0x1F: f = col80; break;
      }
      return (f ? 0x80 : 0) | (kbd & 0x7F);
    }
    case 0x2: return floatingBus();
    case 0x3:
      spk = !spk;
      if (spkN < SPK_MAX) spkLog[spkN++] = (uint16_t)(cycles - frameCycle0 + 3);
      return floatingBus();
    case 0x4: return floatingBus();
    case 0x5: softSwitch(ad, false); return floatingBus();
    case 0x6: {
      int r = lo & 7;
      uint8_t fb = floatingBus() & 0x7F;
      if (r >= 1 && r <= 3) return (buttons[r - 1] ? 0x80 : 0) | fb;
      if (r >= 4) {
        uint64_t dt = cycles - paddleTrigger;
        return (dt < (uint64_t)paddle[r - 4] * 11 + 8 ? 0x80 : 0) | fb;
      }
      return fb;
    }
    case 0x7:
      if (lo == 0x70) paddleTrigger = cycles;
      if (e && lo == 0x7E) return (ioudis ? 0x80 : 0) | (kbd & 0x7F);
      if (e && lo == 0x7F) return (dhires ? 0x80 : 0) | (kbd & 0x7F);
      return floatingBus();
    case 0x8: lcAccess(ad, false); return floatingBus();
    case 0xE: return diskIO(lo & 15, false, 0);  // slot 6
    default: return floatingBus();
  }
}

void Apple2::ioWrite(uint16_t ad, uint8_t v) {
  bool e = mdl != APPLE2PLUS;
  uint8_t lo = ad & 0xFF;
  switch (lo >> 4) {
    case 0x0:
      if (!e) return;
      switch (lo) {
        case 0x00: sw80store = false; break;
        case 0x01: sw80store = true; break;
        case 0x02: ramrd = false; break;
        case 0x03: ramrd = true; break;
        case 0x04: ramwrt = false; break;
        case 0x05: ramwrt = true; break;
        case 0x06: intcxrom = false; break;
        case 0x07: intcxrom = true; break;
        case 0x08: altzp = false; break;
        case 0x09: altzp = true; break;
        case 0x0A: slotc3rom = false; break;
        case 0x0B: slotc3rom = true; break;
        case 0x0C: col80 = false; break;
        case 0x0D: col80 = true; break;
        case 0x0E: altchar = false; break;
        case 0x0F: altchar = true; break;
      }
      remap();
      return;
    case 0x1: kbd &= 0x7F; return;
    case 0x3:
      spk = !spk;
      if (spkN < SPK_MAX) spkLog[spkN++] = (uint16_t)(cycles - frameCycle0 + 3);
      return;
    case 0x5: softSwitch(ad, true); return;
    case 0x7:
      if (lo == 0x70) paddleTrigger = cycles;
      if (e && lo == 0x7E) ioudis = true;
      if (e && lo == 0x7F) ioudis = false;
      return;
    case 0x8: lcAccess(ad, true); return;
    case 0xE: diskIO(lo & 15, true, v); return;
    default: return;
  }
}

// ---------------------------------------------------------------- keyboard
void Apple2::keyDown(uint8_t c) {
  c &= 0x7F;
  if (capsLock && c >= 'a' && c <= 'z') c -= 32;
  if (mdl == APPLE2PLUS) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c == 0x7F) c = 0x08;
  }
  kbd = c | 0x80;
  keysHeld++;
}

void Apple2::keyUp() {
  if (keysHeld > 0) keysHeld--;
}

bool Apple2::typeText(const char* s) {
  for (; *s; s++) {
    char ch = *s;
    if (ch == '\r') continue;
    if (ch == '\n') ch = '\r';
    if ((uint8_t)ch >= 0x80) continue;
    int next = (tqHead + 1) % (int)sizeof tq;
    if (next == tqTail) return false;
    tq[tqHead] = ch;
    tqHead = next;
  }
  return true;
}

void Apple2::pumpTypeBuffer() {
  if (tqHead == tqTail || (kbd & 0x80)) return;
  uint8_t c = (uint8_t)tq[tqTail];
  tqTail = (tqTail + 1) % (int)sizeof tq;
  if (capsLock && c >= 'a' && c <= 'z') c -= 32;
  if (mdl == APPLE2PLUS && c >= 'a' && c <= 'z') c -= 32;
  kbd = c | 0x80;
}

// ---------------------------------------------------------------- sound
int Apple2::readAudio(int16_t* out, int max) {
  int n = 0;
  while (n < max && aTail != aHead) {
    out[n++] = aRing[aTail];
    aTail = (aTail + 1) % AUDIO_RING;
  }
  return n;
}

void Apple2::finishAudio() {
  if (!audioEnabled) {
    spkN = 0;
    spkStartLevel = spk;
    return;
  }
  const double cps = A2_CLOCK / A2_AUDIO_RATE;  // cycles per sample (~46.3)
  double t = sampleCarry;
  int idx = 0;
  bool level = spkStartLevel;
  while (t + cps <= A2_CYCLES_PER_FRAME) {
    double t1 = t + cps;
    // time-weighted average of the speaker level over [t, t1)
    double hi = 0, cur = t;
    while (idx < spkN && spkLog[idx] < t1) {
      double at = spkLog[idx] < cur ? cur : spkLog[idx];
      if (level) hi += at - cur;
      cur = at;
      level = !level;
      idx++;
    }
    if (level) hi += t1 - cur;
    float x = (float)(hi / cps) * 2.0f - 1.0f;
    dcLevel += (x - dcLevel) * 0.0015f;
    float yv = (x - dcLevel) * 9000.0f;
    if (yv > 32000) yv = 32000;
    if (yv < -32000) yv = -32000;
    int next = (aHead + 1) % AUDIO_RING;
    if (next != aTail) { aRing[aHead] = (int16_t)yv; aHead = next; }
    t = t1;
  }
  while (idx < spkN) { level = !level; idx++; }
  sampleCarry = t - A2_CYCLES_PER_FRAME;
  spkStartLevel = level;
  spkN = 0;
}

// ---------------------------------------------------------------- Disk II
void Apple2::insertDisk(int dn, DiskImage* img) {
  Drive& d = drives[dn & 1];
  diskFlush(d);
  d.img = img;
  d.loadedTrack = -1;
  d.dirty = false;
  d.pos = 0;
}

void Apple2::flushDisks() {
  diskFlush(drives[0]);
  diskFlush(drives[1]);
}

static void nibblizeTrack(const uint8_t* data, int track, bool po, uint8_t* out) {
  int o = 0;
  auto put = [&](uint8_t b) { if (o < A2_TRACK_NIBBLES) out[o++] = b; };
  auto put44 = [&](uint8_t v) { put((v >> 1) | 0xAA); put(v | 0xAA); };
  const uint8_t vol = 254;
  for (int i = 0; i < 48; i++) put(0xFF);
  for (int s = 0; s < 16; s++) {
    // address field
    put(0xD5); put(0xAA); put(0x96);
    put44(vol); put44((uint8_t)track); put44((uint8_t)s); put44(vol ^ (uint8_t)track ^ (uint8_t)s);
    put(0xDE); put(0xAA); put(0xEB);
    for (int i = 0; i < 6; i++) put(0xFF);
    // data field
    const uint8_t* src = data + (po ? PRODOS_ORDER[s] : DOS_ORDER[s]) * 256;
    uint8_t buf[343];
    for (int j = 0; j < 86; j++) {
      uint8_t v = 0;
      for (int k = 0; k < 3; k++) {
        int i = j + 86 * k;
        if (i < 256) v |= (((src[i] & 1) << 1) | ((src[i] >> 1) & 1)) << (2 * k);
      }
      buf[j] = v;
    }
    for (int i = 0; i < 256; i++) buf[86 + i] = src[i] >> 2;
    put(0xD5); put(0xAA); put(0xAD);
    uint8_t prev = 0;
    for (int i = 0; i < 342; i++) { put(NIB62[buf[i] ^ prev]); prev = buf[i]; }
    put(NIB62[prev]);
    put(0xDE); put(0xAA); put(0xEB);
    for (int i = 0; i < 20; i++) put(0xFF);
  }
  while (o < A2_TRACK_NIBBLES) out[o++] = 0xFF;
}

// Decode a (possibly rewritten) nibble track back into sectors. Returns a bitmask of sectors found.
static uint16_t denibblizeTrack(const uint8_t* nib, int track, bool po, uint8_t* data) {
  const int N = A2_TRACK_NIBBLES;
  uint16_t found = 0;
  auto at = [&](int i) { return nib[((i % N) + N) % N]; };
  for (int i = 0; i < N; i++) {
    if (at(i) != 0xD5 || at(i + 1) != 0xAA || at(i + 2) != 0x96) continue;
    auto d44 = [&](int k) { return (uint8_t)(((at(k) << 1) | 1) & at(k + 1)); };
    uint8_t t = d44(i + 5), s = d44(i + 7);
    if (t != track || s > 15) continue;
    // find the data field within the next ~60 bytes
    int j = i + 11, end = i + 80;
    for (; j < end; j++)
      if (at(j) == 0xD5 && at(j + 1) == 0xAA && at(j + 2) == 0xAD) break;
    if (j >= end) continue;
    j += 3;
    uint8_t buf[342];
    uint8_t prev = 0;
    bool ok = true;
    for (int k = 0; k < 342; k++) {
      int8_t v = DENIB62[at(j + k)];
      if (v < 0) { ok = false; break; }
      prev ^= (uint8_t)v;
      buf[k] = prev;
    }
    if (!ok) continue;
    int8_t ck = DENIB62[at(j + 342)];
    if (ck < 0 || (uint8_t)ck != prev) continue;
    uint8_t* dst = data + (po ? PRODOS_ORDER[s] : DOS_ORDER[s]) * 256;
    for (int k = 0; k < 256; k++) {
      uint8_t two = buf[k % 86] >> (2 * (k / 86));
      dst[k] = (uint8_t)((buf[86 + k] << 2) | ((two & 1) << 1) | ((two >> 1) & 1));
    }
    found |= 1 << s;
  }
  return found;
}

void Apple2::diskFlush(Drive& d) {
  if (!d.dirty || !d.img || d.loadedTrack < 0) { d.dirty = false; return; }
  d.dirty = false;
  if (!d.img->readTrack(d.loadedTrack, trackTmp)) return;
  if (denibblizeTrack(d.nib, d.loadedTrack, d.img->prodosOrder, trackTmp))
    d.img->writeTrack(d.loadedTrack, trackTmp);
}

void Apple2::diskLoadTrack(Drive& d) {
  int t = d.halfTrack >> 1;
  if (t > 34) t = 34;
  if (d.loadedTrack == t) return;
  diskFlush(d);
  if (!d.img->readTrack(t, trackTmp)) memset(trackTmp, 0, 4096);
  nibblizeTrack(trackTmp, t, d.img->prodosOrder, d.nib);
  d.loadedTrack = t;
  if (d.pos >= A2_TRACK_NIBBLES) d.pos = 0;
}

uint8_t Apple2::diskIO(int reg, bool isWrite, uint8_t v) {
  Drive& d0 = drives[curDrive];
  switch (reg) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: {
      int ph = reg >> 1;
      if (reg & 1) {
        d0.phases |= 1 << ph;
        int delta = (ph - (d0.halfTrack & 3)) & 3;
        if (delta == 1 && d0.halfTrack < 69) d0.halfTrack++;
        else if (delta == 3 && d0.halfTrack > 0) d0.halfTrack--;
      } else {
        d0.phases &= ~(1 << ph);
      }
      break;
    }
    case 8: if (motorOn) { motorOn = false; motorStopAt = cycles + 1000000; } break;  // spins down ~1 s later
    case 9: motorOn = true; break;
    case 10: curDrive = 0; break;
    case 11: curDrive = 1; break;
    case 12: q6 = false; break;
    case 13: q6 = true; break;
    case 14: q7 = false; break;
    case 15: q7 = true; break;
  }
  Drive& d = drives[curDrive];
  if (isWrite) {
    if ((reg & 1) && q7 && (motorOn || cycles < motorStopAt) && d.img && !d.img->writeProtected) {
      diskLoadTrack(d);
      d.nib[d.pos] = v;
      d.pos = (d.pos + 1) % A2_TRACK_NIBBLES;
      d.dirty = true;
    }
    return 0;
  }
  if (reg & 1) return floatingBus();
  if (q7) return latch;
  if (q6) return (d.img == nullptr || d.img->writeProtected) ? 0x80 : 0x00;  // write-protect sense
  if (!motorOn && cycles >= motorStopAt) return latch;
  if (!d.img) { latch = 0xFF; return latch; }
  diskLoadTrack(d);
  // The disk turns at one nibble per 32 CPU cycles. A read soon after the last nibble sees the
  // latch without its high bit (the next byte is still shifting in), as on the real controller.
  uint64_t el = cycles - d.lastRead;
  if (el < 32) return latch & 0x7F;
  int adv = el > 32 * A2_TRACK_NIBBLES ? 1 : (int)(el / 32);
  d.pos = (d.pos + adv) % A2_TRACK_NIBBLES;
  d.lastRead = cycles;
  latch = d.nib[d.pos];
  return latch;
}

void Apple2::diskTrap(int n) {
  Drive& d = drives[curDrive];
  if (n == 1) {  // recalibrate + build the PROM's nibble table at $356 (some loaders reuse it)
    d.halfTrack = 0;
    d.phases = 0;
    for (int b = 0x96; b < 0x100; b++)
      if (DENIB62[b] >= 0) write(0x0356 + (b - 0x80), (uint8_t)DENIB62[b]);
    cycles += 2000;
    return;
  }
  if (n == 2) {  // read physical sector $3D of the current track into ($26)
    if (!d.img) {  // no disk: go to BASIC instead of spinning forever
      motorOn = false;
      pc = 0xE000;
      return;
    }
    diskFlush(d);
    int t = d.halfTrack >> 1;
    if (!d.img->readTrack(t > 34 ? 34 : t, trackTmp)) memset(trackTmp, 0, 4096);
    uint8_t s = read(0x3D) & 15;
    uint16_t dst = read(0x26) | (read(0x27) << 8);
    const uint8_t* src = trackTmp + (d.img->prodosOrder ? PRODOS_ORDER[s] : DOS_ORDER[s]) * 256;
    for (int i = 0; i < 256; i++) write((uint16_t)(dst + i), src[i]);
    write(0x40, (uint8_t)t);
    cycles += 3000;
  }
}

// ---------------------------------------------------------------- frame
void Apple2::runFrame() {
  frameCycle0 = cycles;
  pumpTypeBuffer();
  uint64_t end = cycles + A2_CYCLES_PER_FRAME;
  while (cycles < end) cycles += step();
  finishAudio();
  if (!motorOn && motorStopAt && cycles >= motorStopAt) {  // drive stopped: write changed tracks back
    motorStopAt = 0;
    flushDisks();
  }
  frameCount++;
}

// ---------------------------------------------------------------- CPU
void Apple2::cpuTestSetup(const uint8_t* img, uint16_t startPc, bool nm) {
  // plain 64K RAM for CPU test programs: everything writable, no I/O
  memcpy(ram, img, 65536);
  for (int p = 0; p < 256; p++) { rpage[p] = ram + p * 256; wpage[p] = ram + p * 256; wdirty[p] = -1; }
  nmos = nm;
  pc = startPc; sp = 0xFF; p = FU | FI;
}

void Apple2::doIrq(uint16_t vec, bool brk) {
  push(pc >> 8);
  push(pc & 255);
  push((p | FU | (brk ? FB : 0)) & (brk ? 0xFF : ~FB));
  p |= FI;
  if (!nmos) p &= ~FD;
  pc = read(vec) | (read(vec + 1) << 8);
}

// NMOS opcodes that are documented (everything else runs the "illegal opcode" path in II+ mode)
static bool nmosDocumented(uint8_t op) {
  static const char* doc =
      // 0123456789ABCDEF
      "XX...XX.XXX..XX."  // 0
      "XX...XX.XX...XX."  // 1
      "XX..XXX.XXX.XXX."  // 2
      "XX...XX.XX...XX."  // 3
      "XX...XX.XXX.XXX."  // 4
      "XX...XX.XX...XX."  // 5
      "XX...XX.XXX.XXX."  // 6
      "XX...XX.XX...XX."  // 7
      ".X..XXX.X.X.XXX."  // 8
      "XX..XXX.XXX..X.."  // 9
      "XXX.XXX.XXX.XXX."  // A
      "XX..XXX.XXX.XXX."  // B
      "XX..XXX.XXX.XXX."  // C
      "XX...XX.XX...XX."  // D
      "XX..XXX.XXX.XXX."  // E
      "XX...XX.XX...XX."; // F
  return doc[op] == 'X';
}

int Apple2::step() {
  uint8_t op = read(pc++);
  int extra = 0;

  // ---- addressing helpers
  auto imm = [&]() -> uint16_t { return pc++; };
  auto zp = [&]() -> uint16_t { return read(pc++); };
  auto zpx = [&]() -> uint16_t { return (uint8_t)(read(pc++) + x); };
  auto zpy = [&]() -> uint16_t { return (uint8_t)(read(pc++) + y); };
  auto abs_ = [&]() -> uint16_t { uint16_t v = read(pc) | (read(pc + 1) << 8); pc += 2; return v; };
  auto absi = [&](uint8_t r, bool pen) -> uint16_t {
    uint16_t b = read(pc) | (read(pc + 1) << 8); pc += 2;
    uint16_t e = b + r;
    if (pen && ((b ^ e) & 0xFF00)) extra++;
    return e;
  };
  auto ind16zp = [&](uint8_t z) -> uint16_t { return read(z) | (read((uint8_t)(z + 1)) << 8); };
  auto indx = [&]() -> uint16_t { return ind16zp((uint8_t)(read(pc++) + x)); };
  auto indy = [&](bool pen) -> uint16_t {
    uint16_t b = ind16zp(read(pc++));
    uint16_t e = b + y;
    if (pen && ((b ^ e) & 0xFF00)) extra++;
    return e;
  };
  auto indz = [&]() -> uint16_t { return ind16zp(read(pc++)); };

  // ---- ALU helpers
  auto nz = [&](uint8_t v) { p = (p & ~(FN | FZ)) | (v & FN) | (v ? 0 : FZ); };
  auto adc = [&](uint8_t v) {
    int c = p & FC;
    if (!(p & FD)) {
      int r = a + v + c;
      p &= ~(FC | FV);
      if (r > 0xFF) p |= FC;
      if (~(a ^ v) & (a ^ r) & 0x80) p |= FV;
      a = (uint8_t)r;
      nz(a);
      return;
    }
    int al = (a & 0x0F) + (v & 0x0F) + c;
    if (al >= 0x0A) al = ((al + 0x06) & 0x0F) + 0x10;
    int r = (a & 0xF0) + (v & 0xF0) + al;
    int bin = (a + v + c) & 0xFF;
    p &= ~(FC | FV | FN | FZ);
    if (~(a ^ v) & (a ^ r) & 0x80) p |= FV;
    if (nmos) { if (r & 0x80) p |= FN; if (!bin) p |= FZ; }
    if (r >= 0xA0) r += 0x60;
    if (r >= 0x100) p |= FC;
    a = (uint8_t)r;
    if (!nmos) { nz(a); extra++; }
  };
  auto sbc = [&](uint8_t v) {
    int c = p & FC;
    int bin = a - v - (1 - c);
    if (!(p & FD)) {
      p &= ~(FC | FV);
      if (bin >= 0) p |= FC;
      if ((a ^ v) & (a ^ bin) & 0x80) p |= FV;
      a = (uint8_t)bin;
      nz(a);
      return;
    }
    p &= ~(FC | FV);
    if (bin >= 0) p |= FC;
    if ((a ^ v) & (a ^ bin) & 0x80) p |= FV;
    if (nmos) {
      int al = (a & 0x0F) - (v & 0x0F) - (1 - c);
      if (al < 0) al = ((al - 0x06) & 0x0F) - 0x10;
      int r = (a & 0xF0) - (v & 0xF0) + al;
      if (r < 0) r -= 0x60;
      nz((uint8_t)bin);
      a = (uint8_t)r;
    } else {
      int al = (a & 0x0F) - (v & 0x0F) + c - 1;
      int r = bin;
      if (r < 0) r -= 0x60;
      if (al < 0) r -= 0x06;
      a = (uint8_t)r;
      nz(a);
      extra++;
    }
  };
  auto cmp = [&](uint8_t r, uint8_t v) {
    int d = r - v;
    p = (p & ~(FC | FN | FZ)) | (d >= 0 ? FC : 0) | ((uint8_t)d & FN) | ((uint8_t)d ? 0 : FZ);
  };
  auto bit = [&](uint8_t v) { p = (p & ~(FN | FV | FZ)) | (v & (FN | FV)) | ((a & v) ? 0 : FZ); };
  auto asl = [&](uint8_t v) -> uint8_t { p = (p & ~FC) | (v >> 7); v <<= 1; nz(v); return v; };
  auto lsr = [&](uint8_t v) -> uint8_t { p = (p & ~FC) | (v & 1); v >>= 1; nz(v); return v; };
  auto rol = [&](uint8_t v) -> uint8_t { uint8_t c = p & FC; p = (p & ~FC) | (v >> 7); v = (v << 1) | c; nz(v); return v; };
  auto ror = [&](uint8_t v) -> uint8_t { uint8_t c = p & FC; p = (p & ~FC) | (v & 1); v = (v >> 1) | (c << 7); nz(v); return v; };
  auto branch = [&](bool cond) -> int {
    int8_t off = (int8_t)read(pc++);
    if (!cond) return 2;
    uint16_t t = pc + off;
    int c = ((t ^ pc) & 0xFF00) ? 4 : 3;
    pc = t;
    return c;
  };
#define RMW(addrexpr, fn, cyc) { uint16_t ea_ = addrexpr; write(ea_, fn(read(ea_))); return cyc + extra; }

  // ---- NMOS 6502 (II+): undocumented opcodes
  if (nmos && op != 0x02 && !nmosDocumented(op)) {
    uint8_t lo = op & 0x1F;
    uint8_t grp = op >> 5;  // which combined op for xx3/xx7/xxF/xx13/xx17/xx1B/xx1F
    auto eaFor = [&](uint8_t l, bool yIdx, int& cyc) -> uint16_t {
      switch (l) {
        case 0x03: cyc = 8; return indx();
        case 0x07: cyc = 5; return zp();
        case 0x0F: cyc = 6; return abs_();
        case 0x13: cyc = 8; return indy(false);
        case 0x17: cyc = 6; return yIdx ? zpy() : zpx();
        case 0x1B: cyc = 7; return absi(y, false);
        case 0x1F: cyc = 7; return absi(yIdx ? y : x, false);
      }
      cyc = 2; return 0;
    };
    if (lo == 0x03 || lo == 0x07 || lo == 0x0F || lo == 0x13 || lo == 0x17 || lo == 0x1B || lo == 0x1F) {
      int cyc;
      if (grp == 4) {  // SAX / SHA / SHX family (8x/9x)
        if (op == 0x9B || op == 0x9F || op == 0x93) {  // TAS/SHA: rarely used; approximate
          uint16_t ea = op == 0x93 ? indy(false) : absi(y, false);
          write(ea, a & x & (uint8_t)((ea >> 8) + 1));
          return op == 0x93 ? 6 : 5;
        }
        uint16_t ea = eaFor(lo, true, cyc);
        write(ea, a & x);
        return lo == 0x03 ? 6 : lo == 0x07 ? 3 : lo == 0x0F ? 4 : 4;
      }
      if (grp == 5) {  // LAX family (Ax/Bx)
        if (op == 0xBB) { uint8_t v = read(absi(y, true)) & sp; a = x = sp = v; nz(v); return 4 + extra; }
        uint16_t ea;
        int c;
        switch (lo) {
          case 0x03: ea = indx(); c = 6; break;
          case 0x07: ea = zp(); c = 3; break;
          case 0x0F: ea = abs_(); c = 4; break;
          case 0x13: ea = indy(true); c = 5; break;
          case 0x17: ea = zpy(); c = 4; break;
          default: ea = absi(y, true); c = 4; break;
        }
        a = x = read(ea); nz(a);
        return c + extra;
      }
      uint16_t ea = eaFor(lo, false, cyc);
      uint8_t v = read(ea);
      switch (grp) {
        case 0: v = asl(v); write(ea, v); a |= v; nz(a); break;           // SLO
        case 1: v = rol(v); write(ea, v); a &= v; nz(a); break;           // RLA
        case 2: v = lsr(v); write(ea, v); a ^= v; nz(a); break;           // SRE
        case 3: v = ror(v); write(ea, v); adc(v); break;                  // RRA
        case 6: v--; write(ea, v); cmp(a, v); break;                      // DCP
        case 7: v++; write(ea, v); sbc(v); break;                         // ISC
      }
      return cyc;
    }
    switch (op) {
      case 0x0B: case 0x2B: a &= read(imm()); nz(a); p = (p & ~FC) | (a >> 7); return 2;  // ANC
      case 0x4B: a &= read(imm()); a = lsr(a); return 2;                                   // ALR
      case 0x6B: {                                                                         // ARR
        a &= read(imm());
        a = (a >> 1) | ((p & FC) << 7);
        nz(a);
        p = (p & ~(FC | FV)) | ((a >> 6) & 1) | (((a >> 6) ^ (a >> 5)) & 1 ? FV : 0);
        return 2;
      }
      case 0x8B: a = x & read(imm()); nz(a); return 2;   // XAA (unstable)
      case 0xAB: a = x = read(imm()); nz(a); return 2;   // LAX #
      case 0xCB: { uint8_t v = read(imm()); int d = (a & x) - v; x = (uint8_t)d; p = (p & ~FC) | (d >= 0 ? FC : 0); nz(x); return 2; }  // AXS
      case 0xEB: sbc(read(imm())); return 2;
      case 0x9C: { uint16_t ea = absi(x, false); write(ea, y & (uint8_t)((ea >> 8) + 1)); return 5; }  // SHY
      case 0x9E: { uint16_t ea = absi(y, false); write(ea, x & (uint8_t)((ea >> 8) + 1)); return 5; }  // SHX
      case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA: return 2;
      case 0x80: case 0x82: case 0x89: case 0xC2: case 0xE2: pc++; return 2;
      case 0x04: case 0x44: case 0x64: pc++; return 3;
      case 0x14: case 0x34: case 0x54: case 0x74: case 0xD4: case 0xF4: pc++; return 4;
      case 0x0C: pc += 2; return 4;
      case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC: absi(x, true); return 4 + extra;
      default: return 2;  // KIL/JAM: treat as a 1-byte NOP rather than freezing the machine
    }
  }

  switch (op) {
    // ---- loads / stores
    case 0xA9: a = read(imm()); nz(a); return 2;
    case 0xA5: a = read(zp()); nz(a); return 3;
    case 0xB5: a = read(zpx()); nz(a); return 4;
    case 0xAD: a = read(abs_()); nz(a); return 4;
    case 0xBD: a = read(absi(x, true)); nz(a); return 4 + extra;
    case 0xB9: a = read(absi(y, true)); nz(a); return 4 + extra;
    case 0xA1: a = read(indx()); nz(a); return 6;
    case 0xB1: a = read(indy(true)); nz(a); return 5 + extra;
    case 0xB2: a = read(indz()); nz(a); return 5;
    case 0xA2: x = read(imm()); nz(x); return 2;
    case 0xA6: x = read(zp()); nz(x); return 3;
    case 0xB6: x = read(zpy()); nz(x); return 4;
    case 0xAE: x = read(abs_()); nz(x); return 4;
    case 0xBE: x = read(absi(y, true)); nz(x); return 4 + extra;
    case 0xA0: y = read(imm()); nz(y); return 2;
    case 0xA4: y = read(zp()); nz(y); return 3;
    case 0xB4: y = read(zpx()); nz(y); return 4;
    case 0xAC: y = read(abs_()); nz(y); return 4;
    case 0xBC: y = read(absi(x, true)); nz(y); return 4 + extra;
    case 0x85: write(zp(), a); return 3;
    case 0x95: write(zpx(), a); return 4;
    case 0x8D: write(abs_(), a); return 4;
    case 0x9D: write(absi(x, false), a); return 5;
    case 0x99: write(absi(y, false), a); return 5;
    case 0x81: write(indx(), a); return 6;
    case 0x91: write(indy(false), a); return 6;
    case 0x92: write(indz(), a); return 5;
    case 0x86: write(zp(), x); return 3;
    case 0x96: write(zpy(), x); return 4;
    case 0x8E: write(abs_(), x); return 4;
    case 0x84: write(zp(), y); return 3;
    case 0x94: write(zpx(), y); return 4;
    case 0x8C: write(abs_(), y); return 4;
    case 0x64: write(zp(), 0); return 3;
    case 0x74: write(zpx(), 0); return 4;
    case 0x9C: write(abs_(), 0); return 4;
    case 0x9E: write(absi(x, false), 0); return 5;
    // ---- transfers / stack
    case 0xAA: x = a; nz(x); return 2;
    case 0xA8: y = a; nz(y); return 2;
    case 0x8A: a = x; nz(a); return 2;
    case 0x98: a = y; nz(a); return 2;
    case 0xBA: x = sp; nz(x); return 2;
    case 0x9A: sp = x; return 2;
    case 0x48: push(a); return 3;
    case 0x68: a = pop(); nz(a); return 4;
    case 0x08: push(p | FB | FU); return 3;
    case 0x28: p = pop() | FU; p &= ~FB; return 4;
    case 0xDA: push(x); return 3;
    case 0xFA: x = pop(); nz(x); return 4;
    case 0x5A: push(y); return 3;
    case 0x7A: y = pop(); nz(y); return 4;
    // ---- logic / arithmetic
#define ALU(base, fn)                                              \
    case base + 0x09: fn(read(imm())); return 2;                   \
    case base + 0x05: fn(read(zp())); return 3;                    \
    case base + 0x15: fn(read(zpx())); return 4 + extra;           \
    case base + 0x0D: fn(read(abs_())); return 4 + extra;          \
    case base + 0x1D: fn(read(absi(x, true))); return 4 + extra;   \
    case base + 0x19: fn(read(absi(y, true))); return 4 + extra;   \
    case base + 0x01: fn(read(indx())); return 6 + extra;          \
    case base + 0x11: fn(read(indy(true))); return 5 + extra;      \
    case base + 0x12: fn(read(indz())); return 5 + extra;
    ALU(0x00, [&](uint8_t v) { a |= v; nz(a); })
    ALU(0x20, [&](uint8_t v) { a &= v; nz(a); })
    ALU(0x40, [&](uint8_t v) { a ^= v; nz(a); })
    ALU(0x60, adc)
    ALU(0xE0, sbc)
    ALU(0xC0, [&](uint8_t v) { cmp(a, v); })
#undef ALU
    case 0xE0: cmp(x, read(imm())); return 2;
    case 0xE4: cmp(x, read(zp())); return 3;
    case 0xEC: cmp(x, read(abs_())); return 4;
    case 0xC0: cmp(y, read(imm())); return 2;
    case 0xC4: cmp(y, read(zp())); return 3;
    case 0xCC: cmp(y, read(abs_())); return 4;
    case 0x24: bit(read(zp())); return 3;
    case 0x2C: bit(read(abs_())); return 4;
    case 0x34: bit(read(zpx())); return 4;
    case 0x3C: bit(read(absi(x, true))); return 4 + extra;
    case 0x89: p = (p & ~FZ) | ((a & read(imm())) ? 0 : FZ); return 2;
    case 0x04: { uint16_t e = zp(); uint8_t v = read(e); p = (p & ~FZ) | ((a & v) ? 0 : FZ); write(e, v | a); return 5; }
    case 0x0C: { uint16_t e = abs_(); uint8_t v = read(e); p = (p & ~FZ) | ((a & v) ? 0 : FZ); write(e, v | a); return 6; }
    case 0x14: { uint16_t e = zp(); uint8_t v = read(e); p = (p & ~FZ) | ((a & v) ? 0 : FZ); write(e, v & ~a); return 5; }
    case 0x1C: { uint16_t e = abs_(); uint8_t v = read(e); p = (p & ~FZ) | ((a & v) ? 0 : FZ); write(e, v & ~a); return 6; }
    // ---- shifts / inc / dec
    case 0x0A: a = asl(a); return 2;
    case 0x4A: a = lsr(a); return 2;
    case 0x2A: a = rol(a); return 2;
    case 0x6A: a = ror(a); return 2;
    case 0x06: RMW(zp(), asl, 5)
    case 0x16: RMW(zpx(), asl, 6)
    case 0x0E: RMW(abs_(), asl, 6)
    case 0x1E: RMW(absi(x, !nmos), asl, nmos ? 7 : 6)
    case 0x46: RMW(zp(), lsr, 5)
    case 0x56: RMW(zpx(), lsr, 6)
    case 0x4E: RMW(abs_(), lsr, 6)
    case 0x5E: RMW(absi(x, !nmos), lsr, nmos ? 7 : 6)
    case 0x26: RMW(zp(), rol, 5)
    case 0x36: RMW(zpx(), rol, 6)
    case 0x2E: RMW(abs_(), rol, 6)
    case 0x3E: RMW(absi(x, !nmos), rol, nmos ? 7 : 6)
    case 0x66: RMW(zp(), ror, 5)
    case 0x76: RMW(zpx(), ror, 6)
    case 0x6E: RMW(abs_(), ror, 6)
    case 0x7E: RMW(absi(x, !nmos), ror, nmos ? 7 : 6)
    case 0xE6: { uint16_t e = zp(); uint8_t v = read(e) + 1; write(e, v); nz(v); return 5; }
    case 0xF6: { uint16_t e = zpx(); uint8_t v = read(e) + 1; write(e, v); nz(v); return 6; }
    case 0xEE: { uint16_t e = abs_(); uint8_t v = read(e) + 1; write(e, v); nz(v); return 6; }
    case 0xFE: { uint16_t e = absi(x, false); uint8_t v = read(e) + 1; write(e, v); nz(v); return 7; }
    case 0xC6: { uint16_t e = zp(); uint8_t v = read(e) - 1; write(e, v); nz(v); return 5; }
    case 0xD6: { uint16_t e = zpx(); uint8_t v = read(e) - 1; write(e, v); nz(v); return 6; }
    case 0xCE: { uint16_t e = abs_(); uint8_t v = read(e) - 1; write(e, v); nz(v); return 6; }
    case 0xDE: { uint16_t e = absi(x, false); uint8_t v = read(e) - 1; write(e, v); nz(v); return 7; }
    case 0x1A: a++; nz(a); return 2;
    case 0x3A: a--; nz(a); return 2;
    case 0xE8: x++; nz(x); return 2;
    case 0xCA: x--; nz(x); return 2;
    case 0xC8: y++; nz(y); return 2;
    case 0x88: y--; nz(y); return 2;
    // ---- flags
    case 0x18: p &= ~FC; return 2;
    case 0x38: p |= FC; return 2;
    case 0x58: p &= ~FI; return 2;
    case 0x78: p |= FI; return 2;
    case 0xB8: p &= ~FV; return 2;
    case 0xD8: p &= ~FD; return 2;
    case 0xF8: p |= FD; return 2;
    // ---- branches / jumps
    case 0x10: return branch(!(p & FN));
    case 0x30: return branch(p & FN);
    case 0x50: return branch(!(p & FV));
    case 0x70: return branch(p & FV);
    case 0x90: return branch(!(p & FC));
    case 0xB0: return branch(p & FC);
    case 0xD0: return branch(!(p & FZ));
    case 0xF0: return branch(p & FZ);
    case 0x80: return branch(true) + 0;
    case 0x4C: pc = abs_(); return 3;
    case 0x6C: {
      uint16_t ptr = abs_();
      if (nmos) { pc = read(ptr) | (read((ptr & 0xFF00) | ((ptr + 1) & 0xFF)) << 8); return 5; }
      pc = read(ptr) | (read(ptr + 1) << 8);
      return 6;
    }
    case 0x7C: { uint16_t ptr = abs_() + x; pc = read(ptr) | (read(ptr + 1) << 8); return 6; }
    case 0x20: {
      uint16_t t = read(pc) | (read(pc + 1) << 8);
      pc++;
      push(pc >> 8); push(pc & 255);
      pc = t;
      return 6;
    }
    case 0x60: { uint8_t l = pop(); uint8_t h = pop(); pc = ((h << 8) | l) + 1; return 6; }
    case 0x40: { p = pop() | FU; p &= ~FB; uint8_t l = pop(); uint8_t h = pop(); pc = (h << 8) | l; return 6; }
    case 0x00: pc++; doIrq(0xFFFE, true); return 7;
    case 0xEA: return 2;
    // ---- 65C02 extras: trap, NOPs, WAI/STP, Rockwell bit ops
    case 0x02: { uint8_t n = read(pc++); diskTrap(n); return 2; }
    case 0x22: case 0x42: case 0x62: case 0x82: case 0xC2: case 0xE2: pc++; return 2;
    case 0x44: pc++; return 3;
    case 0x54: case 0xD4: case 0xF4: pc++; return 4;
    case 0x5C: pc += 2; return 8;
    case 0xDC: case 0xFC: pc += 2; return 4;
    case 0xCB: return 3;   // WAI: no interrupt sources here, carry on
    case 0xDB: pc--; return 3;  // STP: stop
    default:
      if ((op & 0x0F) == 0x07) {  // RMBn / SMBn zp
        uint16_t e = zp();
        uint8_t bitm = 1 << ((op >> 4) & 7);
        uint8_t v = read(e);
        write(e, (op & 0x80) ? (v | bitm) : (v & ~bitm));
        return 5;
      }
      if ((op & 0x0F) == 0x0F) {  // BBRn / BBSn zp,rel
        uint8_t v = read(zp());
        int8_t off = (int8_t)read(pc++);
        bool set = (v >> ((op >> 4) & 7)) & 1;
        if (set == ((op & 0x80) != 0)) {
          uint16_t t = pc + off;
          int c = ((t ^ pc) & 0xFF00) ? 7 : 6;
          pc = t;
          return c;
        }
        return 5;
      }
      return 1;  // x3 / xB: 1-byte, 1-cycle NOPs
  }
#undef RMW
}

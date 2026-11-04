// Runs Klaus Dormann's 6502 functional test and 65C02 extended opcode test on the Apple2 CPU core.
#include "../Heltec-AppleII/apple2.h"
#include <cstdio>
#include <vector>
#include <fstream>
static std::vector<uint8_t> load(const char* p) { std::ifstream f(p, std::ios::binary); return {std::istreambuf_iterator<char>(f), {}}; }
static bool run(const char* file, bool nmos, uint16_t success) {
  auto img = load(file);
  if (img.size() != 65536) { printf("missing %s\n", file); return false; }
  static Apple2 m;
  m.cpuTestSetup(img.data(), 0x400, nmos);
  uint16_t last = 0xFFFF;
  for (long i = 0; i < 200000000; i++) {
    uint16_t pc = m.pc;
    m.cpuStep();
    if (m.pc == pc) {
      printf("%s (%s): trapped at $%04X after %ld instr -> %s\n", file, nmos ? "NMOS" : "65C02", pc, i, pc == success ? "PASS" : "FAIL");
      return pc == success;
    }
    last = pc;
  }
  printf("%s: timeout last=%04X\n", file, last);
  return false;
}
int main(int argc, char** argv) {
  const char* dir = argc > 1 ? argv[1] : "../../testdata";
  char a[512], b[512];
  snprintf(a, sizeof a, "%s/6502_functional_test.bin", dir);
  snprintf(b, sizeof b, "%s/65C02_extended_opcodes_test.bin", dir);
  bool ok = run(a, true, 0x3469);
  ok &= run(a, false, 0x3469);
  ok &= run(b, false, 0x24F1);
  return ok ? 0 : 1;
}

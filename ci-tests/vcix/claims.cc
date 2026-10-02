// An extension for the tests: claims one encoding. Built twice: claiming `add`,
// which Spike implements, and (with -DFREE) one in custom-0, which nothing does.
#include "extension.h"

static reg_t run(processor_t*, insn_t, reg_t pc) { return pc + 4; }

struct claims_t : extension_t {
  const char* name() { return "claims"; }
  std::vector<insn_desc_t> get_instructions() {
#ifdef FREE
    return {{0x0000000b, 0x0000007f, run, run}};
#else
    return {{0x00000033, 0xfe00707f, run, run}};
#endif
  }
  std::vector<disasm_insn_t*> get_disasms() { return {}; }
};

REGISTER_EXTENSION(claims, []() { return new claims_t; })

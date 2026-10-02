// The extension `vcixaccel`: every instruction a loaded accelerator model owns
// is handed to that model's functional face.
//
// The model is a library given with --extlib that exports vcix_accel_model().
// vcix_accel.h is the interface between the two. It is a copy of
// include/vcix_accel.h of https://github.com/PSAL-POSTECH/vcix-accelerator and
// is replaced as a whole, never edited here.

#include "insn_macros.h"
#include "extension.h"
#include "mmu.h"
#include "processor.h"
#include "trap.h"
#include "vcix_accel.h"
#include "vcix_accel_extension.h"
#include <dlfcn.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>

namespace {

const uint32_t OPCODE_MASK = 0x7f;
const uint32_t OPCODE_CUSTOM_1 = 0x2b; // R-type on x registers
const uint32_t OPCODE_CUSTOM_2 = 0x5b; // VCIX
const uint32_t RS1_IS_F = 5;           // funct3 of the VCIX forms that read f[rs1]

void refuse(const char* fmt, ...) __attribute__((noreturn, format(printf, 1, 2)));
void refuse(const char* fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  fprintf(stderr, "vcixaccel: ");
  vfprintf(stderr, fmt, args);
  fprintf(stderr, "\n");
  va_end(args);
  exit(1);
}

// The table of the model given with --extlib. Nothing but abi_version is read
// from a table of another version. Of the functions, the ones checked are the
// ones Spike calls; the timing face is the timing simulator's to check.
const vcix_model* load_model()
{
  void* symbol = dlsym(RTLD_DEFAULT, "vcix_accel_model");
  if (!symbol)
    refuse("no accelerator model is loaded; give its library with --extlib=<model.so>");
  Dl_info where;
  const char* lib = dladdr(symbol, &where) && where.dli_fname ? where.dli_fname : "the model";

  const vcix_model* m = reinterpret_cast<const vcix_model* (*)()>(symbol)();
  if (!m)
    refuse("%s: vcix_accel_model() returned no table", lib);
  if (m->abi_version != VCIX_ACCEL_ABI_VERSION)
    refuse("%s has ABI %u, Spike has %u", lib, m->abi_version, VCIX_ACCEL_ABI_VERSION);
  if (!m->name)
    refuse("%s: the table has no name", lib);
  if (!m->create || !m->destroy || !m->execute)
    refuse("%s: %s: create, destroy or execute is NULL", lib, m->name);
  if (m->num_encodings && !m->encodings)
    refuse("%s: %s: the table counts %zu encodings and has none", lib, m->name, m->num_encodings);

  for (size_t i = 0; i < m->num_encodings; i++) {
    const vcix_encoding& e = m->encodings[i];
    if (!e.name)
      refuse("%s: %s: encoding %zu has no name", lib, m->name, i);
    // The interface is for custom-1 and custom-2, and that is all the gem5
    // side decodes: an encoding must fix its opcode to one of them.
    const uint32_t opcode = e.match & OPCODE_MASK;
    if ((e.mask & OPCODE_MASK) != OPCODE_MASK || (opcode != OPCODE_CUSTOM_1 && opcode != OPCODE_CUSTOM_2))
      refuse("%s: %s: encoding %s (match 0x%08x mask 0x%08x) is not in custom-1 or custom-2",
             lib, m->name, e.name, e.match, e.mask);
  }
  return m;
}

const vcix_model* model()
{
  static const vcix_model* m = load_model();
  return m;
}

processor_t* proc(void* ctx) { return static_cast<processor_t*>(ctx); }

// A register a model asks for that does not exist is the model's bug; there
// is no trap for it, so the run ends.
void check_index(const char* what, uint32_t index, uint32_t count)
{
  if (index >= count)
    refuse("%s: asked for %s %u of %u", model()->name, what, index, count);
}

const vcix_host host_template = {
  nullptr,
  [](void* c) -> uint32_t { return proc(c)->VU.get_vu_num(); },
  [](void* c) -> uint32_t { return proc(c)->VU.get_vlen(); },
  [](void* c, uint32_t lane, uint32_t reg, int will_write) -> void* {
    check_index("lane", lane, proc(c)->VU.get_vu_num());
    check_index("vector register", reg, NVPR);
    if (will_write)
      proc(c)->get_state()->sstatus->dirty(SSTATUS_VS);
    return &proc(c)->VU.elt<uint8_t>(reg, 0, lane, will_write);
  },
  [](void* c, uint32_t reg) -> uint64_t {
    check_index("x register", reg, NXPR);
    return proc(c)->get_state()->XPR[reg];
  },
  [](void* c, uint32_t reg, uint64_t value) {
    check_index("x register", reg, NXPR);
    proc(c)->get_state()->XPR.write(reg, value);
  },
  [](void* c, uint32_t reg) -> uint64_t {
    check_index("f register", reg, NFPR);
    return proc(c)->get_state()->FPR[reg].v[0];
  },
  // As the hart holds it, with no check of the privilege mode: a model is part
  // of the machine, not of the program.
  [](void* c, uint32_t csr) -> uint64_t {
    auto& csrs = proc(c)->get_state()->csrmap;
    auto found = csrs.find(csr);
    if (found == csrs.end())
      refuse("%s: asked for CSR 0x%x, which this hart does not have", model()->name, csr);
    return found->second->read();
  },
  [](void* c, uint64_t addr, void* dst, size_t bytes) {
    for (size_t i = 0; i < bytes; i++)
      static_cast<uint8_t*>(dst)[i] = proc(c)->get_mmu()->load_uint8(addr + i);
  },
  [](void* c, uint64_t addr, const void* src, size_t bytes) {
    for (size_t i = 0; i < bytes; i++)
      proc(c)->get_mmu()->store_uint8(addr + i, static_cast<const uint8_t*>(src)[i]);
  },
};

reg_t dispatch(processor_t* p, insn_t insn, reg_t pc);

// One per hart, and so is the instance of the model it holds.
class vcix_accel_extension_t : public extension_t
{
 public:
  vcix_accel_extension_t() : self(nullptr) {}
  ~vcix_accel_extension_t() { if (self) model()->destroy(self); }

  const char* name() { return "vcixaccel"; }

  void reset()
  {
    void* instance = get_instance();
    if (model()->reset)
      model()->reset(instance);
  }

  std::vector<insn_desc_t> get_instructions()
  {
    std::vector<insn_desc_t> insns;
    for (size_t i = 0; i < model()->num_encodings; i++) {
      const vcix_encoding& e = model()->encodings[i];
      insns.push_back((insn_desc_t){e.match, e.mask, dispatch, dispatch});
    }
    return insns;
  }

  std::vector<disasm_insn_t*> get_disasms()
  {
    std::vector<disasm_insn_t*> insns;
    for (size_t i = 0; i < model()->num_encodings; i++) {
      const vcix_encoding& e = model()->encodings[i];
      insns.push_back(new disasm_insn_t(e.name, e.match, e.mask, {}));
    }
    return insns;
  }

  // The instance is made on first use, not in the constructor: it is made
  // from the machine description, which the hart holds, and the extension is
  // given its hart only after it is constructed.
  void* get_instance()
  {
    if (self)
      return self;
    vcix_config config = {
      const_cast<std::map<std::string, std::string>*>(&p->machine_config),
      [](void* ctx, const char* key) -> const char* {
        auto& values = *static_cast<const std::map<std::string, std::string>*>(ctx);
        auto found = values.find(key);
        return found == values.end() ? nullptr : found->second.c_str();
      }
    };
    char error[256] = "";
    self = model()->create(&config, error, sizeof error);
    if (!self)
      refuse("%s: %s", model()->name, error);
    return self;
  }

 private:
  void* self;
};

reg_t dispatch(processor_t* p, insn_t insn, reg_t pc)
{
  const uint32_t bits = insn.bits();
  if (!vcix_owner(model(), bits))
    throw trap_illegal_instruction(bits);

  // A VCIX instruction is a vector instruction: what Spike's own require of
  // the hart, it requires too, and one that reads f[rs1] requires FS as well.
  if ((bits & OPCODE_MASK) == OPCODE_CUSTOM_2) {
    require_vector(false);
    if (insn.rm() == RS1_IS_F)
      require_fp;
  }

  vcix_host host = host_template;
  host.ctx = p;
  // vlmul is the low three bits of vtype, signed.
  int vlmul = p->VU.vtype->read() & 0x7;
  vcix_insn decoded = {bits, (uint32_t)p->VU.vl->read(), (uint32_t)p->VU.vsew, vlmul >= 4 ? vlmul - 8 : vlmul};

  auto extension = static_cast<vcix_accel_extension_t*>(p->get_extension("vcixaccel"));
  model()->execute(extension->get_instance(), &host, &decoded);

  // A model runs the instruction to the end, so there is nothing to resume.
  p->VU.vstart->write(0);
  return pc + 4;
}

} // namespace

extension_t* vcix_accel_extension()
{
  return new vcix_accel_extension_t;
}

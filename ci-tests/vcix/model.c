/* A model for the tests: owns all of custom-2, says what it is configured with and what
 * it executes. Built several ways, each with one thing wrong; see run.sh. It leaves the
 * timing face NULL, which Spike does not call. */
#include <stdio.h>
#include <stdlib.h>

#include "vcix_accel.h"

#ifndef OPCODE
#define OPCODE 0x5b
#endif
#ifndef ABI
#define ABI VCIX_ACCEL_ABI_VERSION
#endif

static const vcix_encoding encodings[] = {{OPCODE, 0x7f, "everything"}};

static void *create(const vcix_config *config, char *error, size_t error_size) {
  static const char *const keys[] = {"vpu_num_lanes", "vpu_spad_base_vaddr", "run_base_path", "not_in_the_file"};
  for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
    const char *value = config->get(config->ctx, keys[i]);
    if (value) printf("[model] %s = <%s>\n", keys[i], value);
    else printf("[model] %s is absent\n", keys[i]);
  }
  fflush(stdout);
  (void)error; (void)error_size;
  return malloc(1);
}

static void destroy(void *self) { free(self); }

static void execute(void *self, const vcix_host *host, const vcix_insn *insn) {
  (void)self;
  printf("[model] execute %08x lanes %u vlen %u\n", insn->bits, host->lanes(host->ctx), host->vlen_bits(host->ctx));
  fflush(stdout);
#ifdef BAD_LANE
  host->vreg(host->ctx, host->lanes(host->ctx), 0, 0);
#endif
#ifdef BAD_REGISTER
  host->xreg_read(host->ctx, 32);
#endif
#ifdef READ_CSR
  printf("[model] frm %llu vtype %llx\n", (unsigned long long)host->csr_read(host->ctx, 0x002),
         (unsigned long long)host->csr_read(host->ctx, 0xc21));
  fflush(stdout);
#endif
#ifdef BAD_CSR
  host->csr_read(host->ctx, 0x800);
#endif
}

static const vcix_model table = {
    .abi_version = ABI,
    .name = "fork_test",
    .encodings = encodings,
    .num_encodings = 1,
    .create = create,
    .destroy = destroy,
#ifndef NO_EXECUTE
    .execute = execute,
#endif
};

__attribute__((visibility("default"))) const vcix_model *vcix_accel_model(void) {
#ifdef NO_TABLE
  return NULL;
#else
  return &table;
#endif
}

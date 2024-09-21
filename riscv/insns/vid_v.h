// vmpopc rd, vs2, vm
require(P.VU.vsew >= e8 && P.VU.vsew <= e64);
require_vector(true);
reg_t vl = P.VU.vl->read();
reg_t sew = P.VU.vsew;
reg_t rd_num = insn.rd();
reg_t rs1_num = insn.rs1();
reg_t rs2_num = insn.rs2();
require_align(rd_num, P.VU.vflmul);
require_vm;

const reg_t n_vu = P.VU.get_vu_num();
for (reg_t i = P.VU.vstart->read() ; i < P.VU.vl->read(); ++i) {
  for (reg_t vu_idx=0; vu_idx<n_vu; vu_idx++) {
    VI_LOOP_ELEMENT_SKIP();

    switch (sew) {
    case e8:
      P.VU.elt<uint8_t>(rd_num, i, vu_idx, true) = i;
      break;
    case e16:
      P.VU.elt<uint16_t>(rd_num, i, vu_idx, true) = i;
      break;
    case e32:
      P.VU.elt<uint32_t>(rd_num, i, vu_idx, true) = i;
      break;
    default:
      P.VU.elt<uint64_t>(rd_num, i, vu_idx, true) = i;
      break;
    }
  }
}

P.VU.vstart->write(0);

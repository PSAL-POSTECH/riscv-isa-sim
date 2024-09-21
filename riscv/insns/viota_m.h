// vmpopc rd, vs2, vm
require(P.VU.vsew >= e8 && P.VU.vsew <= e64);
require_vector(true);
reg_t vl = P.VU.vl->read();
reg_t sew = P.VU.vsew;
reg_t rd_num = insn.rd();
reg_t rs1_num = insn.rs1();
reg_t rs2_num = insn.rs2();
require(P.VU.vstart->read() == 0);
require_vm;
require_align(rd_num, P.VU.vflmul);
require_noover(rd_num, P.VU.vflmul, rs2_num, 1);

const reg_t n_vu = P.VU.get_vu_num();
int cnt = 0;
for (reg_t i = 0; i < vl; ++i) {
  for (reg_t vu_idx=0; vu_idx<n_vu; vu_idx++) {
    const int midx = i / 64;
    const int mpos = i % 64;

    bool vs2_lsb = ((P.VU.elt<uint64_t>(rs2_num, midx, vu_idx) >> mpos) & 0x1) == 1;
    bool do_mask = (P.VU.elt<uint64_t>(0, midx, vu_idx) >> mpos) & 0x1;

    bool has_one = false;
    if (insn.v_vm() == 1 || (insn.v_vm() == 0 && do_mask)) {
      if (vs2_lsb) {
        has_one = true;
      }
    }

    bool use_ori = (insn.v_vm() == 0) && !do_mask;
    switch (sew) {
    case e8:
      P.VU.elt<uint8_t>(rd_num, i, vu_idx, true) = use_ori ?
                                    P.VU.elt<uint8_t>(rd_num, i, vu_idx) : cnt;
      break;
    case e16:
      P.VU.elt<uint16_t>(rd_num, i, vu_idx, true) = use_ori ?
                                      P.VU.elt<uint16_t>(rd_num, i, vu_idx) : cnt;
      break;
    case e32:
      P.VU.elt<uint32_t>(rd_num, i, vu_idx, true) = use_ori ?
                                      P.VU.elt<uint32_t>(rd_num, i, vu_idx) : cnt;
      break;
    default:
      P.VU.elt<uint64_t>(rd_num, i, vu_idx, true) = use_ori ?
                                      P.VU.elt<uint64_t>(rd_num, i, vu_idx) : cnt;
      break;
    }

    if (has_one) {
      cnt++;
    }
  }
}


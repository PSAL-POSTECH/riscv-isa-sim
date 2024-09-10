// vmv1r.v vd, vs2
const uint32_t n_vu = P.VU.get_vu_num();
for (int vu_idx=0; vu_idx<static_cast<int>(n_vu); vu_idx++) {
  require_vector_novtype(true, true);
  const reg_t baseAddr = RS1;
  const reg_t vd = insn.rd();
  const reg_t vs2 = insn.rs2();
  const reg_t len = insn.rs1() + 1;
  require_align(vd, len);
  require_align(vs2, len);
  const reg_t size = len * P.VU.vlenb;

  //register needs one-by-one copy to keep commitlog correct
  if (vd != vs2 && P.VU.vstart->read() < size) {
    reg_t i = P.VU.vstart->read() / P.VU.vlenb;
    reg_t off = P.VU.vstart->read() % P.VU.vlenb;
    if (off) {
      memcpy(&P.VU.elt<uint8_t>(vd + i, off, true, vu_idx),
            &P.VU.elt<uint8_t>(vs2 + i, off, false, vu_idx), P.VU.vlenb - off);
      i++;
    }

    for (; i < len; ++i) {
      memcpy(&P.VU.elt<uint8_t>(vd + i, 0, true, vu_idx),
            &P.VU.elt<uint8_t>(vs2 + i, 0, false, vu_idx), P.VU.vlenb);
    }
  }

  P.VU.vstart->write(0);
}
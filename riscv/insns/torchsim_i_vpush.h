reg_t vd = insn.rs2();
const reg_t vl = P.VU.vl->read();
const uint32_t n_vu = P.VU.get_vu_num();
const reg_t vstart = P.VU.vstart->read();

for (int vu_idx=0; vu_idx<static_cast<int>(n_vu); vu_idx++) {
    P.VU.vstart->write(vstart);
    for (reg_t i = 0; i < vl; ++i) {
        VI_STRIP(i);
        P.VU.vstart->write(i);
        int32_t val = P.VU.elt<int32_t>(vd, vreg_inx, true, vu_idx);
        P.SA.input_vpush(vu_idx, val);
    }
}
P.VU.vstart->write(0);
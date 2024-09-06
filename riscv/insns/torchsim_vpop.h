reg_t vd = insn.rd();
const reg_t vl = P.VU.vl->read();
const uint32_t n_vu = P.VU.get_vu_num();
const reg_t vstart = P.VU.vstart->read();

for (int vu_idx=0; vu_idx<static_cast<int>(n_vu); vu_idx++) {
    P.VU.vstart->write(vstart);
    for (reg_t i = 0; i < vl; ++i) {
        if (P.SA.output[vu_idx]->empty())
            break;

        VI_STRIP(i);
        P.VU.vstart->write(i);
        uint32_t val = P.SA.output_pop(vu_idx);
        P.VU.elt<uint32_t>(vd, vreg_inx, true, vu_idx) = val;
    }
}
P.VU.vstart->write(0);
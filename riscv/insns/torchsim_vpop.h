reg_t vd = insn.rd();
const reg_t vl = P.VU.vl->read();
const uint32_t n_vu = P.VU.get_vu_num();
const reg_t vstart = P.VU.vstart->read();

for (reg_t vu_idx=0; vu_idx<n_vu; vu_idx++) {
    P.VU.vstart->write(vstart);
    for (reg_t i = 0; i < vl; ++i) {
        if (P.SA.output[vu_idx]->empty())
            break;

        VI_STRIP(i);
        P.VU.vstart->write(i);
        float val = P.SA.output_pop(vu_idx);
        P.VU.elt<float>(vd, vreg_inx, vu_idx, true) = val;
    }
}
P.VU.vstart->write(0);
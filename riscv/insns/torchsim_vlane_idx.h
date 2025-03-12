// torchsim_vlane_idx

const reg_t vd = insn.rd();
const reg_t vl = P.VU.vl->read();
const reg_t n_vu = P.VU.get_vu_num();
const reg_t vstart = P.VU.vstart->read();

for (reg_t vu_idx=0; vu_idx<n_vu; vu_idx++) {
    P.VU.vstart->write(vstart);
    for (reg_t i=0; i<vl; i++) {
        VI_STRIP(i);
        P.VU.vstart->write(i);
        int64_t val = static_cast<int64_t>(vu_idx);
        P.VU.elt<int64_t>(vd, vreg_inx, vu_idx, true) = val;
    }
}
P.VU.vstart->write(0);
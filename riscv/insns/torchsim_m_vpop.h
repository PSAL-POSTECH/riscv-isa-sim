// The multi-precision array's pop: each lane's f32 results, narrowed to the current
// SEW. SIMM5 [1:0] names the format an e8 pop narrows to (msaUnit_t::fmt_t); an
// idle column pops the 0 its compute produced.

const reg_t vd = insn.rd();
const uint32_t simm5 = (uint32_t)(insn.v_simm5() & 0x1f);
const uint32_t fmt = msaUnit_t::fmt(simm5);
const reg_t vl = P.VU.vl->read();
const reg_t n_vu = P.VU.get_vu_num();
const reg_t vstart = P.VU.vstart->read();
msaUnit_t* msa = P.MSA;

for (reg_t vu_idx = 0; vu_idx < n_vu; vu_idx++) {
    P.VU.vstart->write(vstart);
    for (reg_t i = 0; i < vl; ++i) {
        float val;
        if (!msa->pop(vu_idx, val))
            break;
        VI_STRIP(i);
        P.VU.vstart->write(i);
        float32_t fp32;
        memcpy(&fp32.v, &val, sizeof(float));
        switch (P.VU.vsew) {
          case e8:
            softfloat_fp8Format = (fmt == msaUnit_t::FMT_E5M2) ? softfloat_fp8_e5m2
                                                               : softfloat_fp8_e4m3;
            P.VU.elt<float8_t>(vd, vreg_inx, vu_idx, true) = f32_to_f8(fp32);
            break;
          case e16:
            P.VU.elt<float16_t>(vd, vreg_inx, vu_idx, true) = f32_to_f16(fp32);
            break;
          default:
            P.VU.elt<float>(vd, vreg_inx, vu_idx, true) = val;
            break;
        }
    }
}
P.VU.vstart->write(0);

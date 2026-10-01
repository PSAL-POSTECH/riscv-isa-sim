// The multi-precision array's push, one encoding for both operands. SIMM5: [4] weight,
// [3:2] active-width shift, [1:0] format (msaUnit_t::fmt_t). Issued at SEW=32: each
// element is one lane word holding pack K-elements, the lowest bits first.

const reg_t vs = insn.rs2();
const uint32_t simm5 = (uint32_t)(insn.v_simm5() & 0x1f);
const reg_t vl = P.VU.vl->read();
const reg_t n_vu = P.VU.get_vu_num();
const reg_t vstart = P.VU.vstart->read();
require(P.VU.vsew == e32);

msaUnit_t* msa = P.MSA;
const bool is_weight = msaUnit_t::is_weight(simm5);
const uint32_t fmt = msaUnit_t::fmt(simm5);
const uint32_t pk = msaUnit_t::pack(fmt);
const uint32_t eb = 32 / pk;
const uint32_t lo_mask = (eb == 32) ? 0xffffffffu : ((1u << eb) - 1);

if (is_weight)
    msa->configure(simm5);
else
    require(pk == msa->get_pack());

auto unpack = [&](uint32_t word, uint32_t s) -> float {
    const uint32_t bits = (word >> (s * eb)) & lo_mask;
    float32_t r;
    switch (fmt) {
      case msaUnit_t::FMT_F32:
        r.v = bits;
        break;
      case msaUnit_t::FMT_F16: {
        float16_t h;
        h.v = (uint16_t)bits;
        r = f16_to_f32(h);
        break;
      }
      default: {
        softfloat_fp8Format = (fmt == msaUnit_t::FMT_E5M2) ? softfloat_fp8_e5m2
                                                           : softfloat_fp8_e4m3;
        float8_t b;
        b.v = (uint8_t)bits;
        r = f8_to_f32(b);
        break;
      }
    }
    float val;
    memcpy(&val, &r.v, sizeof(float));
    return val;
};

if (is_weight) {
    // Lane = output column; element i holds K-elements [i*pk, i*pk + pk).
    for (reg_t vu_idx = 0; vu_idx < n_vu; vu_idx++) {
        for (reg_t i = 0; i < vl; ++i) {
            VI_STRIP(i);
            P.VU.vstart->write(i);
            const uint32_t word = P.VU.elt<uint32_t>(vs, vreg_inx, vu_idx);
            for (uint32_t s = 0; s < pk; s++)
                msa->push_weight(vu_idx, unpack(word, s));
        }
    }
} else {
    // Element = A row; lane j holds K-elements [j*pk, j*pk + pk) of that row.
    std::vector<float> row(n_vu * pk);
    for (reg_t i = 0; i < vl; ++i) {
        VI_STRIP(i);
        P.VU.vstart->write(i);
        for (reg_t vu_idx = 0; vu_idx < n_vu; vu_idx++) {
            const uint32_t word = P.VU.elt<uint32_t>(vs, vreg_inx, vu_idx);
            for (uint32_t s = 0; s < pk; s++)
                row[vu_idx * pk + s] = unpack(word, s);
        }
        msa->push_input_row(row);
    }
}
P.VU.vstart->write(0);

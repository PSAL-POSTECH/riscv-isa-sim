// vfncvt.sat.f.f.q vd, vs2, vm: vfncvt.f.f.q whose infinite results saturate
// to the largest finite fp8 value of the same sign.
VI_VFP_NCVT_QUAD(
{
  auto vs2 = P.VU.elt<float32_t>(rs2_num, i, vu_idx);
  P.VU.elt<float8_t>(rd_num, i, vu_idx, true) = f32_to_f8_sat(vs2);
},
{
  require(p->extension_enabled(EXT_ZVFOFP8MIN));
})

// vfncvt.f.f.q vd, vs2, vm: f32 -> fp8 (altfmt picks E5M2), one rounding by frm.
// Infinite results: E5M2 keeps the infinity, E4M3 gives the canonical NaN 0x7F.
VI_VFP_NCVT_QUAD(
{
  auto vs2 = P.VU.elt<float32_t>(rs2_num, i, vu_idx);
  P.VU.elt<float8_t>(rd_num, i, vu_idx, true) = f32_to_f8(vs2);
},
{
  require(p->extension_enabled(EXT_ZVFOFP8MIN));
})

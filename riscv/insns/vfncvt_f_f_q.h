// vfncvt.f.f.q vd, vs2, vm
// f32 STRAIGHT TO fp8, not by way of f16: going through e16 rounds twice, and a
// value that lands exactly between two fp8 numbers there loses the bit that said
// which side it came from. One rounding is the only way the answer is the right one.
VI_VFP_NCVT_QUAD(
{
  auto vs2 = P.VU.elt<float32_t>(rs2_num, i, vu_idx);
  P.VU.elt<float8_t>(rd_num, i, vu_idx, true) = f32_to_f8(vs2);
},
{
  require(p->extension_enabled(EXT_ZVFP8));
})

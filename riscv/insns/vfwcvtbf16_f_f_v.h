// vfwcvtbf16.f.f.v vd, vs2, vm: SEW=8 is fp8 -> bf16 (Zvfofp8min, exact, no
// flags at all); SEW=16 is bf16 -> f32 (Zvfbfmin, exact).
VI_VFP_CVT_SCALE
({
  auto vs2 = P.VU.elt<float8_t>(rs2_num, i, vu_idx);
  P.VU.elt<bfloat16_t>(rd_num, i, vu_idx, true) = f8_to_bf16(vs2);
},
{
  auto vs2 = P.VU.elt<bfloat16_t>(rs2_num, i, vu_idx);
  P.VU.elt<float32_t>(rd_num, i, vu_idx, true) = bf16_to_f32(vs2);
},
{
  require(0);
},
{
  require(p->extension_enabled(EXT_ZVFOFP8MIN));
},
{
  require(p->extension_enabled(EXT_ZVFBFMIN));
},
{
},
true, (P.VU.vsew == e8 || P.VU.vsew == e16))

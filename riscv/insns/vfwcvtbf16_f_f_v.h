// vfwcvtbf16.f.f.v vd, vs2, vm: bf16 -> f32, exact; only SEW=16 exists.
VI_VFP_CVT_SCALE
({
  require(0);
},
{
  auto vs2 = P.VU.elt<bfloat16_t>(rs2_num, i, vu_idx);
  P.VU.elt<float32_t>(rd_num, i, vu_idx, true) = bf16_to_f32(vs2);
},
{
  require(0);
},
{
},
{
  require(p->extension_enabled(EXT_ZVFBFMIN));
},
{
},
true, (P.VU.vsew == e16))

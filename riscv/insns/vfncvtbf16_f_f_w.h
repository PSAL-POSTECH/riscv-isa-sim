// vfncvtbf16.f.f.w vd, vs2, vm: f32 -> bf16, rounded by frm; only SEW=16 exists.
VI_VFP_CVT_SCALE
({
  require(0);
},
{
  auto vs2 = P.VU.elt<float32_t>(rs2_num, i, vu_idx);
  P.VU.elt<bfloat16_t>(rd_num, i, vu_idx, true) = f32_to_bf16(vs2);
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
false, (P.VU.vsew == e16))

// vfncvtbf16.f.f.w vd, vs2, vm: SEW=8 is bf16 -> fp8 (Zvfofp8min), SEW=16 is
// f32 -> bf16 (Zvfbfmin); both round once by frm.
VI_VFP_CVT_SCALE
({
  auto vs2 = P.VU.elt<bfloat16_t>(rs2_num, i, vu_idx);
  P.VU.elt<float8_t>(rd_num, i, vu_idx, true) = bf16_to_f8(vs2);
},
{
  auto vs2 = P.VU.elt<float32_t>(rs2_num, i, vu_idx);
  P.VU.elt<bfloat16_t>(rd_num, i, vu_idx, true) = f32_to_bf16(vs2);
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
false, (P.VU.vsew == e8 || P.VU.vsew == e16))

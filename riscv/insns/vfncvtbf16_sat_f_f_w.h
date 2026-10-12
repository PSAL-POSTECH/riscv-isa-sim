// vfncvtbf16.sat.f.f.w vd, vs2, vm: bf16 -> fp8 at SEW=8 only, rounded by frm,
// infinite results saturating to the largest finite value of the same sign.
VI_VFP_CVT_SCALE
({
  auto vs2 = P.VU.elt<bfloat16_t>(rs2_num, i, vu_idx);
  P.VU.elt<float8_t>(rd_num, i, vu_idx, true) = bf16_to_f8_sat(vs2);
},
{
  require(0);
},
{
  require(0);
},
{
  require(p->extension_enabled(EXT_ZVFOFP8MIN));
},
{
},
{
},
false, (P.VU.vsew == e8))

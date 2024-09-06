// Custom instruction config_mvout
// config_mvout rs1, rs2
// rs1 = main memory stride
// rs2[31:0] = element size
// rs2[32] = is transpose (is row-wise)

P.VU.out_mm_stride = RS1;
P.VU.out_element_size = (RS2 & ((1ULL << 32) - 1));
P.VU.out_is_transpose = (RS2 >> 32) & 1;
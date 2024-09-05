// Custom instruction config_mvin
// config_mvin rs1, rs2
// rs1 = main memory stride
// rs2[31:0] = element size
// rs2[32] = is transpose (is row-wise)

P.VU.in_mm_stride = RS1;
P.VU.in_element_size = (RS2 & ((1 << 32) - 1));
P.VU.in_is_transpose = (RS2 >> 32) & 1;
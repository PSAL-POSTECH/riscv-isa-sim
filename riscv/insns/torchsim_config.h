// Custom instruction config_mvin
// config_mvin rs1, rs2
// rs1 = main-memory stride
// rs2[15:0] = element size
// rs2[16] = is_col_major
// rs2[18:17] = mvin setting_idx to set (0 ~ 3)
// rs2[63:32] = chunk size
enum {
  MVIN = 0,
  MVIN1,
  MVIN2,
  MVOUT,
} config_t;

const int config_type = (RS2 >> 17) & 0b11; // 2 bits
if (config_type == MVIN) {
  P.VU.in_mm_stride[0] = RS1 >> 32;
  P.VU.in_spad_mm_stride[0] = RS1 & ((1ULL << 32) - 1);
  P.VU.in_element_size[0] = RS2 & ((1ULL << 16) - 1);
  P.VU.in_is_col_major[0] = (RS2 >> 16) & 0b1;
  P.VU.in_chunk_size[0] = RS2 >> 32;
} else if (config_type == MVIN1) {
  P.VU.in_mm_stride[1] = RS1 >> 32;
  P.VU.in_spad_mm_stride[1] = RS1 & ((1ULL << 32) - 1);
  P.VU.in_element_size[1] = RS2 & ((1ULL << 16) - 1);
  P.VU.in_is_col_major[1] = (RS2 >> 16) & 0b1;
  P.VU.in_chunk_size[1] = RS2 >> 32;
} else if (config_type == MVIN2) {
  P.VU.in_mm_stride[2] = RS1 >> 32;
  P.VU.in_spad_mm_stride[2] = RS1 & ((1ULL << 32) - 1);
  P.VU.in_mm_stride[2] = RS1;
  P.VU.in_element_size[2] = RS2 & ((1ULL << 16) - 1);
  P.VU.in_is_col_major[2] = (RS2 >> 16) & 0b1;
  P.VU.in_chunk_size[2] = RS2 >> 32;
} else if (config_type == MVOUT) {
  P.VU.out_mm_stride = RS1 >> 32;
  P.VU.out_spad_mm_stride = RS1 & ((1ULL << 32) - 1);
  P.VU.out_element_size = RS2 & ((1ULL << 16) - 1);
  P.VU.out_is_col_major = (RS2 >> 16) & 0b1;
  P.VU.out_chunk_size = RS2 >> 32;
} else {
  // Invalid config type
  assert(0);
}
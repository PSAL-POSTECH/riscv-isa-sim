// Custom instruction mvin
// mvin rs1, rs2
// rs1 = virtual DRAM address
// rs2[31:0] = scratchpad address
// rs2[47:32] = number of columns to load in
// rs2[63:48] = number of rows to load in

const reg_t dramAddr = RS1;
const reg_t scratchpadAddr = (uint32_t)(RS2 & ((1ULL << 32) - 1));
const reg_t n_col = (RS2 >> 32) & ((1 << 16) - 1);
const reg_t n_row = RS2 >> 48;
const reg_t mm_stride = P.VU.in_mm_stride;
const reg_t element_size = P.VU.in_element_size;
const reg_t is_transpose = P.VU.in_is_transpose;
const uint32_t n_vu = P.VU.get_vu_num();

assert(mm_stride > 0);
assert(element_size > 0);

uint32_t element_per_vlane = is_transpose ? n_col : n_row;
uint32_t sram_stride = is_transpose ? element_size : mm_stride;    // stride in tiled matrix within vector lane data
uint32_t vlane_stride = is_transpose ? mm_stride : element_size;   // stride in tiled matrix between vector lane data

for (int vu_idx=0; vu_idx<static_cast<int>(n_vu); vu_idx++) {
    reg_t dram_base = dramAddr + vu_idx * vlane_stride;
    reg_t sram_base = scratchpadAddr + vu_idx * P.VU.vu_sram_byte;
    for (int count_in_vlane=0; count_in_vlane<static_cast<int>(element_per_vlane); count_in_vlane++) {
        reg_t d_addr = dram_base + sram_stride * count_in_vlane;
        reg_t s_addr = sram_base + element_size * count_in_vlane;
        uint32_t val = MMU.load_uint32(d_addr);
        MMU.store_uint32(s_addr, val);
    }
}
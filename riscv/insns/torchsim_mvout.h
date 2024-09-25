// mvout rs1, rs2
// rs1 = virtual DRAM address
// rs2[31:0] = scratchpad address
// rs2[47:32] = number of columns to load in
// rs2[63:48] = number of rows to load in

#define ROW 0
#define COL 1

const reg_t dramAddr = RS1;
const reg_t scratchpadAddr = (uint32_t)(RS2 & ((1ULL << 32) - 1));
const reg_t n_col = (RS2 >> 32) & ((1 << 16) - 1);
const reg_t n_row = RS2 >> 48;
const reg_t mm_stride = P.VU.out_mm_stride;
const reg_t element_size = P.VU.out_element_size;
const reg_t chunk_size = P.VU.out_chunk_size;
const bool is_col_major = P.VU.out_is_col_major;
const uint32_t n_vu = P.VU.get_vu_num();

assert(mm_stride > 0);
assert(element_size > 0);
assert(chunk_size > 0);

const reg_t n_elements_per_chunk = chunk_size / element_size;
const bool lane_split_axis = n_elements_per_chunk < n_col ? COL : ROW;
reg_t n_used_vlane  = 0;

// printf("n_col: %d, n_row: %d, mm_stride: %d, element_size: %d, chunk_size: %d, is_col_major: %d, n_vu: %d\n", n_col, n_row, mm_stride, element_size, chunk_size, is_col_major, n_vu);

if (lane_split_axis)
    n_used_vlane = n_col / n_elements_per_chunk;
else
    n_used_vlane = n_row / (n_elements_per_chunk / n_col);

assert(n_used_vlane <= n_vu);

const reg_t block_h = lane_split_axis ? n_row : n_row / n_used_vlane;
const reg_t block_w = lane_split_axis ? n_col / n_used_vlane : n_col;
const reg_t next_element_stride = is_col_major ? mm_stride : element_size;
const reg_t next_line_stride = is_col_major ? element_size : mm_stride;
const reg_t logical_block_h = is_col_major ? block_w : block_h;
const reg_t logical_block_w = is_col_major ? block_h : block_w;

for (reg_t lane_idx=0; lane_idx<n_vu; lane_idx++) {
    reg_t dram_base = dramAddr + lane_idx * chunk_size;
    reg_t sram_base = scratchpadAddr + lane_idx * P.VU.vu_sram_byte;
    if (lane_idx < n_used_vlane) {
        for (reg_t b_h=0; b_h<logical_block_h; b_h++) {
            reg_t dram_line_offset = b_h * next_line_stride;
            for (reg_t b_w=0; b_w<logical_block_w; b_w++) {
                reg_t s_addr = sram_base + element_size * (b_h * logical_block_w + b_w);
                reg_t d_addr = dram_base + dram_line_offset + next_element_stride * b_w;
                if (element_size == 8){
                    uint64_t val = MMU.load_uint64(s_addr);
                    MMU.store_uint64(d_addr, val);
                } else if (element_size == 4){
                    uint32_t val = MMU.load_uint32(s_addr);
                    MMU.store_uint32(d_addr, val);
                } else if (element_size == 2){
                    uint16_t val = MMU.load_uint16(s_addr);
                    MMU.store_uint16(d_addr, val);
                } else if (element_size == 1){
                    uint8_t val = MMU.load_uint8(s_addr);
                    MMU.store_uint8(d_addr, val);
                }
            }
        }
    } else
        break;
}
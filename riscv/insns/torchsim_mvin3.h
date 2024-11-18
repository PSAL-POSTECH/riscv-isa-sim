// Custom instruction mvin
// mvin rs1, rs2
// rs1[61:0] = virtual DRAM address
// rs1[63:62]: # mvin setting to use (0~3)
// rs2[31:0] = scratchpad address
// rs2[47:32] = number of columns to load in
// rs2[63:48] = number of rows to load in
#define ROW 0
#define COL 1

const char* debug_env = std::getenv("SPIKE_DEBUG");
const int debug_flag = debug_env ? std::stoi(debug_env) : 0;

const reg_t dramAddr = RS1;
const reg_t setting_idx = 2;
const reg_t scratchpadAddr = (uint32_t)(RS2 & ((1ULL << 32) - 1));
const reg_t n_col = (RS2 >> 32) & ((1 << 16) - 1);
const reg_t n_row = RS2 >> 48;
const reg_t mm_stride = P.VU.in_mm_stride[setting_idx];
const reg_t spad_mm_stride = P.VU.in_spad_mm_stride[setting_idx];
const reg_t element_size = P.VU.in_element_size[setting_idx];
const reg_t chunk_size = P.VU.in_chunk_size[setting_idx];
const bool is_col_major = P.VU.in_is_col_major[setting_idx];
const reg_t n_vu = P.VU.get_vu_num();

// assert(mm_stride > 0);
assert(element_size > 0);
assert(chunk_size > 0);

const reg_t n_elements_per_chunk = chunk_size / element_size;
const bool lane_split_axis = n_elements_per_chunk < n_col ? COL : ROW;
reg_t n_used_vlane  = 0;

if (lane_split_axis)
    n_used_vlane = n_col / n_elements_per_chunk;
else
    n_used_vlane = n_row / (n_elements_per_chunk / n_col);

reg_t outer_loop = 1;
if (n_used_vlane > n_vu) {
    assert(n_used_vlane % n_vu == 0);
    outer_loop = n_used_vlane / n_vu;
    n_used_vlane = n_vu;
}
assert(n_used_vlane <= n_vu);

const reg_t block_h = lane_split_axis ? n_row : n_row / n_used_vlane;
const reg_t block_w = lane_split_axis ? n_col / n_used_vlane : n_col;
const reg_t dram_vlane_offet = lane_split_axis ? chunk_size : mm_stride * block_h;
const reg_t next_element_stride = is_col_major ? mm_stride : element_size;
const reg_t next_line_stride = is_col_major ? element_size : mm_stride;
const reg_t logical_block_h = is_col_major ? block_w : block_h;
const reg_t logical_block_w = is_col_major ? block_h : block_w;

if (debug_flag) {
    printf("======== MVIN =========\n");
    printf("mvin: dramAddr: 0x%lx, scratchpadAddr: 0x%lx\n\
n_col: %ld, n_row: %ld\n\
mm_stride: %ld, element_size: %ld, chunk_size: %ld, is_col_major: %d\n\
n_vu: %ld, n_used_vlane: %ld\n\
block_h: %ld, block_w: %ld\n\
dram_vlane_offet: %ld, next_element_stride: %ld, next_line_stride: %ld\n\
logical_block_h: %ld, logical_block_w: %ld\n",
    dramAddr, scratchpadAddr,
    n_col, n_row,
    mm_stride, element_size, chunk_size, is_col_major,
    n_vu, n_used_vlane,
    block_h, block_w,
    dram_vlane_offet, next_element_stride, next_line_stride,
    logical_block_h, logical_block_w);
}

for (reg_t outer_idx=0; outer_idx<outer_loop; outer_idx++) {
    reg_t dram_outer_base = dramAddr + outer_idx * n_vu * element_size;
    reg_t sram_outer_base = scratchpadAddr + outer_idx * spad_mm_stride;
    for (reg_t lane_idx=0; lane_idx<n_vu; lane_idx++) {
        reg_t dram_base = dram_outer_base + lane_idx * dram_vlane_offet;
        reg_t sram_base = sram_outer_base + lane_idx * P.VU.vu_sram_byte;
        if (lane_idx < n_used_vlane) {
            if (debug_flag) {
                printf("=========[%ld]========\n", lane_idx);
            }
            for (reg_t b_h=0; b_h<logical_block_h; b_h++) {
                reg_t dram_line_offset = b_h * next_line_stride;
                if (debug_flag) {
                    printf("block_idx [%ld]\n", b_h);
                }
                for (reg_t b_w=0; b_w<logical_block_w; b_w++) {
                    reg_t d_addr = dram_base + dram_line_offset + next_element_stride * b_w;
                    reg_t s_addr = sram_base + element_size * (b_h * logical_block_w + b_w);
                    if (element_size == 8){
                        uint64_t val = MMU.load_uint64(d_addr);
                        MMU.store_uint64(s_addr, val);
                        if (debug_flag) {
                            printf("DRAM_ADDR: 0x%x SRAM_ADDR: 0x%x %lf, ", d_addr, s_addr, *((double*)&val));
                        }
                    } else if (element_size == 4){
                        uint32_t val = MMU.load_uint32(d_addr);
                        MMU.store_uint32(s_addr, val);
                        if (debug_flag) {
                            printf("DRAM_ADDR: 0x%x SRAM_ADDR: 0x%x %f, ", d_addr, s_addr, *((float*)&val));
                        }
                    } else if (element_size == 2){
                        uint16_t val = MMU.load_uint16(d_addr);
                        MMU.store_uint16(s_addr, val);
                        if (debug_flag) {
                            printf("DRAM_ADDR: 0x%x SRAM_ADDR: 0x%x %x, ", d_addr, s_addr, *((short*)&val));
                        }
                    } else if (element_size == 1){
                        uint8_t val = MMU.load_uint8(d_addr);
                        MMU.store_uint8(s_addr, val);
                        if (debug_flag) {
                            printf("DRAM_ADDR: 0x%x SRAM_ADDR: 0x%x %x, ", d_addr, s_addr, *((char*)&val));
                        }
                    }
                }
                if (debug_flag) {
                    printf("\n");
                }
            }
        } else {
            // Zero padding
            for (reg_t b_h=0; b_h<logical_block_h; b_h++) {
                for (reg_t b_w=0; b_w<logical_block_w; b_w++) {
                    reg_t s_addr = sram_base + element_size * (b_h * logical_block_w + b_w);
                    if (element_size == 8)
                        MMU.store_uint64(s_addr, 0);
                    else if (element_size == 4)
                        MMU.store_uint32(s_addr, 0);
                    else if (element_size == 2)
                        MMU.store_uint16(s_addr, 0);
                    else if (element_size == 1)
                        MMU.store_uint8(s_addr, 0);
                }
            }
        }
    }
}
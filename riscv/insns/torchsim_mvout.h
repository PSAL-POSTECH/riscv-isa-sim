// mvout rs1, rs2
// rs1 = virtual DRAM address
// rs2[31:0] = scratchpad address
// rs2[47:32] = number of columns to load in
// rs2[63:48] = number of rows to load in

#define ROW 0
#define COL 1
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))

const char* debug_env = std::getenv("SPIKE_DEBUG");
const int debug_flag = debug_env ? std::stoi(debug_env) : 0;

const reg_t dramAddr = RS1;
const reg_t scratchpadAddr = (uint32_t)(RS2 & ((1ULL << 32) - 1));
const reg_t n_col = (RS2 >> 32) & ((1 << 16) - 1);
const reg_t n_row = RS2 >> 48;
const reg_t mm_stride = P.VU.out_mm_stride;
const reg_t element_size = P.VU.out_element_size;
const reg_t chunk_size = P.VU.out_chunk_size;
const bool is_col_major = P.VU.out_is_col_major;
const reg_t n_vu = P.VU.get_vu_num();

assert(mm_stride > 0);
assert(element_size > 0);
assert(chunk_size > 0);

const reg_t n_elements_per_chunk = chunk_size / element_size;
const bool lane_split_axis = n_elements_per_chunk < n_col ? COL : ROW;
reg_t n_used_vlane  = 0;
reg_t n_used_vlane_per_subtile = 0;

if (lane_split_axis)
    n_used_vlane = n_col / n_elements_per_chunk;
else
    n_used_vlane = n_row / (n_elements_per_chunk / n_col);
const reg_t nr_blocks = MAX(n_used_vlane / n_vu, 1);

const reg_t block_h = lane_split_axis ? n_row : n_row / n_used_vlane;
const reg_t block_w = lane_split_axis ? n_col / n_used_vlane : n_col;
const reg_t dram_vlane_offet = lane_split_axis ? chunk_size : mm_stride * block_h;
const reg_t next_element_stride = is_col_major ? mm_stride : element_size;
const reg_t next_line_stride = is_col_major ? element_size : mm_stride;
const reg_t logical_block_h = is_col_major ? block_w : block_h;
const reg_t logical_block_w = is_col_major ? block_h : block_w;
const reg_t dram_block_offset = lane_split_axis ? n_vu * chunk_size : block_h * block_w * element_size;
const reg_t sram_block_offset = lane_split_axis ? n_row * element_size : chunk_size * n_vu;
if (debug_flag) {
    printf("======== MVOUT =========\n");
    printf("mvin: dramAddr: 0x%lx, scratchpadAddr: 0x%lx\n\
n_col: %ld, n_row: %ld\n\
mm_stride: %ld, element_size: %ld, chunk_size: %ld, is_col_major: %d\n\
n_vu: %ld, n_used_vlane: %ld\n\
block_h: %ld, block_w: %ld\n\
dram_vlane_offet: %ld, next_element_stride: %ld, next_line_stride: %ld\n\
logical_block_h: %ld, logical_block_w: %ld\n\
nr_blocks %ld: dram_block_offset: %ld, sram_block_offset: %ld\n",
    dramAddr, scratchpadAddr,
    n_col, n_row,
    mm_stride, element_size, chunk_size, is_col_major,
    n_vu, n_used_vlane,
    block_h, block_w,
    dram_vlane_offet, next_element_stride, next_line_stride,
    logical_block_h, logical_block_w,
    nr_blocks, dram_block_offset, sram_block_offset);
}

assert(n_used_vlane % n_vu == 0);

for (reg_t lane_idx=0; lane_idx<n_vu; lane_idx++) {
    reg_t dram_base = dramAddr + lane_idx * dram_vlane_offet;
    reg_t sram_base = scratchpadAddr + lane_idx * P.VU.vu_sram_byte;
    if (lane_idx < n_used_vlane) {
        if (debug_flag) {
            printf("=========[%ld]========\n", lane_idx);
        }
        for (reg_t b_idx=0; b_idx<nr_blocks; b_idx++) {
            if (debug_flag) {
                printf("block_idx [%ld]\n", b_idx);
            }
            for (reg_t b_h=0; b_h<logical_block_h; b_h++) {
                reg_t dram_line_offset = b_h * next_line_stride;
                if (debug_flag) {
                    printf("[%ld] ", b_h);
                }
                for (reg_t b_w=0; b_w<logical_block_w; b_w++) {
                    reg_t d_addr = dram_base + b_idx*dram_block_offset + dram_line_offset + next_element_stride * b_w;
                    reg_t s_addr = sram_base + b_idx*sram_block_offset + element_size * (b_h * logical_block_w + b_w);
                    if (element_size == 8){
                        uint64_t val = MMU.load_uint64(s_addr);
                        MMU.store_uint64(d_addr, val);
                        if (debug_flag) {
                            printf("DRAM_ADDR: 0x%x SRAM_ADDR: 0x%x %lf, ", d_addr, s_addr, *((double*)&val));
                        }
                    } else if (element_size == 4){
                        uint32_t val = MMU.load_uint32(s_addr);
                        MMU.store_uint32(d_addr, val);
                        if (debug_flag) {
                            printf("DRAM_ADDR: 0x%x SRAM_ADDR: 0x%x %f, ", d_addr, s_addr, *((float*)&val));
                        }
                    } else if (element_size == 2){
                        uint16_t val = MMU.load_uint16(s_addr);
                        MMU.store_uint16(d_addr, val);
                        if (debug_flag) {
                            printf("DRAM_ADDR: 0x%x SRAM_ADDR: 0x%x %x, ", d_addr, s_addr, *((short*)&val));
                        }
                    } else if (element_size == 1){
                        uint8_t val = MMU.load_uint8(s_addr);
                        MMU.store_uint8(d_addr, val);
                        if (debug_flag) {
                            printf("DRAM_ADDR: 0x%x SRAM_ADDR: 0x%x %x, ", d_addr, s_addr, *((char*)&val));
                        }
                    }
                }
                if (debug_flag) {
                    printf("\n");
                }
            }
        }
    } else
        break;
}
// mvout rs1, rs2
// rs1 = virtual DRAM address
// rs2[31:0] = scratchpad address
// rs2[47:32] = number of columns to load in
// rs2[63:48] = number of rows to load in

const reg_t dramAddr = RS1;
const reg_t scratchpadAddr = (uint32_t)(RS2 & ((1 << 32) - 1));
const reg_t n_col = (RS2 >> 32) & ((1 << 16) - 1);
const reg_t n_row = RS2 >> 48;

const reg_t mm_stride = 8; // Todo: make it configurable
const uint32_t n_vu = P.VU.get_vu_num();
uint32_t vu_idx = 0;

for (int r=0; r<n_row; r++) {
    for (int c=0; c<n_col; c++) {
        vu_idx = vu_idx >= n_vu ? 0: vu_idx;

        reg_t addr = dramAddr + (r * mm_stride + c) * sizeof(uint32_t);
        reg_t va = scratchpadAddr + (r * n_col + c) * sizeof(uint32_t);
        uint32_t val = (uint32_t)MMU.load_uint32(va);
        MMU.store_uint32(addr, val);
        vu_idx++;
    }
}
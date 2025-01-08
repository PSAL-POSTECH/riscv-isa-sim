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

const reg_t n_vu = P.VU.get_vu_num();

const reg_t dramAddr = RS1;
const reg_t scratchpadAddr = RS2;
const reg_t *p_dim_size = P.VU.dma_dim_size;
const reg_t *p_mm_stride = P.VU.dma_mm_stride;
const reg_t *p_spad_stride = P.VU.dma_spad_stride;
const reg_t element_size = P.VU.dma_element_size;
const reg_t vlane_stride = P.VU.dma_vlane_stride;
const int vlane_split_axis = P.VU.dma_vlane_split_axis;
const bool is_col_major = P.VU.dma_is_col_major;
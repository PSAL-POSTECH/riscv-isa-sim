// Custom instruction mvin
// mvin rs1, rs2
// rs1 = virtual main memory address
// rs2: scratchpad address

#define N 0
#define C 1
#define H 2
#define W 3

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
int n_outerloop = 1;

if (vlane_split_axis == N)
    n_outerloop = (p_dim_size[0] + (vlane_stride * n_vu) - 1) / (vlane_stride * n_vu);
else if (vlane_split_axis == C)
    n_outerloop = (p_dim_size[1] + (vlane_stride * n_vu) - 1) / (vlane_stride * n_vu);
else if (vlane_split_axis == H)
    n_outerloop = (p_dim_size[2] + (vlane_stride * n_vu) - 1) / (vlane_stride * n_vu);
else if (vlane_split_axis == W)
    n_outerloop = (p_dim_size[3] + (vlane_stride * n_vu) - 1) / (vlane_stride * n_vu);
else
    assert(0);

if (debug_flag) {
    printf("=============== MVOUT ===============\n");
    printf("- dramAddr: 0x%lx\n", dramAddr);
    printf("- scratchpadAddr: 0x%lx\n", scratchpadAddr);
    printf("- p_dim_size: (%ld, %ld, %ld, %ld)\n", p_dim_size[0], p_dim_size[1], p_dim_size[2], p_dim_size[3]);
    printf("- p_mm_stride: (%ld, %ld, %ld, %ld)\n", p_mm_stride[0], p_mm_stride[1], p_mm_stride[2], p_mm_stride[3]);
    printf("- p_spad_stride: (%ld, %ld, %ld, %ld)\n", p_spad_stride[0], p_spad_stride[1], p_spad_stride[2], p_spad_stride[3]);
    printf("- element_size: %ld\n", element_size);
    printf("- vlane_stride: %ld\n", vlane_stride);
    printf("- vlane_split_axis: %d\n", vlane_split_axis);
    printf("- Outer loop: %d\n", n_outerloop);
}

assert(element_size > 0);
assert(vlane_stride > 0);

void *dma_tensor = nullptr;
if (element_size == 1)
    dma_tensor = new uint8_t[p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]];
else if (element_size == 2)
    dma_tensor = new uint16_t[p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]];
else if (element_size == 4)
    dma_tensor = new uint32_t[p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]];
else if (element_size == 8)
    dma_tensor = new uint64_t[p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]];
else
    assert(0);

reg_t n, c, h, w;
reg_t *p_split_dim;

if (vlane_split_axis == N)
    p_split_dim = &n;
else if (vlane_split_axis == C)
    p_split_dim = &c;
else if (vlane_split_axis == H)
    p_split_dim = &h;
else if (vlane_split_axis == W)
    p_split_dim = &w;
else
    assert(0);

// Load data from scratchpad by spad_stride
reg_t used_vlane = (p_dim_size[vlane_split_axis] + vlane_stride - 1) / vlane_stride;
reg_t block_shape[4] = {p_dim_size[0], p_dim_size[1], p_dim_size[2], p_dim_size[3]};
block_shape[vlane_split_axis] = vlane_stride;

if (debug_flag) {
    printf("- Block_shape: (%ld, %ld, %ld, %ld)\n", block_shape[0], block_shape[1], block_shape[2], block_shape[3]);
}

reg_t factor, outerloop_factor, outerloop_stride;
if (vlane_split_axis == N) {
    factor = p_dim_size[1] * p_dim_size[2] * p_dim_size[3];
    outerloop_stride = p_spad_stride[0];
    outerloop_factor = block_shape[0] * block_shape[1] * block_shape[2] * n_vu * vlane_stride;
} else if (vlane_split_axis == C) {
    factor = p_dim_size[2] * p_dim_size[3];
    outerloop_stride = p_spad_stride[1];
    outerloop_factor = block_shape[0] * block_shape[1] * n_vu * vlane_stride;
} else if (vlane_split_axis == H) {
    factor = p_dim_size[3];
    outerloop_stride = p_spad_stride[2];
    outerloop_factor = block_shape[0] * n_vu * vlane_stride;
} else if (vlane_split_axis == W) {
    factor = 1;
    outerloop_stride = p_spad_stride[3];
    outerloop_factor = n_vu * vlane_stride;
}

if (debug_flag) {
    printf("- factor = %ld\n", factor);
    printf("- outerloop_factor = %ld\n", outerloop_factor);
    printf("- outerloop_stride = %ld\n", outerloop_stride);
}

for (int outerloop_idx=0; outerloop_idx<n_outerloop; outerloop_idx++){
    for (int vlane_idx=0; vlane_idx<n_vu; vlane_idx++) {
        bool padding_flag = vlane_idx >= used_vlane;
        if (debug_flag) {
            printf("<<< Vlane: %d, Padding: %d >>>\n", vlane_idx, padding_flag);
        }
        for (n=0; n<block_shape[0]; n++) {
            for (c=0; c<block_shape[1]; c++) {
                for (h=0; h<block_shape[2]; h++) {
                    for (w=0; w<block_shape[3]; w++) {
                        reg_t s_offset = (outerloop_idx * outerloop_stride + n * p_spad_stride[0] + c * p_spad_stride[1] + h * p_spad_stride[2] + w * p_spad_stride[3]) * element_size;
                        reg_t s_addr = scratchpadAddr + s_offset + vlane_idx * P.VU.vu_sram_byte;

                        if (element_size == 1) {
                            if (padding_flag || *p_split_dim >= vlane_stride)
                                continue;
                            uint8_t val = MMU.load_uint8(s_addr);
                            static_cast<uint8_t*>(dma_tensor)[outerloop_idx * outerloop_factor + vlane_idx * vlane_stride * factor + n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w] = val;
                        } else if (element_size == 2) {
                            if (padding_flag || *p_split_dim >= vlane_stride)
                                continue;
                            uint16_t val = MMU.load_uint16(s_addr);
                            static_cast<uint16_t*>(dma_tensor)[outerloop_idx * outerloop_factor + vlane_idx * vlane_stride * factor + n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w] = val;
                        } else if (element_size == 4) {
                            if (padding_flag || *p_split_dim >= vlane_stride)
                                continue;
                            uint32_t val = MMU.load_uint32(s_addr);
                            static_cast<uint32_t*>(dma_tensor)[outerloop_idx * outerloop_factor + vlane_idx * vlane_stride * factor + n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w] = val;
                            if (debug_flag) {
                                printf("spad_addr: 0x%lx, value: %f\n", s_addr, *(float *)&(val));
                            }
                        } else if (element_size == 8) {
                            if (padding_flag || *p_split_dim >= vlane_stride)
                                continue;
                            uint64_t val = MMU.load_uint64(s_addr);
                            static_cast<uint64_t*>(dma_tensor)[outerloop_idx * outerloop_factor + vlane_idx * vlane_stride * factor + n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w] = val;
                        }
                    }
                }
            }
        }
    }
}

// print dma_tensor
if (debug_flag) {
    printf("[[ MVOUT TENSOR ]]\n");
    for (int i=0; i<p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]; i++) {
        printf("%f ", *(float *)&(static_cast<uint32_t*>(dma_tensor)[i]));
    }
    printf("\n");
}

// Store data to memory by mm_stride
for (n=0; n<p_dim_size[0]; n++) {
    for (c=0; c<p_dim_size[1]; c++) {
        for (h=0; h<p_dim_size[2]; h++) {
            for (w=0; w<p_dim_size[3]; w++) {
                reg_t d_offset = (n * p_mm_stride[0] + c * p_mm_stride[1] + h * p_mm_stride[2] + w * p_mm_stride[3]) * element_size;
                reg_t d_addr = dramAddr + d_offset;

                if (element_size == 1) {
                    uint8_t val = static_cast<uint8_t*>(dma_tensor)[n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w];
                    MMU.store_uint8(d_addr, val);
                } else if (element_size == 2) {
                    uint16_t val = static_cast<uint16_t*>(dma_tensor)[n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w];
                    MMU.store_uint16(d_addr, val);
                } else if (element_size == 4) {
                    uint32_t val = static_cast<uint32_t*>(dma_tensor)[n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w];
                    MMU.store_uint32(d_addr, val);
                    if (debug_flag) {
                        printf("dram_address: 0x%lx, value: %f\n", d_addr, *(float *)&(val));
                    }
                } else if (element_size == 8) {
                    uint64_t val = static_cast<uint64_t*>(dma_tensor)[n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w];
                    MMU.store_uint64(d_addr, val);
                }
            }
        }
    }
}


delete [] dma_tensor;
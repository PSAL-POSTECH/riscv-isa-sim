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

// Load data from scratchpad by spad_stride
reg_t used_vlane = (p_dim_size[vlane_split_axis] + vlane_stride - 1) / vlane_stride;
reg_t block_shape[4] = {p_dim_size[0], p_dim_size[1], p_dim_size[2], p_dim_size[3]};
block_shape[vlane_split_axis] = vlane_stride;

if (debug_flag) {
    printf("- Block_shape: (%ld, %ld, %ld, %ld)\n", block_shape[0], block_shape[1], block_shape[2], block_shape[3]);
}

typedef struct {
    uint32_t dim;
    reg_t stride;
} Stride_info;
Stride_info spad_stride[4] = {{N, p_spad_stride[0]}, {C, p_spad_stride[1]}, {H, p_spad_stride[2]}, {W, p_spad_stride[3]}};
uint32_t dim_seq[4] = {N, C, H, W};

uint32_t n_dim=0;
// Sort by spad_stride
for (int i=0; i<4; i++) {
    if (spad_stride[i].stride == 0)
        continue;
    if (n_dim==0)
        n_dim = i;
    for (int j=i+1; j<4; j++) {
        if (spad_stride[i].stride < spad_stride[j].stride) {
            Stride_info temp = spad_stride[i];
            spad_stride[i] = spad_stride[j];
            spad_stride[j] = temp;

            // change dim seq
            uint32_t temp_dim = dim_seq[i];
            dim_seq[i] = dim_seq[j];
            dim_seq[j] = temp_dim;
        }
    }
}

for (int i=dim_seq[vlane_split_axis]-1; i>=n_dim; i--) {
    spad_stride[i].stride /= (p_spad_stride[dim_seq[vlane_split_axis]]/vlane_stride);
}

if (debug_flag) {
    printf("dim_seq: (%d, %d, %d, %d)\n", dim_seq[0], dim_seq[1], dim_seq[2], dim_seq[3]);
    printf("spad_stride: (%d, %d, %d, %d)\n", spad_stride[0].stride, spad_stride[1].stride, spad_stride[2].stride, spad_stride[3].stride);
}

reg_t *p_split_dim;
reg_t factor, outerloop_factor, outerloop_stride;
if (vlane_split_axis == N) {
    p_split_dim = &n;
    factor = p_dim_size[1] * p_dim_size[2] * p_dim_size[3];
    outerloop_stride = spad_stride[dim_seq[N]].stride * vlane_stride;
    outerloop_factor = block_shape[0] * block_shape[1] * block_shape[2] * n_vu * vlane_stride;
} else if (vlane_split_axis == C) {
    p_split_dim = &c;
    factor = p_dim_size[2] * p_dim_size[3] * vlane_stride;
    outerloop_stride = spad_stride[dim_seq[C]].stride * vlane_stride;
    outerloop_factor = block_shape[0] * block_shape[1] * n_vu * vlane_stride;
} else if (vlane_split_axis == H) {
    p_split_dim = &h;
    factor = p_dim_size[3];
    outerloop_stride = spad_stride[dim_seq[H]].stride * vlane_stride;
    outerloop_factor = block_shape[0] * n_vu * vlane_stride;
} else if (vlane_split_axis == W) {
    p_split_dim = &w;
    factor = 1;
    outerloop_stride = spad_stride[dim_seq[W]].stride * vlane_stride;
    outerloop_factor = n_vu * vlane_stride;
} else
    assert(0);

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
                        reg_t s_offset = (outerloop_idx * outerloop_stride + n * spad_stride[dim_seq[N]].stride + c * spad_stride[dim_seq[C]].stride + h * spad_stride[dim_seq[H]].stride + w * spad_stride[dim_seq[W]].stride) * element_size;
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
                    // if (debug_flag) {
                    //     printf("dram_address: 0x%lx, value: %f\n", d_addr, *(float *)&(val));
                    // }
                } else if (element_size == 8) {
                    uint64_t val = static_cast<uint64_t*>(dma_tensor)[n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w];
                    MMU.store_uint64(d_addr, val);
                }
            }
        }
    }
}


delete [] dma_tensor;
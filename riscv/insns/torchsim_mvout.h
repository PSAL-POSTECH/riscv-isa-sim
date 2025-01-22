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

void *dma_buffer = nullptr;
if (element_size == 1)
    dma_buffer = new uint8_t[p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]];
else if (element_size == 2)
    dma_buffer = new uint16_t[p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]];
else if (element_size == 4)
    dma_buffer = new uint32_t[p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]];
else if (element_size == 8)
    dma_buffer = new uint64_t[p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]];
else
    assert(0);

reg_t n, c, h, w;

// Load data from scratchpad by spad_stride
reg_t used_vlane = (p_dim_size[vlane_split_axis] + vlane_stride - 1) / vlane_stride / n_outerloop;
reg_t subtensor_dim[4] = {p_dim_size[0], p_dim_size[1], p_dim_size[2], p_dim_size[3]};
subtensor_dim[vlane_split_axis] = vlane_stride * n_outerloop;

void *subtensor_buffer = nullptr;
if (element_size == 1)
    subtensor_buffer = new uint8_t[subtensor_dim[N] * subtensor_dim[C] * subtensor_dim[H] * subtensor_dim[W]];
else if (element_size == 2)
    subtensor_buffer = new uint16_t[subtensor_dim[N] * subtensor_dim[C] * subtensor_dim[H] * subtensor_dim[W]];
else if (element_size == 4)
    subtensor_buffer = new uint32_t[subtensor_dim[N] * subtensor_dim[C] * subtensor_dim[H] * subtensor_dim[W]];
else if (element_size == 8)
    subtensor_buffer = new uint64_t[subtensor_dim[N] * subtensor_dim[C] * subtensor_dim[H] * subtensor_dim[W]];
else
    assert(0);

// Reorder spad stride
typedef struct {
    uint32_t dim;
    reg_t stride;
} Stride_info;
Stride_info reorder_stride[4] = {{N, p_spad_stride[0]}, {C, p_spad_stride[1]}, {H, p_spad_stride[2]}, {W, p_spad_stride[3]}};

// Sort by spad_stride
for (int i=0; i<4; i++) {
    if (reorder_stride[i].stride == 0)
        continue;
    for (int j=i+1; j<4; j++) {
        if (reorder_stride[i].stride < reorder_stride[j].stride) {
            Stride_info temp = reorder_stride[i];
            reorder_stride[i] = reorder_stride[j];
            reorder_stride[j] = temp;
        }
    }
}

if (debug_flag) {
    printf("Reorder spad stride:\n");
    for (int i=0; i<4; i++) {
        printf("dim: %d, stride: %ld\n", reorder_stride[i].dim, reorder_stride[i].stride);
    }
}

// Calculate subtensor stride
reg_t divide_factor = 1;
for (int i=H; i>0; i--) {
    if (reorder_stride[i].stride == 0)
        break;
    if (reorder_stride[i+1].dim == vlane_split_axis)
        divide_factor = (reorder_stride[i].stride / reorder_stride[i+1].stride) / (vlane_stride * n_outerloop);
    reorder_stride[i].stride /= divide_factor;
}
reg_t subtensor_stride[4];
for (int i=0; i<4; i++) {
    subtensor_stride[reorder_stride[i].dim] = reorder_stride[i].stride;
}

if (debug_flag) {
    printf("used_vlane: %ld\n", used_vlane);
    printf("Subtensor dim: (%ld, %ld, %ld, %ld)\n", subtensor_dim[0], subtensor_dim[1], subtensor_dim[2], subtensor_dim[3]);
    printf("Subtensor stride: (%ld, %ld, %ld, %ld)\n", subtensor_stride[0], subtensor_stride[1], subtensor_stride[2], subtensor_stride[3]);
}

reg_t *p_split_dim;
reg_t vlane_index_stride, outerloop_stride;
if (vlane_split_axis == N) {
    p_split_dim = &n;
    vlane_index_stride = p_dim_size[C] * p_dim_size[H] * p_dim_size[W] * vlane_stride;
    outerloop_stride = subtensor_dim[C] * subtensor_dim[H] * subtensor_dim[W] * vlane_stride * (n_vu - 1);
} else if (vlane_split_axis == C) {
    p_split_dim = &c;
    vlane_index_stride = p_dim_size[H] * p_dim_size[W] * vlane_stride;
    outerloop_stride = subtensor_dim[H] * subtensor_dim[W] * vlane_stride * (n_vu - 1);
} else if (vlane_split_axis == H) {
    p_split_dim = &h;
    vlane_index_stride = p_dim_size[W] * vlane_stride;
    outerloop_stride = subtensor_dim[W] * vlane_stride * (n_vu - 1);
} else if (vlane_split_axis == W) {
    p_split_dim = &w;
    vlane_index_stride = vlane_stride;
    outerloop_stride = vlane_stride * (n_vu - 1);
} else
    assert(0);

for (int vlane_idx=0; vlane_idx<used_vlane; vlane_idx++) {
    if (debug_flag) {
        printf("<<< Vlane: %d >>>\n", vlane_idx);
    }
    // Load subtensor buffer from scratchpad
    for (n=0; n<subtensor_dim[0]; n++) {
        for (c=0; c<subtensor_dim[1]; c++) {
            for (h=0; h<subtensor_dim[2]; h++) {
                for (w=0; w<subtensor_dim[3]; w++) {
                    reg_t s_offset = (n * subtensor_stride[0] + c * subtensor_stride[1] + h * subtensor_stride[2] + w * subtensor_stride[3]) * element_size;
                    reg_t s_addr = scratchpadAddr + s_offset + vlane_idx * P.VU.vu_sram_byte;
                    if (element_size == 1) {
                        uint8_t val = MMU.load_uint8(s_addr);
                        static_cast<uint8_t*>(subtensor_buffer)[n * subtensor_dim[1] * subtensor_dim[2] * subtensor_dim[3] + c * subtensor_dim[2] * subtensor_dim[3] + h * subtensor_dim[3] + w] = val;
                    } else if (element_size == 2) {
                        uint16_t val = MMU.load_uint16(s_addr);
                        static_cast<uint16_t*>(subtensor_buffer)[n * subtensor_dim[1] * subtensor_dim[2] * subtensor_dim[3] + c * subtensor_dim[2] * subtensor_dim[3] + h * subtensor_dim[3] + w] = val;
                    } else if (element_size == 4) {
                        uint32_t val = MMU.load_uint32(s_addr);
                        static_cast<uint32_t*>(subtensor_buffer)[n * subtensor_dim[1] * subtensor_dim[2] * subtensor_dim[3] + c * subtensor_dim[2] * subtensor_dim[3] + h * subtensor_dim[3] + w] = val;
                        if (debug_flag) {
                            printf("s_offset/element_size: %ld, s_addr: 0x%lx, val: %f\n", s_offset/element_size, s_addr, *(float *)&val);
                        }
                    } else if (element_size == 8) {
                        uint64_t val = MMU.load_uint64(s_addr);
                        static_cast<uint64_t*>(subtensor_buffer)[n * subtensor_dim[1] * subtensor_dim[2] * subtensor_dim[3] + c * subtensor_dim[2] * subtensor_dim[3] + h * subtensor_dim[3] + w] = val;
                    }
                }
            }
        }
    }

    // Print subtensor buffer
    if (debug_flag) {
        printf("[[ SUBTENSOR %d ]]\n", vlane_idx);
        for (int i=0; i<subtensor_dim[0] * subtensor_dim[1] * subtensor_dim[2] * subtensor_dim[3]; i++) {
            printf("%f ", *(float *)&(static_cast<uint32_t*>(subtensor_buffer)[i]));
        }
        printf("\n");
    }

    // Store subtensor buffer to buffer
    for (n=0; n<subtensor_dim[0]; n++) {
        for (c=0; c<subtensor_dim[1]; c++) {
            for (h=0; h<subtensor_dim[2]; h++) {
                for (w=0; w<subtensor_dim[3]; w++) {
                    reg_t index = vlane_index_stride * vlane_idx + n * subtensor_dim[C] * subtensor_dim[H] * subtensor_dim[W] + c * subtensor_dim[H] * subtensor_dim[W] + h * subtensor_dim[W] + w;
                    if (*p_split_dim >= vlane_stride)
                        index += outerloop_stride * (*p_split_dim / vlane_stride);

                    if (element_size == 1)
                        static_cast<uint8_t*>(dma_buffer)[index] = static_cast<uint8_t*>(subtensor_buffer)[n * subtensor_dim[1] * subtensor_dim[2] * subtensor_dim[3] + c * subtensor_dim[2] * subtensor_dim[3] + h * subtensor_dim[3] + w];
                    else if (element_size == 2)
                        static_cast<uint16_t*>(dma_buffer)[index] = static_cast<uint16_t*>(subtensor_buffer)[n * subtensor_dim[1] * subtensor_dim[2] * subtensor_dim[3] + c * subtensor_dim[2] * subtensor_dim[3] + h * subtensor_dim[3] + w];
                    else if (element_size == 4)
                        static_cast<uint32_t*>(dma_buffer)[index] = static_cast<uint32_t*>(subtensor_buffer)[n * subtensor_dim[1] * subtensor_dim[2] * subtensor_dim[3] + c * subtensor_dim[2] * subtensor_dim[3] + h * subtensor_dim[3] + w];
                    else if (element_size == 8)
                        static_cast<uint64_t*>(dma_buffer)[index] = static_cast<uint64_t*>(subtensor_buffer)[n * subtensor_dim[1] * subtensor_dim[2] * subtensor_dim[3] + c * subtensor_dim[2] * subtensor_dim[3] + h * subtensor_dim[3] + w];
                }
            }
        }
    }
}

// print dma_buffer
if (debug_flag) {
    printf("[[ MVOUT TENSOR ]]\n");
    for (int i=0; i<p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]; i++) {
        printf("%f ", *(float *)&(static_cast<uint32_t*>(dma_buffer)[i]));
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
                    uint8_t val = static_cast<uint8_t*>(dma_buffer)[n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w];
                    MMU.store_uint8(d_addr, val);
                } else if (element_size == 2) {
                    uint16_t val = static_cast<uint16_t*>(dma_buffer)[n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w];
                    MMU.store_uint16(d_addr, val);
                } else if (element_size == 4) {
                    uint32_t val = static_cast<uint32_t*>(dma_buffer)[n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w];
                    MMU.store_uint32(d_addr, val);
                    // if (debug_flag) {
                    //     printf("dram_address: 0x%lx, value: %f\n", d_addr, *(float *)&(val));
                    // }
                } else if (element_size == 8) {
                    uint64_t val = static_cast<uint64_t*>(dma_buffer)[n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w];
                    MMU.store_uint64(d_addr, val);
                }
            }
        }
    }
}

delete [] dma_buffer;
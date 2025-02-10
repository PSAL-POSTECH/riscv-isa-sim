// Custom instruction mvin
// mvin rs1, rs2
// rs1 = virtual main memory address
// rs2: scratchpad address

#define N 0
#define C 1
#define H 2
#define W 3

#define ROUNDUP(X, Y) (((X) + (Y) - 1) / (Y) * (Y))

const char* debug_env = std::getenv("SPIKE_DEBUG");
const int debug_flag = debug_env ? std::stoi(debug_env) : 0;

const uint64_t n_vu = P.VU.get_vu_num();

const reg_t dramAddr = RS1;
const reg_t scratchpadAddr = RS2;
const reg_t *p_dim_size = P.VU.dma_dim_size;
const reg_t *p_mm_stride = P.VU.dma_mm_stride;
const reg_t *p_spad_stride = P.VU.dma_spad_stride;
const reg_t element_size = P.VU.dma_element_size;
const reg_t vlane_stride = P.VU.dma_vlane_stride;
const int vlane_split_axis = P.VU.dma_vlane_split_axis;
uint64_t n_outerloop = 1;

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

uint64_t used_vlane = n_outerloop > 1 ? n_vu : (p_dim_size[vlane_split_axis] + vlane_stride - 1) / vlane_stride;

uint64_t block_dim[4] = {p_dim_size[N], p_dim_size[C], p_dim_size[H], p_dim_size[W]};
block_dim[vlane_split_axis] = vlane_stride;

uint64_t block_stride[4] = {p_spad_stride[N], p_spad_stride[C], p_spad_stride[H], p_spad_stride[W]};
for (int i=0; i<4; i++) {
    if (block_stride[i] > p_spad_stride[vlane_split_axis])
        block_stride[i] = (block_stride[i] / p_dim_size[vlane_split_axis]) * vlane_stride * n_outerloop;
}

uint64_t buffer_size = ROUNDUP(p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3], used_vlane * block_dim[N] * block_dim[C] * block_dim[H] * block_dim[W]);
uint64_t dma_buffer_stride[4] = {p_dim_size[C] * p_dim_size[H] * p_dim_size[W], p_dim_size[H] * p_dim_size[W], p_dim_size[W], 1};

uint64_t d_vlane_idx_stride = dma_buffer_stride[vlane_split_axis] * vlane_stride;
uint64_t d_outerloop_idx_stride = d_vlane_idx_stride * used_vlane;
uint64_t s_outerloop_idx_stride = block_stride[vlane_split_axis] * vlane_stride;

void *dma_buffer = nullptr;
try {
    if (element_size == 1)
        dma_buffer = new uint8_t[buffer_size];
    else if (element_size == 2)
        dma_buffer = new uint16_t[buffer_size];
    else if (element_size == 4)
        dma_buffer = new uint32_t[buffer_size];
    else if (element_size == 8)
        dma_buffer = new uint64_t[buffer_size];
    else
        assert(false && "Unsupported element size");
} catch (const std::bad_alloc& e) {
    std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    assert(false);
}

if (debug_flag) {
    printf("=============== MVOUT ===============\n");
    printf("Instruction configs:\n");
    printf("- dramAddr: 0x%lx\n", dramAddr);
    printf("- scratchpadAddr: 0x%lx\n", scratchpadAddr);
    printf("- p_dim_size: (%ld, %ld, %ld, %ld)\n", p_dim_size[0], p_dim_size[1], p_dim_size[2], p_dim_size[3]);
    printf("- p_mm_stride: (%ld, %ld, %ld, %ld)\n", p_mm_stride[0], p_mm_stride[1], p_mm_stride[2], p_mm_stride[3]);
    printf("- p_spad_stride: (%ld, %ld, %ld, %ld)\n", p_spad_stride[0], p_spad_stride[1], p_spad_stride[2], p_spad_stride[3]);
    printf("- element_size: %ld\n", element_size);
    printf("- vlane_stride: %ld\n", vlane_stride);
    printf("- vlane_split_axis: %d\n", vlane_split_axis);
    printf("- Outer loop: %ld\n", n_outerloop);

    printf("\nDMA buffer configs:\n");
    printf("- dma buffer size: %ld\n", buffer_size);
    printf("- dma buffer stride: (%ld, %ld, %ld, %ld)\n", dma_buffer_stride[0], dma_buffer_stride[1], dma_buffer_stride[2], dma_buffer_stride[3]);

    printf("\nBlock configs:\n");
    printf("- block_dim: (%ld, %ld, %ld, %ld)\n", block_dim[N], block_dim[C], block_dim[H], block_dim[W]);
    printf("- block_stride: (%ld, %ld, %ld, %ld)\n", block_stride[N], block_stride[C], block_stride[H], block_stride[W]);
    printf("- used_vlane: %ld\n", used_vlane);
    printf("- d_vlane_idx_stride: %ld\n", d_vlane_idx_stride);
    printf("- d_outerloop_idx_stride: %ld\n", d_outerloop_idx_stride);
    printf("- s_outerloop_idx_stride: %ld\n", s_outerloop_idx_stride);
}

assert(element_size > 0);
assert(vlane_stride > 0);

for (uint64_t outerloop_idx=0; outerloop_idx<n_outerloop; outerloop_idx++) {
    for (uint64_t vlane_idx=0; vlane_idx<used_vlane; vlane_idx++) {
        for (uint64_t n=0; n<block_dim[N]; n++) {
            for (uint64_t c=0; c<block_dim[C]; c++) {
                for (uint64_t h=0; h<block_dim[H]; h++) {
                    for (uint64_t w=0; w<block_dim[W]; w++) {
                        uint64_t d_offset = d_outerloop_idx_stride * outerloop_idx + d_vlane_idx_stride * vlane_idx + dma_buffer_stride[N] * n + dma_buffer_stride[C] * c + dma_buffer_stride[H] * h + dma_buffer_stride[W] * w ;
                        uint64_t s_offset = (s_outerloop_idx_stride * outerloop_idx + block_stride[N] * n + block_stride[C] * c + block_stride[H] * h + block_stride[W] * w) * element_size;
                        uint64_t s_addr = scratchpadAddr + s_offset + vlane_idx * P.VU.vu_sram_byte;

                        if (element_size == 1) {
                            uint8_t val = MMU.load_uint8(s_addr);
                            static_cast<uint8_t*>(dma_buffer)[d_offset] = val;
                        } else if (element_size == 2) {
                            uint16_t val = MMU.load_uint16(s_addr);
                            static_cast<uint16_t*>(dma_buffer)[d_offset] = val;
                        } else if (element_size == 4) {
                            uint32_t val = MMU.load_uint32(s_addr);
                            static_cast<uint32_t*>(dma_buffer)[d_offset] = val;
                        } else if (element_size == 8) {
                            uint64_t val = MMU.load_uint64(s_addr);
                            static_cast<uint64_t*>(dma_buffer)[d_offset] = val;
                        }
                    }
                }
            }
        }
    }
}

// print dma_buffer
if (debug_flag) {
    printf("[[ MVOUT TENSOR ]]\n");
    for (uint64_t i=0; i<p_dim_size[0] * p_dim_size[1] * p_dim_size[2] * p_dim_size[3]; i++) {
        printf("%f ", *(float *)&(static_cast<uint32_t*>(dma_buffer)[i]));
    }
    printf("\n");
}

// Store data to memory by mm_stride
for (uint64_t n=0; n<p_dim_size[0]; n++) {
    for (uint64_t c=0; c<p_dim_size[1]; c++) {
        for (uint64_t h=0; h<p_dim_size[2]; h++) {
            for (uint64_t w=0; w<p_dim_size[3]; w++) {
                uint64_t d_offset = (n * p_mm_stride[0] + c * p_mm_stride[1] + h * p_mm_stride[2] + w * p_mm_stride[3]) * element_size;
                uint64_t d_addr = dramAddr + d_offset;
                uint64_t buffer_idx = n * p_dim_size[1] * p_dim_size[2] * p_dim_size[3] + c * p_dim_size[2] * p_dim_size[3] + h * p_dim_size[3] + w;

                if (element_size == 1) {
                    uint8_t val = static_cast<uint8_t*>(dma_buffer)[buffer_idx];
                    MMU.store_uint8(d_addr, val);
                    if (debug_flag) {
                        printf("[Buffer -> DRAM] DRAM_ADDR: 0x%lx, Buffer_idx: %ld, Val: %x\n", d_addr, buffer_idx, *((char*)&val));
                    }
                } else if (element_size == 2) {
                    uint16_t val = static_cast<uint16_t*>(dma_buffer)[buffer_idx];
                    MMU.store_uint16(d_addr, val);
                    if (debug_flag) {
                        printf("[Buffer -> DRAM] DRAM_ADDR: 0x%lx, Buffer_idx: %ld, Val: %x\n", d_addr, buffer_idx, *((short*)&val));
                    }
                } else if (element_size == 4) {
                    uint32_t val = static_cast<uint32_t*>(dma_buffer)[buffer_idx];
                    MMU.store_uint32(d_addr, val);
                    if (debug_flag) {
                        printf("[Buffer -> DRAM] DRAM_ADDR: 0x%lx, Buffer_idx: %ld, Val: %f\n", d_addr, buffer_idx, *((float*)&val));
                    }
                } else if (element_size == 8) {
                    uint64_t val = static_cast<uint64_t*>(dma_buffer)[buffer_idx];
                    MMU.store_uint64(d_addr, val);
                    if (debug_flag) {
                        printf("[Buffer -> DRAM] DRAM_ADDR: 0x%lx, Buffer_idx: %ld, Val: %lf\n", d_addr, buffer_idx, *((double*)&val));
                    }
                }
            }
        }
    }
}

if (dma_buffer != nullptr) {
    if (element_size == 1)
        delete [] static_cast<uint8_t*>(dma_buffer);
    else if (element_size == 2)
        delete [] static_cast<uint16_t*>(dma_buffer);
    else if (element_size == 4)
        delete [] static_cast<uint32_t*>(dma_buffer);
    else if (element_size == 8)
        delete [] static_cast<uint64_t*>(dma_buffer);

    dma_buffer = nullptr;
}
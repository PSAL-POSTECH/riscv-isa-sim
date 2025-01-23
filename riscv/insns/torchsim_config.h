const char* debug_env = std::getenv("SPIKE_DEBUG");
const int debug_flag = debug_env ? std::stoi(debug_env) : 0;

/*
// RS1
rs1[63:48]: 1st dim size
rs1[47:32]: 2nd dim size
rs1[31:16]: 3rd dim size
rs1[15:0]: 4th dim size

// RS2
rs2[63:32]: vlane_stride
rs2[31:19]: not assigned
rs2[18:17]: mvin setting to set (0 ~ 3)
- CONFIG_MVIN 0x0
- CONFIG_MVIN2 0x1
- CONFIG_MVIN3 0x2
- CONFIG_MVOUT 0x3
rs2[16]: not assigned
rs2[15:14]:  vlane_split_axis
- 0: 1st dim, 1: 2nd dim, 2: 3rd dim, 3: 4th dim
rs2[13:0]: element size
*/

enum {
  MVIN = 0,
  MVIN1,  // 1
  MVIN2,  // 2
  MVOUT,  // 3
} config_t;

const int config_type = (RS2 >> 17) & 0b11; // 2 bits

// RS1
P.VU.dma_dim_size[0] = RS1 >> 48;
P.VU.dma_dim_size[1] = (RS1 >> 32) & 0xFFFF;
P.VU.dma_dim_size[2] = (RS1 >> 16) & 0xFFFF;
P.VU.dma_dim_size[3] = RS1 & 0xFFFF;

// RS2
P.VU.dma_element_size = RS2 & ((1ULL << 14) - 1);
P.VU.dma_vlane_split_axis = (RS2 >> 14) & 0b11;
P.VU.dma_vlane_stride = RS2 >> 32;

if (debug_flag) {
  printf("======== CONFIG =========\n");
  printf("RS1: %lx\n", RS1);
  printf("RS2: %lx\n", RS2);

  printf("dim_size = (%ld, %ld, %ld, %ld)\n", P.VU.dma_dim_size[0], P.VU.dma_dim_size[1], P.VU.dma_dim_size[2], P.VU.dma_dim_size[3]);
  printf("element_size = %ld\n", P.VU.dma_element_size);
  printf("vlane_split_axis = %d\n", P.VU.dma_vlane_split_axis);
  printf("vlane_stride = %ld\n", P.VU.dma_vlane_stride);
}
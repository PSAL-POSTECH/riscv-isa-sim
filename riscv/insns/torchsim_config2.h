const char* debug_env = std::getenv("SPIKE_DEBUG");
const int debug_flag = debug_env ? std::stoi(debug_env) : 0;

/*
// RS1
rs1[63:32]: 1st mm stride
rs1[31:0] 2nd mm stride

// RS2
rs2[63:32]: 3rd mm stride
rs2[31:0] 4th mm stride
*/

// RS1
P.VU.dma_mm_stride[0] = RS1 >> 32;
P.VU.dma_mm_stride[1] = RS1 & 0xFFFFFFFF;

// RS2
P.VU.dma_mm_stride[2] = RS2 >> 32;
P.VU.dma_mm_stride[3] = RS2 & 0xFFFFFFFF;

if (debug_flag) {
  printf("======== CONFIG2 =========\n");
  printf("RS1: %lx\n", RS1);
  printf("RS2: %lx\n", RS2);
  printf("mm_stride = (%ld, %ld, %ld, %ld)\n", P.VU.dma_mm_stride[0], P.VU.dma_mm_stride[1], P.VU.dma_mm_stride[2], P.VU.dma_mm_stride[3]);
}
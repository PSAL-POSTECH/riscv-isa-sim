const char* debug_env = std::getenv("SPIKE_DEBUG");
const int debug_flag = debug_env ? std::stoi(debug_env) : 0;

/*
rs1: scratchpad address of index data
rs2[16:0]: indirect index stride 
*/

P.VU.dma_indirect_addr= RS1;
P.VU.dma_indirect_stride = RS2 & 0xFFFF;
P.VU.dma_indirect_element_size = (RS2 >> 16) & 0xFF;

if (debug_flag) {
  printf("======== CONFIG4 =========\n");
  printf("RS1: %lx\n", RS1);
  printf("RS2: %lx\n", RS2);
  printf("Indirect config: (Addr: 0x%lx, stride: %ld, element_size: %ld)\n", P.VU.dma_indirect_addr, P.VU.dma_indirect_stride, P.VU.dma_indirect_element_size);
}
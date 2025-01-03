reg_t vs = insn.rs2();
const reg_t vl = P.VU.vl->read();
const reg_t n_vu = P.VU.get_vu_num();
const reg_t vstart = P.VU.vstart->read();
const char* debug_env = std::getenv("SPIKE_DEBUG");
const int debug_flag = debug_env ? std::stoi(debug_env) : 0;

for (reg_t vu_idx=0; vu_idx<n_vu; vu_idx++) {
    P.VU.vstart->write(vstart);
    if (debug_flag) {
        printf("[VPUSH_I] lane[%ld]", vu_idx);
    }
    for (reg_t i = 0; i < vl; ++i) {
        VI_STRIP(i);
        P.VU.vstart->write(i);
        float val = P.VU.elt<float>(vs, vreg_inx, vu_idx, true);
        P.SA->i_serializer_vpush(vu_idx, val);
        if (debug_flag) {
            printf("%f ", val);
        }
    }
    if (debug_flag) {
        printf("\n");
    }
}
P.VU.vstart->write(0);
P.SA->n_input += vl;
P.SA->compute();
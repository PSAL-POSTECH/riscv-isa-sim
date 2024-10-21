reg_t vd = insn.rd();
const reg_t vl = P.VU.vl->read();
const uint32_t n_vu = P.VU.get_vu_num();
const reg_t vstart = P.VU.vstart->read();
const char* debug_env = std::getenv("SPIKE_DEBUG");
const int debug_flag = debug_env ? std::stoi(debug_env) : 0;

assert(P.SA->n_output >= vl);
for (reg_t vu_idx=0; vu_idx<n_vu; vu_idx++) {
    P.VU.vstart->write(vstart);
    if (debug_flag) {
        printf("[VPOP] lane[%d]", vu_idx);
    }
    for (reg_t i = 0; i < vl; ++i) {
        if (P.SA->deserializer[vu_idx]->empty())
            break;

        VI_STRIP(i);
        P.VU.vstart->write(i);
        float val = P.SA->deserializer_pop(vu_idx);
        P.VU.elt<float>(vd, vreg_inx, vu_idx, true) = val;
        if (debug_flag) {
            printf("%f ", val);
        }
    }
    if (debug_flag) {
        printf("\n");
    }
}
P.VU.vstart->write(0);
P.SA->n_output -= vl;
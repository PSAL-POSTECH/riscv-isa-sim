reg_t vs1 = insn.rs1();
reg_t vs2 = insn.rs2();

const uint32_t sa_dim = P.SA.get_sa_dim();

float m_a[sa_dim][sa_dim];
float m_b[sa_dim][sa_dim];
float out[sa_dim][sa_dim];

// pop from input, weight serializers
for (int depth=0; depth<static_cast<int>(sa_dim); depth++) {
    for (int dim_idx=0; dim_idx<static_cast<int>(sa_dim); dim_idx++) {
        m_a[depth][dim_idx] = P.SA.input_vpop(dim_idx);
        m_b[depth][dim_idx] = P.SA.weight_vpop(dim_idx);
    }
}

// matmul
for (int m=0; m<static_cast<int>(sa_dim); m++) {
    for (int n=0; n<static_cast<int>(sa_dim); n++) {
        float tmp=0;
        for (int k=0; k<static_cast<int>(sa_dim); k++)
            tmp += m_a[m][k] * m_b[k][n];
        out[m][n] = tmp;
    }
}

// push to deserializer
for (int m=0; m<static_cast<int>(sa_dim); m++) {
    for (int n=0; n<static_cast<int>(sa_dim); n++) {
        P.SA.output_push(n, out[m][n]);
    }
}
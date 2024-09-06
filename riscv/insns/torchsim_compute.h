const uint32_t sa_dim = P.SA.get_sa_dim();

uint32_t m_a[sa_dim][sa_dim];
uint32_t m_b[sa_dim][sa_dim];
uint32_t out[sa_dim][sa_dim];

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
        for (int k=0; k<sa_dim; k++)
            tmp += *(float*)(char*)&m_a[m][k] * *(float*)(char*)&m_b[k][n];
        out[m][n] = *(uint32_t*)(char*)&tmp;
    }
}

// push to deserializer
for (int m=0; m<static_cast<int>(sa_dim); m++) {
    for (int n=0; n<static_cast<int>(sa_dim); n++) {
        P.SA.output_push(n, out[m][n]);
    }
}
const uint32_t sa_dim = P.SA.get_sa_dim();

float m_a[sa_dim][sa_dim];
float m_b[sa_dim][sa_dim];
float out[sa_dim][sa_dim];

// pop from input, weight serializers
for (reg_t depth=0; depth<sa_dim; depth++) {
    for (reg_t dim_idx=0; dim_idx<sa_dim; dim_idx++) {
        m_a[depth][dim_idx] = P.SA.input_vpop(dim_idx);
        m_b[depth][dim_idx] = P.SA.weight_vpop(dim_idx);
    }
}

// matmul
for (reg_t m=0; m<sa_dim; m++) {
    for (reg_t n=0; n<sa_dim; n++) {
        float tmp=0;
        for (reg_t k=0; k<sa_dim; k++)
            tmp += m_a[m][k] * m_b[k][n];
        out[m][n] = tmp;
    }
}

// push to deserializer
for (reg_t m=0; m<sa_dim; m++) {
    for (reg_t n=0; n<sa_dim; n++) {
        P.SA.output_push(n, out[m][n]);
    }
}
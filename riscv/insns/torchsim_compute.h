const uint32_t sa_dim = P.SA.get_sa_dim();

static float** m_a = nullptr;
static float** m_b = nullptr;
static float** out = nullptr;

// Allocate memory only if it's not already allocated
if (m_a == nullptr) {
    m_a = new float*[sa_dim];
    m_b = new float*[sa_dim];
    out = new float*[sa_dim];

    for (size_t i = 0; i < sa_dim; ++i) {
        m_a[i] = new float[sa_dim];
        m_b[i] = new float[sa_dim];
        out[i] = new float[sa_dim];
    }
}

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
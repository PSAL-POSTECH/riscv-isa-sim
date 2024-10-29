#include "systolic_array.h"

void systolicArray_t::reset() {
  if (i_serializer) {
    for (int dim_idx=0; dim_idx<static_cast<int>(sa_dim); dim_idx++)
      delete i_serializer[dim_idx];
    delete[] i_serializer;
  }
  if (w_serializer) {
    for (int dim_idx=0; dim_idx<static_cast<int>(sa_dim); dim_idx++)
      delete w_serializer[dim_idx];
    delete[] w_serializer;
  }
  if (deserializer) {
    for (int dim_idx=0; dim_idx<static_cast<int>(sa_dim); dim_idx++)
      delete deserializer[dim_idx];
    delete[] deserializer;
  }
  i_serializer = new std::queue<float>*[sa_dim];
  w_serializer = new std::queue<float>*[sa_dim];
  deserializer = new std::queue<float>*[sa_dim];

  for (int dim_idx=0; dim_idx<static_cast<int>(sa_dim); dim_idx++)
    i_serializer[dim_idx] = new std::queue<float>();
  for (int dim_idx=0; dim_idx<static_cast<int>(sa_dim); dim_idx++)
    w_serializer[dim_idx] = new std::queue<float>();
  for (int dim_idx=0; dim_idx<static_cast<int>(sa_dim); dim_idx++)
    deserializer[dim_idx] = new std::queue<float>();
}

void systolicArray_t::prefill_weight() {
  bool debug_flag = get_env_flag("SPIKE_DEBUG");

  if (weight == nullptr) {
    printf("weight allocation\n");
    weight = new std::vector<std::deque<float>*>(sa_dim);
    for (int i=0; i<sa_dim; i++)
      weight->at(i) = new std::deque<float>();
  }

  for (reg_t vl_idx=0; vl_idx<sa_dim; vl_idx++) {
    for (reg_t vreg_idx=0; vreg_idx<n_weight; vreg_idx++) {
      if (weight->at(vl_idx)->size() == sa_dim)
        weight->at(vl_idx)->pop_front();
      weight->at(vl_idx)->push_back(w_serializer_vpop(vl_idx));
    }
  }
  n_weight_push += n_weight;
  n_weight = 0;
}

void systolicArray_t::compute() {
  assert(!weight->at(0)->empty());
  bool debug_flag = get_env_flag("SPIKE_DEBUG");
  bool sparse_flag = get_env_flag("SPIKE_SPARSE");

  if (debug_flag){
    printf("======= COMPUTE =======\n");
    printf("-------- Weight --------\n");
    for (int i=0; i<sa_dim; i++) {
      for (auto iter : *(weight->at(i)))
        printf("%9f ", iter);
      printf("\n");
    }
  }

  if (sparse_flag)
    nz = 0;
    outerloop_count = (total_input_pushed - 1) / sa_dim;
    if (compute_cycle->size() <= outerloop_count)
      compute_cycle->push_back(0);

  for (int i=0; i<n_input; i++) {
    float input_vector[sa_dim];
    float output_vector[sa_dim];

    for (int j=0; j<sa_dim; j++) {
      input_vector[j] = i_serializer_vpop(j);
      output_vector[j] = 0;

      if (sparse_flag && input_vector[j] != 0)
        nz++;
    }

    if (debug_flag) {
      printf("-------- Input --------\n");
      for (int j=0; j<sa_dim; j++)
        printf("%9f ", input_vector[j]);
      printf("\n");
    }

    for (int j=0; j<sa_dim; j++) {
      int k = 0;
      for (auto iter : *(weight->at(j))) {
        output_vector[j] += input_vector[k] * iter;
        k++;
      }
      deserializer_push(j, output_vector[j]);
    }

    if (debug_flag) {
      printf("-------- Output --------\n");
      for (int j=0; j<sa_dim; j++)
        printf("%9f ", output_vector[j]);
      printf("\n\n");
    }
  }

  if (sparse_flag) {
    d = sa_dim * 3;
    if (n_weight_push > 0) {
      reg_t prefill_cycle = sa_dim * 2 - 1; // Always weight pushed with padding?
      compute_cycle->at(outerloop_count) += prefill_cycle;

      printf("Weight Pushed: %d\n", n_weight_push);
      printf("Prefill cycle: %ld\n", prefill_cycle);
    }

    reg_t cycle = d * (1 - alpha * (1 - nz / (sa_dim * n_input)));
    // reg_t vpop_cycle = n_input;
    compute_cycle->at(outerloop_count) += cycle;

    printf("nz: %d\n", nz);
    printf("d: %d\n", d);
    printf("alpha: %f\n", alpha);
    printf("compute cycle: %ld\n", compute_cycle->at(outerloop_count));
  }

  n_output += n_input;
  n_input = 0;
  n_weight_push = 0;
}
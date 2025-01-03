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
    weight = new std::vector<std::deque<float>*>(sa_dim);
    for (uint32_t i=0; i<sa_dim; i++)
      weight->at(i) = new std::deque<float>();
  }

  for (uint32_t vl_idx=0; vl_idx<sa_dim; vl_idx++) {
    for (reg_t vreg_idx=0; vreg_idx<n_weight; vreg_idx++) {
      if (weight->at(vl_idx)->size() == sa_dim)
        weight->at(vl_idx)->pop_front();
      weight->at(vl_idx)->push_back(w_serializer_vpop(vl_idx));
    }
  }
  n_weight = 0;
}

void systolicArray_t::compute() {
  assert(!weight->at(0)->empty());
  bool debug_flag = get_env_flag("SPIKE_DEBUG");

  if (debug_flag){
    printf("======= COMPUTE =======\n");
    printf("-------- Weight --------\n");
    for (uint32_t i=0; i<sa_dim; i++) {
      for (auto iter : *(weight->at(i)))
        printf("%9f ", iter);
      printf("\n");
    }
  }

  for (reg_t i=0; i<n_input; i++) {
    float input_vector[sa_dim];
    float output_vector[sa_dim];

    for (uint32_t j=0; j<sa_dim; j++) {
      input_vector[j] = i_serializer_vpop(j);
      output_vector[j] = 0;
    }

    if (debug_flag) {
      printf("-------- Input --------\n");
      for (uint32_t j=0; j<sa_dim; j++)
        printf("%9f ", input_vector[j]);
      printf("\n");
    }

    for (uint32_t j=0; j<sa_dim; j++) {
      int k = 0;
      for (auto iter : *(weight->at(j))) {
        output_vector[j] += input_vector[k] * iter;
        k++;
      }
      deserializer_push(j, output_vector[j]);
    }

    if (debug_flag) {
      printf("-------- Output --------\n");
      for (uint32_t j=0; j<sa_dim; j++)
        printf("%9f ", output_vector[j]);
      printf("\n\n");
    }
  }
  n_output += n_input;
  n_input = 0;
}
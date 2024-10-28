#ifndef _RISCV_SYSTOLIC_ARRAY_H
#define _RISCV_SYSTOLIC_ARRAY_H

#include "processor.h"
#include <queue>
#include <deque>
#include <vector>

class processor_t;

class systolicArray_t
{
public:
  processor_t *p;
  uint32_t sa_dim;
  std::queue<float> **i_serializer;
  std::queue<float> **w_serializer;
  std::queue<float> **deserializer;

  float **input;
  float **output;
  std::vector<std::deque<float> *> *weight;

  reg_t n_input;
  reg_t n_weight;
  reg_t n_output;

  reg_t total_input_pushed;


  // Sparse Accelerator
  // d = Dense matrix multiplication cyclic time
  // nz = Non-zero elements in input matrix (0~1)
  // alpha = Amount of decrease in cycle time by zero elements
  // Cycle = d x (1 - alpha x (1 - nz))
  reg_t d;
  reg_t nz;
  float alpha = 0.5;
  reg_t n_weight_push;

  uint32_t outerloop_count;
  std::vector<reg_t> *compute_cycle;

public:
  void reset();
  void prefill_weight();
  void compute();

  systolicArray_t(processor_t *p, reg_t n_vu) : p(p),
                                                sa_dim(n_vu),
                                                i_serializer(0),
                                                w_serializer(0),
                                                deserializer(0),
                                                input(0),
                                                output(0),
                                                weight(0),
                                                n_input(0),
                                                n_weight(0),
                                                n_output(0),
                                                total_input_pushed(0),
                                                outerloop_count(0),
                                                compute_cycle(0)
  {
    if (get_env_flag("SPIKE_SPARSE"))
      compute_cycle = new std::vector<reg_t>();
  }

  ~systolicArray_t()
  {
    if (i_serializer)
    {
      for (int dim_idx = 0; dim_idx < static_cast<int>(sa_dim); dim_idx++)
        delete i_serializer[dim_idx];
      free(i_serializer);
    }
    if (w_serializer)
    {
      for (int dim_idx = 0; dim_idx < static_cast<int>(sa_dim); dim_idx++)
        delete w_serializer[dim_idx];
      free(w_serializer);
    }
    if (deserializer)
    {
      for (int dim_idx = 0; dim_idx < static_cast<int>(sa_dim); dim_idx++)
        delete deserializer[dim_idx];
      free(deserializer);
    }
    if (input)
    {
      for (int i = 0; i < static_cast<int>(sa_dim); i++)
        free(input[i]);
      free(input);
    }
    if (weight)
    {
      for (int i = 0; i < static_cast<int>(sa_dim); i++)
        free(weight->at(i));
      free(weight);
    }
    if (output)
    {
      for (int i = 0; i < static_cast<int>(sa_dim); i++)
        free(output[i]);
      free(output);
    }
  }

  void i_serializer_vpush(uint32_t dim_idx, float val)
  {
    i_serializer[dim_idx]->push(val);
  }

  void w_serializer_vpush(uint32_t dim_idx, float val)
  {
    w_serializer[dim_idx]->push(val);
  }

  void deserializer_push(uint32_t dim_idx, float val)
  {
    deserializer[dim_idx]->push(val);
  }

  float i_serializer_vpop(uint32_t dim_idx)
  {
    float val = i_serializer[dim_idx]->front();
    i_serializer[dim_idx]->pop();
    return val;
  }

  float w_serializer_vpop(uint32_t dim_idx)
  {
    float val = w_serializer[dim_idx]->front();
    w_serializer[dim_idx]->pop();
    return val;
  }

  float deserializer_pop(uint32_t dim_idx)
  {
    float val = deserializer[dim_idx]->front();
    deserializer[dim_idx]->pop();
    return val;
  }

  uint32_t get_sa_dim()
  {
    return sa_dim;
  }

  std::vector<reg_t>* get_compute_cycles() {
    return compute_cycle;
  }

  bool get_env_flag(const char* env)
  {
    const char* env_value = std::getenv(env);
    const bool debug_flag = env_value ? std::stoi(env_value) : 0;
    return debug_flag;
  }
};

#endif // _RISCV_SYSTOLIC_ARRAY_H
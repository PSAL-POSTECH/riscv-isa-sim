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
  // float **weight;
  float **output;
  // std::queue<float> **weight;
  std::vector<std::deque<float> *> *weight;

  reg_t n_input;
  reg_t n_weight;
  reg_t n_output;

public:
  void reset();
  void prefill_weight();
  void compute();

  systolicArray_t(processor_t *p, reg_t n_vu) : p(p),
                                                sa_dim(n_vu),
                                                i_serializer(0),
                                                w_serializer(0),
                                                deserializer(0),
                                                n_input(0),
                                                n_weight(0),
                                                n_output(0)
  {
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

  bool get_debug_flag()
  {
    const char* debug_env = std::getenv("SPIKE_DEBUG");
    const bool debug_flag = debug_env ? std::stoi(debug_env) : 0;
    return debug_flag;
  }
};

#endif // _RISCV_SYSTOLIC_ARRAY_H
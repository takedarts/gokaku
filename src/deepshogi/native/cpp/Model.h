#pragma once

#include <ATen/ATen.h>
#include <torch/script.h>
#include <torch/torch.h>

#include <cstdint>
#include <string>

namespace deepshogi {

class Model {
 public:
  /**
   * Create a model object.
   * If -1 is specified for the GPU number, create an object that computes using the CPU.
   * @param filename Model file
   * @param gpu GPU number
   * @param fp16 True to compute with 16-bit precision
   * @param deterministic True to make computation results reproducible
   */
  Model(std::string filename, int32_t gpu, bool fp16, bool deterministic);

  /**
   * Destructor.
   */
  virtual ~Model() = default;

  /**
   * Execute inference.
   * @param inputs Input data
   * @param outputs Output data
   * @param size Number of evaluation data
   */
  virtual void forward(float* inputs, float* outputs, uint32_t size);

  /**
   * Return True if using GPU.
   * @return 1 if using GPU, 0 otherwise
   */
  virtual int32_t isCuda();

 private:
  /**
   * Inference model.
   */
  torch::jit::script::Module _model;

  /**
   * Execution device.
   */
  at::Device _device;

  /**
   * Data type used for computation.
   */
  at::ScalarType _dtype;
};

}  // namespace deepshogi

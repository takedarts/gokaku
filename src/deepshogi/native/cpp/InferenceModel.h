#pragma once

#include <ATen/ATen.h>
#include <torch/script.h>
#include <torch/torch.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace deepshogi {

/**
 * A class that executes inference using a model.
 */
class InferenceModel {
 public:
  /**
   * Returns the list of available GPU indices.
   * @return List of GPU indices
   */
  static std::vector<std::int32_t> getAvailableGPUs();
  /**
   * Returns the device available in the current execution environment.
   * @param gpu GPU index
   */
  static at::Device getDevice(int32_t gpu);

  /**
   * Returns the scalar type available in the current execution environment.
   * @param gpu GPU index
   * @param fp16 True to compute with 16-bit precision
   */
  static at::ScalarType getScalarType(int32_t gpu, bool fp16);

  /**
   * Creates a model object.
   * Specifying -1 for the GPU index creates an object that computes on the CPU.
   * @param filename Path to the model file
   * @param gpu GPU index
   * @param fp16 True to compute with 16-bit precision
   * @param deterministic True to make computation results reproducible
   */
  InferenceModel(std::string filename, int32_t gpu, bool fp16, bool deterministic);

  /**
   * Destructor.
   */
  virtual ~InferenceModel() = default;

  /**
   * Runs inference.
   * @param inputs Input data
   * @param masks Output data mask
   * @param outputs Output data
   * @param size Number of evaluation data items
   */
  void forward(int32_t* inputs, int32_t* masks, float* outputs, int32_t size);

  /**
   * Returns True if using CUDA.
   * @return True if using CUDA
   */
  inline bool isCuda() const {
    return _device.is_cuda();
  }

  /**
   * Returns True if using CPU.
   * @return True if using CPU
   */
  inline bool isCpu() const {
    return _cpu;
  }

 private:
  /**
   * Mutex for synchronizing I/O operations.
   */
  std::mutex _ioMutex;

  /**
   * Mutex for synchronizing computation.
   */
  std::mutex _computeMutex;

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

  /**
   * Tensor for bit shifting.
   */
  torch::Tensor _bitShift;

  /**
   * True if computing on CPU.
   */
  bool _cpu;
};

}  // namespace deepshogi

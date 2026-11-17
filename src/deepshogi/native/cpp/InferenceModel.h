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
   * @return Execution device selected for the requested device index.
   */
  static at::Device getSuitableDevice(int32_t gpu);

  /**
   * Returns the scalar type available in the current execution environment.
   * @param gpu GPU index
   * @param fp16 True to compute with 16-bit precision
   * @return Scalar type selected for the requested precision and device.
   */
  static at::ScalarType getSuitableScalarType(int32_t gpu, bool fp16);

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
   * Run inference.
   * @param inputs Input data.
   * @param masks Output mask.
   * @param outputs Output data.
   * @param size Number of input samples.
   */
  void forward(int32_t* inputs, int32_t* masks, float* outputs, int32_t size);

  /**
   * Return the execution device determined at construction.
   * @return Execution device.
   */
  inline const at::Device& getDevice() const {
    return _device;
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

};

}  // namespace deepshogi

#pragma once

#include <ATen/ATen.h>
#include <torch/csrc/inductor/aoti_package/model_package_loader.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace deepshogi {

/**
 * A class that evaluates positions with an AOTI or Dynamo TensorRT PT2 package.
 */
class InferenceModel {
 public:
  /**
   * Returns the list of available CPU and GPU indices.
   * @return List containing -1 for the CPU and the available GPU indices
   */
  static std::vector<std::int32_t> getAvailableGPUs();

  /**
   * Resolve the execution device with the same rules as the former TorchScript implementation.
   * @param gpu Negative for the CPU, otherwise the GPU index
   * @return Specified execution device
   * @throws std::invalid_argument If the specified GPU is not available
   */
  static at::Device getSuitableDevice(int32_t gpu);

  /**
   * Returns the recommended precision for compatibility with the existing API.
   * Returns FP32 on the CPU, but the PT2 execution precision is determined from the metadata.
   * @param gpu Negative for the CPU, otherwise the GPU index
   * @param fp16 True to use FP16 on the GPU
   * @return Scalar type selected with the same rules as the former TorchScript implementation
   */
  static at::ScalarType getSuitableScalarType(int32_t gpu, bool fp16);

  /**
   * Returns a required metadata value, detecting missing entries and empty strings.
   * @param metadata Metadata obtained from the PT2 package
   * @param key Name of the required entry
   * @return Nonempty value of the specified entry
   * @throws std::invalid_argument If the specified entry is missing or empty
   */
  static std::string getMetadataValue(
      const std::unordered_map<std::string, std::string>& metadata, const std::string& key);

  /**
   * Validates the stored execution conditions and loads the PT2 package.
   * @param filename Path to the PT2 package created by compile.py
   * @param gpu Negative for the CPU, otherwise a GPU index on the same runtime platform
   * @param fp16 True if the model is FP16, false if it is FP32
   * @param deterministic True to use the same CuDNN reproducibility settings as before
   * @throws std::invalid_argument If execution conditions or metadata are invalid or mismatched
   * @throws c10::Error If the package or runtime libraries cannot be loaded
   */
  InferenceModel(std::string filename, int32_t gpu, bool fp16, bool deterministic);

  /**
   * Releases the loader and model resources.
   */
  virtual ~InferenceModel() = default;

  /**
   * Unpacks the packed inputs and runs inference, splitting and padding to the fixed batch.
   * @param inputs Array containing size rows of packed board inputs
   * @param masks Array containing size rows of packed output masks
   * @param outputs Array receiving size rows of FP32 outputs, with zeros outside the mask
   * @param size Number of evaluations, at least zero; zero returns without accessing the arrays
   * @throws std::runtime_error If the model returns no output
   */
  void forward(int32_t* inputs, int32_t* masks, float* outputs, int32_t size);

  /**
   * Return the execution device determined at construction.
   * @return Execution device used for the model and inputs.
   */
  inline const at::Device& getDevice() const {
    return _device;
  }

  /**
   * Returns the fixed batch size stored in the model.
   * @return Fixed batch size of at least one
   */
  inline int32_t getBatchSize() const {
    return _batchSize;
  }

 private:
  /**
   * Releases the PT2 loader in synchronization with the TensorRT logger.
   */
  struct LoaderDeleter {
    /**
     *  True if the loader uses the TensorRT runtime.
     */
    bool usesTensorRT = false;

    /**
     * Releases the loader without conflicting with logger setting changes.
     * @param loader PT2 loader to release
     */
    void operator()(torch::inductor::AOTIModelPackageLoader* loader) const;
  };

  /**
   * Mutex serializing input preparation through CPU output retrieval for the same model.
   */
  std::mutex _computeMutex;

  /**
   * PT2 loader shared by AOTI and TensorRT.
   */
  std::unique_ptr<torch::inductor::AOTIModelPackageLoader, LoaderDeleter> _model;

  /**
   * Device whose match between the metadata and the execution environment was verified.
   */
  at::Device _device;

  /**
   * Input precision stored in the metadata, including FP16 on the CPU.
   */
  at::ScalarType _dtype;

  /**
   * Fixed batch size of the inputs passed to the loader.
   */
  int32_t _batchSize;

  /**
   * Bit-shift tensor used to unpack packed inputs and output masks.
   */
  at::Tensor _bitShift;

  /**
   * Bit-shift tensor on the CPU used to unpack the masks for writing back to the output array.
   */
  at::Tensor _cpuBitShift;
};

}  // namespace deepshogi

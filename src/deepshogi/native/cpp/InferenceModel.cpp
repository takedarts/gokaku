#include "InferenceModel.h"

#include <torch/torch.h>

#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>

#ifdef USE_TORCH_TENSORRT
#include <torch_tensorrt/logging.h>

#include <optional>
#include <shared_mutex>
#endif

#include "Config.h"

namespace deepshogi {

#ifdef USE_TORCH_TENSORRT
namespace {

// Prevent conflicts between temporary log-level changes and inference or release that use
// the shared logger.
std::shared_mutex tensor_rt_mutex;

/**
 * Report only errors and above while loading a TensorRT model and restore the log level on exit.
 */
class ScopedTensorRTLoadLogging {
 public:
  /**
   * Save the original log level and suppress warnings if they are configured to be reported.
   */
  ScopedTensorRTLoadLogging()
      : _previousLevel(torch_tensorrt::logging::get_reportable_log_level()) {
    if (_previousLevel > torch_tensorrt::logging::Level::kERROR) {
      torch_tensorrt::logging::set_reportable_log_level(torch_tensorrt::logging::Level::kERROR);
    }
  }

  /**
   * Restore the temporarily changed log level regardless of whether loading succeeded.
   */
  ~ScopedTensorRTLoadLogging() {
    if (_previousLevel > torch_tensorrt::logging::Level::kERROR) {
      torch_tensorrt::logging::set_reportable_log_level(_previousLevel);
    }
  }

  // Limit the responsibility for restoring the log level to the scope that created the guard.
  ScopedTensorRTLoadLogging(const ScopedTensorRTLoadLogging&) = delete;
  ScopedTensorRTLoadLogging& operator=(const ScopedTensorRTLoadLogging&) = delete;

 private:
  // Shared logger setting immediately before loading.
  const torch_tensorrt::logging::Level _previousLevel;
};

}  // namespace
#endif

/**
 * Returns the list of available CPU and GPU indices.
 * @return List containing -1 for the CPU and the available GPU indices
 */
std::vector<std::int32_t> InferenceModel::getAvailableGPUs() {
  // As in the existing class, always include the CPU and add CUDA/ROCm or MPS GPU indices.
  std::vector<std::int32_t> device_ids{-1};

  if (torch::cuda::is_available()) {
    for (int32_t i = 0; i < torch::cuda::device_count(); i++) {
      device_ids.push_back(i);
    }
  } else if (torch::mps::is_available()) {
    device_ids.push_back(0);
  }

  return device_ids;
}

/**
 * Resolve the execution device with the same rules as the former TorchScript implementation.
 * @param gpu Negative for the CPU, otherwise the GPU index
 * @return Specified execution device
 * @throws std::invalid_argument If the specified GPU is not available
 */
at::Device InferenceModel::getSuitableDevice(int32_t gpu) {
  // Keep public API compatibility and resolve negative values as the CPU.
  if (gpu < 0) {
    return at::Device(at::kCPU);
  }

  // ROCm also uses the same device name and API as CUDA.
  if (torch::cuda::is_available() && gpu < torch::cuda::device_count()) {
    return at::Device(at::kCUDA, gpu);
  }

  // Specify the MPS index explicitly so that it can be compared with output tensor devices.
  if (torch::mps::is_available() && gpu == 0) {
    return at::Device(at::kMPS, 0);
  }

  throw std::invalid_argument("Specified GPU device is not available.");
}

/**
 * Returns the recommended precision for compatibility with the existing API.
 * Returns FP32 on the CPU, but the PT2 execution precision is determined from the metadata.
 * @param gpu Negative for the CPU, otherwise the GPU index
 * @param fp16 True to use FP16 on the GPU
 * @return Scalar type selected with the same rules as the former TorchScript implementation
 */
at::ScalarType InferenceModel::getSuitableScalarType(int32_t gpu, bool fp16) {
  // This compatibility helper returns the same values as before and does not decide PT2 inputs.
  if (fp16 && ((torch::cuda::is_available() && gpu >= 0) ||
               (torch::mps::is_available() && gpu == 0))) {
    return at::kHalf;
  }

  return at::kFloat;
}

/**
 * Returns a required metadata value, detecting missing entries and empty strings.
 * @param metadata Metadata obtained from the PT2 package
 * @param key Name of the required entry
 * @return Nonempty value of the specified entry
 * @throws std::invalid_argument If the specified entry is missing or empty
 */
std::string InferenceModel::getMetadataValue(
    const std::unordered_map<std::string, std::string>& metadata, const std::string& key) {
  // Reject models with unknown compile conditions instead of filling in missing defaults.
  const auto item = metadata.find(key);

  if (item == metadata.end() || item->second.empty()) {
    throw std::invalid_argument("Missing PT2 metadata: " + key);
  }

  return item->second;
}

/**
 * Validates the stored execution conditions and loads the PT2 package.
 * @param filename Path to the PT2 package created by compile.py
 * @param gpu Negative for the CPU, otherwise a GPU index on the same runtime platform
 * @param fp16 True if the model is FP16, false if it is FP32
 * @param deterministic True to use the same CuDNN reproducibility settings as before
 * @throws std::invalid_argument If execution conditions or metadata are invalid or mismatched
 * @throws c10::Error If the package or runtime libraries cannot be loaded
 */
InferenceModel::InferenceModel(
    std::string filename, int32_t gpu, bool fp16, bool deterministic)
    : _computeMutex(),
      _model(nullptr, LoaderDeleter{}),
      _device(getSuitableDevice(gpu)),
      _dtype(fp16 ? at::kHalf : at::kFloat),
      _batchSize(0),
      _bitShift() {
  // Protect the shared area used by JSON loading in PyTorch 2.11 and the shared CuDNN settings.
  static std::mutex initialization_mutex;
  std::lock_guard<std::mutex> initialization_lock(initialization_mutex);
  c10::InferenceMode inference_guard;
  torch::DeviceGuard device_guard(_device);

  // Read the stored conditions before loading the model and check the format and version.
  using Loader = torch::inductor::AOTIModelPackageLoader;
  const auto metadata = Loader::load_metadata_from_package(filename, "model");
  const auto backend = getMetadataValue(metadata, "backend");

  if (getMetadataValue(metadata, "schema_version") != "1" ||
      (backend != "aoti" && backend != "tensorrt")) {
    throw std::invalid_argument("Unsupported PT2 metadata schema or backend.");
  }

  // The CPU/GPU type is fixed in the model, and it is not moved after loading.
  const auto device_type = getMetadataValue(metadata, "device_type");

  if (device_type != c10::DeviceTypeName(_device.type(), true) ||
      getMetadataValue(metadata, "AOTI_DEVICE_KEY") != device_type) {
    throw std::invalid_argument("PT2 device type does not match the requested device.");
  }

  // The fp16 argument is not a request to change precision; it is checked against the model.
  if (getMetadataValue(metadata, "precision") != (fp16 ? "fp16" : "fp32") ||
      getMetadataValue(metadata, "input_dtype") != (fp16 ? "float16" : "float32")) {
    throw std::invalid_argument("PT2 precision does not match the requested precision.");
  }

  // Distinguish NVIDIA CUDA and AMD ROCm, which share the same cuda name.
  std::string gpu_runtime;

  if (_device.is_cuda()) {
    gpu_runtime = at::hasHIP() ? "rocm" : "cuda";
  } else if (_device.is_mps()) {
    gpu_runtime = "metal";
  }

  if ((!gpu_runtime.empty() && getMetadataValue(metadata, "gpu_runtime") != gpu_runtime) ||
      (gpu_runtime.empty() && metadata.count("gpu_runtime") != 0) ||
      (backend == "tensorrt" && gpu_runtime != "cuda")) {
    throw std::invalid_argument("PT2 GPU runtime does not match the execution environment.");
  }

  // Parse the fixed batch strictly as an integer and limit it to the existing buffer range.
  const auto batch_text = getMetadataValue(metadata, "batch_size");
  const auto parsed = std::from_chars(
      batch_text.data(), batch_text.data() + batch_text.size(), _batchSize);
  const int32_t buffer_width =
      std::max({MODEL_INPUT_PACK_SIZE, MODEL_OUTPUT_PACK_SIZE, MODEL_OUTPUT_SIZE});

  if (parsed.ec != std::errc() || parsed.ptr != batch_text.data() + batch_text.size() ||
      _batchSize < 1 || _batchSize > std::numeric_limits<int32_t>::max() / buffer_width) {
    throw std::invalid_argument("Invalid PT2 fixed batch size.");
  }

  // Do not rewrite CuDNN settings applied before the game when loading a model during inference.
  if (_device.is_cuda()) {
    auto& context = torch::globalContext();
    if (!context.userEnabledCuDNN()) {
      context.setUserEnabledCuDNN(true);
    }

    const bool benchmark = !deterministic;
    if (context.benchmarkCuDNN() != benchmark) {
      context.setBenchmarkCuDNN(benchmark);
    }

    if (context.deterministicCuDNN() != deterministic) {
      context.setDeterministicCuDNN(deterministic);
    }
  }

  // Create the loader in the normal synchronized mode and place constants and run state on the
  // specified GPU.
  const c10::DeviceIndex device_index = _device.is_cuda() ? _device.index() : -1;

  {
#ifdef USE_TORCH_TENSORRT
    // Serialize TensorRT loading and restore the log level before releasing the lock.
    _model.get_deleter().usesTensorRT = backend == "tensorrt";
    std::unique_lock<std::shared_mutex> runtime_lock(tensor_rt_mutex, std::defer_lock);
    std::optional<ScopedTensorRTLoadLogging> logging_guard;

    if (_model.get_deleter().usesTensorRT) {
      runtime_lock.lock();
      logging_guard.emplace();
    }
#endif

    _model.reset(new Loader(filename, "model", false, 1, device_index));
  }

  // After restoring the log settings, create the tensor used to unpack board inputs.
  _bitShift = torch::arange(0, 32, torch::TensorOptions().dtype(torch::kInt32)).to(_device);
}

/**
 * Unpacks the packed inputs and runs inference, splitting and padding to the fixed batch.
 * @param inputs Array containing size rows of packed board inputs
 * @param masks Array containing size rows of packed output masks
 * @param outputs Array receiving size rows of FP32 outputs, with zeros outside the mask
 * @param size Number of evaluations; zero returns without accessing the arrays
 * @throws std::invalid_argument If size is negative or a nonempty array is null
 * @throws std::runtime_error If the model output does not match the evaluation format
 */
void InferenceModel::forward(int32_t* inputs, int32_t* masks, float* outputs, int32_t size) {
  // Do not use pointers for empty inputs, and reject negative sizes or null nonempty buffers.
  if (size < 0 || (size > 0 && (inputs == nullptr || masks == nullptr || outputs == nullptr))) {
    throw std::invalid_argument("Invalid inference size or null buffer.");
  }

  if (size == 0) {
    return;
  }

  // Do not use the same loader concurrently, and protect it until GPU outputs reach the CPU.
  std::lock_guard<std::mutex> compute_lock(_computeMutex);

#ifdef USE_TORCH_TENSORRT
  // Allow inferences to run in parallel and wait for log-setting changes by other model loads.
  std::shared_lock<std::shared_mutex> runtime_lock(tensor_rt_mutex, std::defer_lock);

  if (_model.get_deleter().usesTensorRT) {
    runtime_lock.lock();
  }
#endif

  c10::InferenceMode inference_guard;
  torch::DeviceGuard device_guard(_device);
  const auto packed_options = torch::TensorOptions().dtype(torch::kInt32).device(torch::kCPU);

  // Split when size exceeds the fixed batch, using a wide integer type for pointer arithmetic.
  for (int64_t offset = 0; offset < size; offset += _batchSize) {
    const int64_t count = std::min<int64_t>(_batchSize, static_cast<int64_t>(size) - offset);
    const auto input_values = torch::from_blob(
        inputs + offset * MODEL_INPUT_PACK_SIZE, {count, MODEL_INPUT_PACK_SIZE}, packed_options);
    auto in_values = torch::empty({_batchSize, MODEL_INPUT_PACK_SIZE}, packed_options);

    in_values.narrow(0, 0, count).copy_(input_values);

    // Fill missing rows with the first actual input row so the model never sees uninitialized
    // boards.
    if (count < _batchSize) {
      in_values.narrow(0, count, _batchSize - count)
          .copy_(input_values.narrow(0, 0, 1).expand({_batchSize - count, MODEL_INPUT_PACK_SIZE}));
    }

    // Treat all outputs of padded rows as invalid and copy the masks without changing the
    // caller's array.
    const auto mask_values = torch::from_blob(
        masks + offset * MODEL_OUTPUT_PACK_SIZE, {count, MODEL_OUTPUT_PACK_SIZE}, packed_options);
    auto out_masks = torch::zeros({_batchSize, MODEL_OUTPUT_PACK_SIZE}, packed_options);

    out_masks.narrow(0, 0, count).copy_(mask_values);
    in_values = in_values.to(_device);
    out_masks = out_masks.to(_device);

    // Unpack and slice bits as before, then convert to the model's input precision.
    auto unpacked = torch::bitwise_right_shift(
        in_values.narrow(1, 0, MODEL_INPUT_PACK_SIZE - 3).unsqueeze(2), _bitShift);

    unpacked = torch::bitwise_and(unpacked, 1).reshape({_batchSize, -1});

    // With batch 1, contiguous() alone does not change the leading stride, so allocate anew.
    auto in_data = torch::empty(
        {_batchSize, MODEL_INPUT_SIZE}, torch::TensorOptions().dtype(_dtype).device(_device));

    in_data.copy_(unpacked.narrow(1, 0, MODEL_INPUT_SIZE));

    // Normalize the last five elements in FP32, convert them to the input precision, and store
    // them at the same positions as before.
    auto info_values = in_values.narrow(1, MODEL_INPUT_PACK_SIZE - 5, 5);

    info_values = (info_values.to(torch::kFloat32) / 0xfffff).to(_dtype);
    in_data.slice(1, MODEL_INFO_OFFSET + 77, MODEL_INFO_OFFSET + 82).copy_(info_values);

    // Unpack the output masks in the same order as before and limit them to the valid width.
    out_masks = torch::bitwise_right_shift(out_masks.unsqueeze(2), _bitShift);
    out_masks = torch::bitwise_and(out_masks, 1).reshape({_batchSize, -1});
    out_masks = out_masks.narrow(1, 0, MODEL_OUTPUT_SIZE).to(torch::kBool);

    // Run AOTI and TensorRT through the common API, matching PT2's fixed shape and stride.
    const auto result = _model->run({in_data});

    if (result.size() != 1 || !result[0].defined() || result[0].dim() != 2 ||
        result[0].size(0) != _batchSize || result[0].size(1) != MODEL_OUTPUT_SIZE ||
        !result[0].is_floating_point() || result[0].device() != _device) {
      throw std::runtime_error(
          "Expected one PT2 output of shape (batch_size, MODEL_OUTPUT_SIZE).");
    }

    // Take only the actual rows, then apply mask compression and transfer to the CPU as before.
    auto out_data = result[0].narrow(0, 0, count).reshape({-1});
    out_masks = out_masks.narrow(0, 0, count).reshape({-1});
    out_data = out_data.masked_select(out_masks).to(torch::kCPU);
    out_masks = out_masks.to(torch::kCPU);

    // Zero values outside the mask and expand the FP32 outputs into the caller's array.
    auto out_values = torch::from_blob(
        outputs + offset * MODEL_OUTPUT_SIZE, {count * MODEL_OUTPUT_SIZE},
        torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCPU));

    out_values.fill_(0.0f);
    out_values.masked_scatter_(out_masks, out_data.to(torch::kFloat32));
  }
}

/**
 * Releases the loader without conflicting with logger setting changes.
 * @param loader PT2 loader to release
 */
void InferenceModel::LoaderDeleter::operator()(
    torch::inductor::AOTIModelPackageLoader* loader) const {
#ifdef USE_TORCH_TENSORRT
  // Keep the original log settings while TensorRT release processing outputs logs.
  std::shared_lock<std::shared_mutex> runtime_lock(tensor_rt_mutex, std::defer_lock);

  if (usesTensorRT) {
    runtime_lock.lock();
  }
#endif

  delete loader;
}

}  // namespace deepshogi

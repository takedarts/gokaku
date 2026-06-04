#include "InferenceModel.h"

#ifdef USE_TORCH_TENSORRT
#include <torch_tensorrt/logging.h>
#endif

#include "Config.h"

namespace deepshogi {

/**
 * Determines whether the specified file is a TensorRT model file.
 * TensorRT model files contain specific marker strings, so the function
 * checks whether any of those markers are present in the file.
 * Always returns false when TensorRT is not supported.
 * @param filename File name
 * @return True if the file is a TensorRT model file
 */
static bool isTensorRTModelFile(const std::string& filename) {
#ifdef USE_TORCH_TENSORRT
  const std::vector<std::string> trt_markers = {
      "__torch__.torch.classes.tensorrt.Engine",
      "torch.classes.tensorrt.Engine",
      "ops.tensorrt.execute_engine",
      "tensorrt::execute_engine",
      "torch_tensorrt",
      "_trt_engine",
  };

  // Open the file in binary mode and read its contents as a string
  std::ifstream ifs(filename, std::ios::binary);

  if (!ifs) {
    throw std::runtime_error("failed to open model file: " + filename);
  }

  std::string data(
      (std::istreambuf_iterator<char>(ifs)),
      std::istreambuf_iterator<char>());

  // Check whether any of the marker strings are present in the file
  for (const auto& marker : trt_markers) {
    if (data.find(marker) != std::string::npos) {
      return true;
    }
  }

#endif

  return false;
}

/**
 * Returns the list of available GPU indices.
 * @return List of GPU indices
 */
std::vector<std::int32_t> InferenceModel::getAvailableGPUs() {
  // List of available GPU indices
  // By default, includes -1 which indicates CPU computation
  std::vector<std::int32_t> device_ids{-1};

  // Add all GPU indices if CUDA is available
  if (torch::cuda::is_available()) {
    int32_t gpu_count = torch::cuda::device_count();

    for (int32_t i = 0; i < gpu_count; ++i) {
      device_ids.push_back(i);
    }
  }
  // Add GPU index 0 if MPS is available
  else if (torch::mps::is_available()) {
    device_ids.push_back(0);
  }

  return device_ids;
}

/**
 * Returns the device available in the current execution environment.
 * @param gpu GPU index
 */
at::Device InferenceModel::getDevice(int32_t gpu) {
  // Use CPU if the GPU index is negative
  if (gpu < 0) {
    return at::Device(at::kCPU);
  }

  // Use CUDA if it is available
  if (torch::cuda::is_available() && gpu < torch::cuda::device_count()) {
    return at::Device(at::kCUDA, gpu);
  }

  // Use MPS if it is available
  if (torch::mps::is_available() && gpu == 0) {
    return at::Device(at::kMPS);
  }

  // Otherwise throw an exception
  throw std::runtime_error("Specified GPU device is not available.");
}

/**
 * Returns the scalar type available in the current execution environment.
 * @param gpu GPU index
 * @param fp16 True to compute with 16-bit precision
 * @return Scalar type
 */
at::ScalarType InferenceModel::getScalarType(int32_t gpu, bool fp16) {
  // Use Half precision when computing with CUDA and fp16 is enabled
  if (torch::cuda::is_available() && gpu >= 0 && fp16) {
    return at::kHalf;
  }
  // Use Half precision when computing with MPS and fp16 is enabled
  else if (torch::mps::is_available() && gpu == 0 && fp16) {
    return at::kHalf;
  }
  // Otherwise use Float32
  else {
    return at::kFloat;
  }
}

/**
 * Creates a model object.
 * Specifying -1 for the GPU index creates an object that computes on the CPU.
 * @param filename Path to the model file
 * @param gpu GPU index
 * @param fp16 True to compute with 16-bit precision
 * @param deterministic True to make computation results reproducible
 */
InferenceModel::InferenceModel(
    std::string filename, int32_t gpu, bool fp16, bool deterministic)
    : _ioMutex(),
      _computeMutex(),
      _device(InferenceModel::getDevice(gpu)),
      _dtype(InferenceModel::getScalarType(gpu, fp16)),
      _bitShift(),
      _cpu(gpu < 0) {
  // Configure CuDNN settings when using CUDA
  if (_device.is_cuda()) {
    torch::globalContext().setUserEnabledCuDNN(true);

    if (deterministic) {
      torch::globalContext().setBenchmarkCuDNN(false);
      torch::globalContext().setDeterministicCuDNN(true);
    } else {
      torch::globalContext().setBenchmarkCuDNN(true);
      torch::globalContext().setDeterministicCuDNN(false);
    }
  }

#ifdef USE_TORCH_TENSORRT
  // Set the TensorRT log level to error
  torch_tensorrt::logging::set_reportable_log_level(
      torch_tensorrt::logging::Level::kERROR);
#endif

  // Load the model and set the execution device and data type
  // The loading method differs between TensorRT model files and other model files,
  // so check the file contents and switch the loading method accordingly.
  // The `isTensorRTModelFile` function always returns false when TensorRT is not supported.
  if (isTensorRTModelFile(filename)) {
    _model = torch::jit::load(filename, _device);
    _model.eval();
  } else {
    _model = torch::jit::load(filename);
    _model.to(_device, _dtype);
    _model.eval();
  }

  // Create the bit-shift tensor and transfer it to the execution device
  _bitShift = torch::arange(0, 32, torch::TensorOptions().dtype(torch::kInt32));
  _bitShift = _bitShift.to(_device);
}

/**
 * Runs inference.
 * @param inputs Input data
 * @param masks Output data mask
 * @param outputs Output data
 * @param size Number of evaluation data items
 */
void InferenceModel::forward(int32_t* inputs, int32_t* masks, float* outputs, int32_t size) {
  c10::InferenceMode guard;
  torch::Tensor in_data;
  torch::Tensor out_data;

  // Convert input data and output mask to tensors
  torch::Tensor in_values = torch::from_blob(
      inputs, size * MODEL_INPUT_PACK_SIZE,
      torch::TensorOptions().dtype(torch::kInt32));
  in_values = in_values.reshape({size, MODEL_INPUT_PACK_SIZE});

  torch::Tensor out_masks = torch::from_blob(
      masks, size * MODEL_OUTPUT_PACK_SIZE,
      torch::TensorOptions().dtype(torch::kInt32));
  out_masks = out_masks.reshape({size, MODEL_OUTPUT_PACK_SIZE});

  // Transfer input data and output mask to the execution device
  {
    std::lock_guard<std::mutex> ioLock(_ioMutex);
    in_values = in_values.to(_device);
    out_masks = out_masks.to(_device);
  }

  // Run computation on the device
  {
    std::lock_guard<std::mutex> computeLock(_computeMutex);

    // Input values are stored in bit representation (except the last three values)
    // Bit-shift all values except the last three to convert them to 0 or 1
    in_data = torch::bitwise_right_shift(
        in_values.narrow(1, 0, MODEL_INPUT_PACK_SIZE - 3).unsqueeze(2), _bitShift);
    in_data = torch::bitwise_and(in_data, 1);
    in_data = in_data.reshape({size, -1});
    in_data = in_data.narrow(1, 0, MODEL_INPUT_SIZE);
    in_data = in_data.to(_dtype);

    // The last three values are stored scaled from the range [0, 1] to [0, 0xfffff]
    // Normalize the last three values back to the range [0, 1] and store them at the end of the input data
    in_values = in_values.narrow(1, MODEL_INPUT_PACK_SIZE - 3, 3);
    in_values = in_values.to(torch::kFloat32) / 0xfffff;
    in_values = in_values.to(_dtype);
    in_data.slice(1, MODEL_INPUT_SIZE - 3, MODEL_INPUT_SIZE).copy_(in_values);

    // Each output mask value is stored in bit representation
    // Bit-shift to convert them to 0 or 1
    out_masks = torch::bitwise_right_shift(
        out_masks.unsqueeze(2), _bitShift);
    out_masks = torch::bitwise_and(out_masks, 1);
    out_masks = out_masks.reshape({size, -1});
    out_masks = out_masks.narrow(1, 0, MODEL_OUTPUT_SIZE);
    out_masks = out_masks.to(torch::kBool);

    // Run the model and obtain the output data
    out_data = _model.forward({in_data}).toTensor();

    // Apply mask compression to the output data
    out_data = out_data.reshape({-1});
    out_masks = out_masks.reshape({-1});
    out_data = out_data.masked_select(out_masks);
  }

  // Transfer output data and output mask to CPU
  {
    std::lock_guard<std::mutex> ioLock(_ioMutex);
    out_data = out_data.to(torch::kCPU);
    out_masks = out_masks.to(torch::kCPU);
  }

  // Expand the output data into the output array using the mask
  torch::Tensor out_values = torch::from_blob(
      outputs, size * MODEL_OUTPUT_SIZE,
      torch::TensorOptions().dtype(torch::kFloat32));

  out_values.fill_(0.0f);
  out_values.masked_scatter_(out_masks, out_data.to(torch::kFloat32));
}

}  // namespace deepshogi

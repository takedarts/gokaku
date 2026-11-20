#include "InferenceProcessor.h"

#include <torch/torch.h>

#include <algorithm>
#include <exception>
#include <future>
#include <limits>
#include <stdexcept>

namespace deepshogi {

/**
 * Construct the inference processor.
 * @param model Model file.
 * @param gpus List of GPU IDs.
 * @param fp16 True to compute with 16-bit precision.
 * @param deterministic True to request reproducible results.
 * @param batchSize Batch size.
 * @param threadsPerGpu Number of threads per GPU.
 * @param cacheSize Inference result cache capacity.
 */
InferenceProcessor::InferenceProcessor(
    std::string model, std::vector<int32_t> gpus, bool fp16, bool deterministic,
    int32_t batchSize, int32_t threadsPerGpu, int32_t cacheSize)
    : _queueMutex(),
      _queueCondition(),
      _queue(),
      _cache(cacheSize),
      _executors(),
      _terminated(false),
      _threadSize(0),
      _batchSize(std::max(batchSize, 1)) {
  // Ensure the device list is not empty and the device count fits in int32_t.
  const int32_t max_value = std::numeric_limits<int32_t>::max();

  if (gpus.empty() || gpus.size() > static_cast<size_t>(max_value)) {
    throw std::invalid_argument("gpus must contain devices within int32 range.");
  }

  // Clamp the threads per device to at least one and compute the total in a wider type.
  const int32_t threads_per_gpu = std::max(threadsPerGpu, 1);
  const int64_t thread_size = static_cast<int64_t>(gpus.size()) * threads_per_gpu;

  // Ensure the total thread count can be converted to int32_t.
  if (thread_size > max_value) {
    throw std::invalid_argument("Inference thread count exceeds int32 range.");
  }

  // Keep the clamped and validated total thread count.
  _threadSize = static_cast<int32_t>(thread_size);

  // Hold the results of the initialization tasks run in parallel for each device.
  std::vector<std::future<std::unique_ptr<InferenceExecutor>>> tasks;

  // Reserve storage so that registering results after starting tasks does not reallocate.
  tasks.reserve(gpus.size());
  _executors.reserve(gpus.size());

  // Hold an exception from starting or running tasks and rethrow it after collecting all tasks.
  std::exception_ptr initialization_error;

  // Create the inference executor of each device in a separate asynchronous task.
  try {
    for (int32_t gpu : gpus) {
      tasks.emplace_back(std::async(
          std::launch::async,
          [this, model, gpu, fp16, deterministic, threads_per_gpu] {
            // Set the target device on the thread running the initialization task.
            torch::DeviceGuard device_guard(InferenceModel::getSuitableDevice(gpu));

            // Create the inference model and per-thread buffers and start inference workers.
            return std::make_unique<InferenceExecutor>(
                this, model, gpu, fp16, deterministic, _batchSize, threads_per_gpu);
          }));
    }
  } catch (...) {
    // Even if starting fails midway, collect all tasks that have already started.
    initialization_error = std::current_exception();
  }

  // Wait for all started tasks and register successful results in the specified device order.
  for (auto& task : tasks) {
    try {
      _executors.emplace_back(task.get());
    } catch (...) {
      // Prefer the exception held first and keep collecting the remaining tasks after a failure.
      if (!initialization_error) {
        initialization_error = std::current_exception();
      }
    }
  }

  // After collecting all tasks, stop workers and destroy them after releasing the lock.
  if (initialization_error) {
    // Set the termination flag while holding the lock that workers use to check the queue.
    {
      std::lock_guard<std::mutex> lock(_queueMutex);
      _terminated = true;
    }

    // Notify waiting workers of the updated termination flag.
    _queueCondition.notify_all();

    // Destroy the created inference executors and wait for their workers to finish.
    _executors.clear();

    // Rethrow the held exception to the caller after reclaiming the created resources.
    std::rethrow_exception(initialization_error);
  }
}

/**
 * Destroys the inference manager object.
 */
InferenceProcessor::~InferenceProcessor() {
  // Terminate inference processing
  {
    std::lock_guard<std::mutex> lock(_queueMutex);
    _terminated = true;
    _queueCondition.notify_all();
  }

  // Destroy inference executor objects
  _executors.clear();
}

/**
 * Schedules an inference execution.
 * The inference computation runs asynchronously, so this function returns immediately.
 * When the computation completes, the node's evaluation value is updated.
 * @param node Node to run inference on
 * @param callback Callback function to notify when inference completes
 */
void InferenceProcessor::submit(
    MctsNode* node, std::function<void(MctsNode*)> callback) {
  // Create the cache key for the requested position.
  InferenceHash inference_hash(&node->getBoard());

  // Prepare storage for a cached inference result.
  InferenceResult cached_result;

  // Use the cached inference result if available.
  if (_cache.get(inference_hash, cached_result)) {
    node->applyInferenceResult(
        cached_result.value, cached_result.remainingTurns, cached_result.policies);
    callback(node);
    return;
  }

  // Define the callback to run after inference.
  auto exec_callback =
      [this, node, callback, inference_hash](
          MctsNode*, const InferenceResult& result) {
        // Cache the inference result if it has not already been registered.
        _cache.put(inference_hash, result);

        // Apply the inference result to the node.
        node->applyInferenceResult(result.value, result.remainingTurns, result.policies);

        // Invoke the callback.
        callback(node);
      };

  // Add the node and callback function to the inference reservation queue
  {
    std::lock_guard<std::mutex> lock(_queueMutex);
    _queue.emplace(node, exec_callback);
    _queueCondition.notify_one();
  }
}

/**
 * Returns the evaluation value for the specified board.
 * @param board Board to evaluate
 * @return Evaluation value
 */
float InferenceProcessor::predict(Board* board) {
  // Create a temporary node.
  MctsManager manager(MctsParameter(0.0f, 1.0f, 18200.0f, 0.0f));
  MctsNode node(&manager);

  // Set Black 28 and White 27 as the nyugyoku scores for standalone inference.
  node.initialize(board->getSfen(), board->getDrawTurn(), 28, 27);

  // Create a mutex and condition variable to wait for inference to complete
  std::mutex mutex;
  std::condition_variable cv;
  bool finished = false;

  // Submit without holding the mutex so synchronous callbacks can acquire it.
  submit(&node, [&mutex, &cv, &finished](MctsNode*) {
    std::lock_guard<std::mutex> lock(mutex);
    finished = true;
    // Protect local objects through their last access, including after setting completion.
    cv.notify_one();
  });

  // Run inference and wait for the node's evaluation value to be updated
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&finished]() { return finished; });
  }

  return node.getNodeValue();
}

/**
 * Runs inference synchronously.
 * @param inputs Input data
 * @param masks Output data mask
 * @param outputs Output data
 * @param size Number of evaluation data items
 */
void InferenceProcessor::execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size) {
  _executors[0]->execute(inputs, masks, outputs, size);
}

}  // namespace deepshogi

#include "InferenceProcessor.h"

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
      _threadSize(static_cast<int32_t>(gpus.size()) * threadsPerGpu),
      _batchSize(batchSize) {
  for (int32_t gpu : gpus) {
    _executors.emplace_back(std::make_unique<InferenceExecutor>(
        this, model, gpu, fp16, deterministic, batchSize, threadsPerGpu));
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
  MctsManager manager(MctsParameter(28, 27, 512, 0.0f, 1.0f, 18200.0f, 0.0f));
  MctsNode node(&manager);

  // Assign the board to the node.
  node.initialize(board->getSfen());

  // Create a mutex and condition variable to wait for inference to complete
  std::mutex mutex;
  std::condition_variable cv;

  // Run inference and wait for the node's evaluation value to be updated
  {
    std::unique_lock<std::mutex> lock(mutex);
    submit(&node, [&cv](MctsNode*) { cv.notify_one(); });
    cv.wait(lock, [&node] { return node.isEvaluated(); });
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

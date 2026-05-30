#include "InferenceProcessor.h"

namespace deepshogi {

/**
 * Creates an inference manager object.
 * @param model Model file path
 * @param gpus List of GPU indices
 * @param fp16 True to compute with 16-bit precision
 * @param deterministic True to make computation results reproducible
 * @param batchSize Batch size
 * @param threadsPerGpu Number of threads per GPU
 * @param cacheSize Cache size for inference results
 */
InferenceProcessor::InferenceProcessor(
    std::string model, std::vector<int32_t> gpus, bool fp16, bool deterministic,
    int32_t batchSize, int32_t threadsPerGpu, int32_t cacheSize)
    : _cacheMutex(),
      _queueMutex(),
      _queueCondition(),
      _queue(),
      _executors(),
      _cacheSize(cacheSize),
      _cacheKeys(),
      _cacheResults(),
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
  // Variable used when a cached inference result is found
  InferenceResult cached_result;
  // Flag indicating whether a cached inference result was found
  bool cached_result_found = false;

  // Acquire the lock for synchronization
  {
    std::lock_guard<std::mutex> lock(_cacheMutex);

    // Check if a cached inference result exists
    // If found, save the cached result and set the flag
    // Applying to the node and invoking the callback are done outside the lock
    BoardHash boardHash(&node->getBoard());
    auto it = _cacheResults.find(boardHash);

    if (it != _cacheResults.end()) {
      cached_result = it->second;
      cached_result_found = true;
    }
  }

  // If a cached inference result was found, use it
  if (cached_result_found) {
    node->applyInferenceResult(cached_result.value, cached_result.policies);
    callback(node);
    return;
  }

  // Define the callback function invoked when inference completes
  auto exec_callback = [this, node, callback](MctsNode*, const InferenceResult& result) {
    // Acquire the lock for synchronization
    {
      std::lock_guard<std::mutex> lock(_cacheMutex);

      // Check if the result is already in the cache
      // If not, save it to the cache
      BoardHash boardHash(&node->getBoard());
      auto it = _cacheResults.find(boardHash);

      if (it == _cacheResults.end()) {
        _cacheResults[boardHash] = result;
        _cacheKeys.push(boardHash);
      }

      // Remove the oldest cache entry when the cache size is exceeded
      if (_cacheKeys.size() >= _cacheSize) {
        _cacheResults.erase(_cacheKeys.front());
        _cacheKeys.pop();
      }
    }

    // Apply the inference result to the node
    node->applyInferenceResult(result.value, result.policies);

    // Invoke the callback function
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
  // Create a dummy node object
  MctsManager manager(MctsParameter(28, 27, 512, 1.0f, 18200.0f));
  MctsNode node(&manager);

  // Set the board on the node object
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

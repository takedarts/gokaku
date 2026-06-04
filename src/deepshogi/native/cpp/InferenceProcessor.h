#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "Board.h"
#include "BoardHash.h"
#include "InferenceExecutor.h"
#include "InferenceResult.h"
#include "MctsNode.h"

namespace deepshogi {

/**
 * A class that manages inference execution for board evaluation.
 */
class InferenceProcessor {
 private:
  // Allow InferenceExecutor class to access the inference reservation queue,
  // so InferenceExecutor class is declared as a friend class.
  friend class InferenceExecutor;

 public:
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
  InferenceProcessor(
      std::string model, std::vector<int32_t> gpus, bool fp16, bool deterministic,
      int32_t batchSize, int32_t threadsPerGpu, int32_t cacheSize);

  /**
   * Destroys the inference manager object.
   */
  virtual ~InferenceProcessor();

  /**
   * Schedules an inference execution.
   * The inference computation runs asynchronously, so this function returns immediately.
   * When the computation completes, the node's evaluation value is updated.
   * @param node Node to run inference on
   * @param callback Callback function to notify when inference completes
   */
  void submit(MctsNode* node, std::function<void(MctsNode*)> callback);

  /**
   * Returns the evaluation value for the specified board.
   * @param board Board to evaluate
   * @return Evaluation value
   */
  float predict(Board* board);

  /**
   * Runs inference synchronously.
   * @param inputs Input data
   * @param masks Output data mask
   * @param outputs Output data
   * @param size Number of evaluation data items
   */
  void execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size);

  /**
   * Returns the batch size.
   * @return Batch size
   */
  inline int32_t getBatchSize() const {
    return _batchSize;
  }

  /**
   * Returns the number of threads used for inference.
   * @return Number of threads used for inference
   */
  inline int32_t getThreadSize() const {
    return _threadSize;
  }

  /**
   * Returns the batch fill rate for inference.
   * @return Batch fill rate for inference
   */
  inline float getBatchFillRate() const {
    float total_fill_rate = 0.0f;

    for (const auto& executor : _executors) {
      total_fill_rate += executor->getBatchFillRate();
    }

    return total_fill_rate / static_cast<float>(_executors.size());
  }

  /**
   * Returns the cache hit rate for inference.
   * @return Cache hit rate for inference
   */
  inline float getCacheHitRate() const {
    return _cacheHitRate.load(std::memory_order_relaxed);
  }

 private:
  /**
   * Mutex object for synchronizing cache access.
   */
  std::mutex _cacheMutex;

  /**
   * Mutex object for synchronizing inference execution reservation queue access.
   */
  std::mutex _queueMutex;

  /**
   * Condition variable for inference execution reservation queue.
   */
  std::condition_variable _queueCondition;

  /**
   * Queue for inference execution reservation.
   */
  std::queue<std::pair<MctsNode*, InferenceExecutorCallback>> _queue;

  /**
   * List of inference executor objects.
   */
  std::vector<std::unique_ptr<InferenceExecutor>> _executors;

  /**
   * Cache size for inference results.
   */
  int32_t _cacheSize;

  /**
   * Queue of cache keys for inference results.
   * Used to evict old cache entries when the cache size is exceeded.
   */
  std::queue<BoardHash> _cacheKeys;

  /**
   * Cache of inference results.
   * Key is the board hash value; value is the inference result.
   */
  std::map<BoardHash, InferenceResult> _cacheResults;

  /**
   * True to terminate.
   */
  bool _terminated;

  /**
   * Number of threads used for inference.
   */
  int32_t _threadSize;

  /**
   * Batch size.
   */
  int32_t _batchSize;

  /**
   * Cache hit rate for inference.
   */
  std::atomic<float> _cacheHitRate;
};

}  // namespace deepshogi

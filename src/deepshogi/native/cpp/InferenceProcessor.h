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
  virtual ~InferenceProcessor() = default;

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

 private:
  /**
   * Mutex object for synchronization.
   */
  std::mutex _mutex;

  /**
   * List of inference executor objects.
   */
  std::vector<std::unique_ptr<InferenceExecutor>> _executors;

  /**
   * Number of threads used for inference.
   */
  int32_t _threadSize;

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
   * List of GPU IDs to use.
   */
  std::vector<int32_t> _gpus;

  /**
   * Batch size.
   */
  int32_t _batchSize;
};

}  // namespace deepshogi

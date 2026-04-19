#pragma once

#include <cstdint>
#include <map>
#include <queue>
#include <shared_mutex>

#include "Board.h"
#include "Config.h"
#include "Evaluation.h"
#include "Processor.h"

namespace deepshogi {

/**
 * Class to store evaluation results.
 */
class Evaluator {
 public:
  /**
   * Create evaluation result object.
   * @param processor Object to execute inference
   * @param cacheSize Size of the cache for evaluation results
   */
  Evaluator(Processor* processor, int32_t cacheSize);

  /**
   * Execute evaluation by the model.
   * @param board Board to be evaluated
   * @return Evaluation result
   */
  Evaluation evaluate(const Board* board);

 private:
  /**
   * Mutex for synchronization.
   */
  std::shared_mutex _mutex;

  /**
   * Object to execute inference.
   */
  Processor* _processor;

  /**
   * Size of the cache for evaluation results.
   */
  int32_t _cacheSize;

  /**
   * Queue of keys for the cache.
   * Used to remove keys from the cache when the cache size exceeds the specified size.
   */
  std::queue<uint64_t> _cacheKeys;

  /**
   * Cache for evaluation results.
   * The key is the hash value of the board, and the value is the evaluation result.
   */
  std::map<uint64_t, Evaluation> _cache;

  /**
   * Execute evaluation by the model.
   * @param board Board to be evaluated
   * @return Evaluation result
   */
  Evaluation _evaluate(const Board* board);
};

}  // namespace deepshogi

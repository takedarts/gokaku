#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>

#include "PnSearchEngine.h"

namespace deepshogi {

/**
 * Class that manages PN search engine objects.
 */
class PnSearchManager {
 public:
  /**
   * Constructs a PnSearchManager object.
   * @param nodes Maximum number of nodes for the search
   * @param size Number of PN search engines to pool
   */
  PnSearchManager(int32_t nodes, int32_t size);

  /**
   * Destroys the PnSearchManager object.
   */
  virtual ~PnSearchManager();

  /**
   * Acquires a PN search engine object.
   * Blocks the calling thread if no PN search engine is available in the pool.
   * @return Pointer to the PN search engine object
   */
  PnSearchEngine* acquire();

  /**
   * Releases a PN search engine object back to the pool.
   * @param engine Pointer to the PN search engine object to release
   */
  void release(PnSearchEngine* engine);

 private:
  /**
   * Mutex for synchronization.
   */
  std::mutex _mutex;

  /**
   * Condition variable.
   */
  std::condition_variable _condition;

  /**
   * Array of PN search engine objects.
   */
  std::unique_ptr<PnSearchEngine*[]> _engines;

  /**
   * Queue of pointers to pooled PN search engines.
   */
  std::queue<PnSearchEngine*> _availableEngines;
};

}  // namespace deepshogi

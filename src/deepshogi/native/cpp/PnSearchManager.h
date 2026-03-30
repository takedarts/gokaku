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
   * Creates an object that manages PN search engine objects.
   * @param nodes Maximum number of nodes for search
   * @param size Number of PN search engines to pool
   */
  PnSearchManager(int32_t nodes, int32_t size);

  /**
   * Destroys the object that manages PN search engine objects.
   */
  virtual ~PnSearchManager();

  /**
   * Acquires a PN search engine object.
   * If no PN search engine objects are available in the pool, blocks the calling thread.
   * @return Pointer to a PN search engine object
   */
  PnSearchEngine* acquire();

  /**
   * Releases a PN search engine object.
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
   * List of PN search engine objects.
   */
  std::unique_ptr<PnSearchEngine*[]> _engines;

  /**
   * Pointers to available PN search engines in the pool.
   */
  std::queue<PnSearchEngine*> _availableEngines;
};

}  // namespace deepshogi

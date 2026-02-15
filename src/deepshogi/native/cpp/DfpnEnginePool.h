#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>

#include "DfpnEngine.h"

namespace deepshogi {

/**
 * Class that manages DFPN engine objects.
 */
class DfpnEnginePool {
 public:
  /**
   * Construct an object that manages DFPN engine instances.
   * @param nodes maximum number of search nodes
   * @param size number of DFPN engines to keep in the pool
   */
  DfpnEnginePool(int32_t nodes, int32_t size);

  /**
   * Destroy the object that manages DFPN engine instances.
   */
  virtual ~DfpnEnginePool();

  /**
   * Acquire a DFPN engine instance.
   * If no engine is available in the pool, this call blocks the calling thread.
   * @return pointer to a DFPN engine instance
   */
  DfpnEngine* acquire();

  /**
   * Return a DFPN engine instance to the pool.
   * @param engine pointer to the DFPN engine instance to return
   */
  void release(DfpnEngine* engine);

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
   * Array of pointers to DFPN engine instances.
   */
  std::unique_ptr<DfpnEngine*[]> _engines;

  /**
   * Queue of pointers to DFPN engines currently available in the pool.
   */
  std::queue<DfpnEngine*> _availableEngines;
};

}  // namespace deepshogi

#include "PnSearchManager.h"

#include <algorithm>

namespace deepshogi {

/**
 * Constructs a PnSearchManager object.
 * @param nodes Maximum number of nodes for the search
 * @param size Number of PN search engines to pool
 */
PnSearchManager::PnSearchManager(int32_t nodes, int32_t size)
    : _mutex(),
      _condition(),
      _engineSize(std::max(size, 1)),
      _engines(new PnSearchEngine*[std::max(size, 1)]()),
      _availableEngines() {
  // Construct engines using the same clamped count as the pointer array.
  const int32_t engine_size = std::max(size, 1);

  try {
    // Create each engine and add it to the pool.
    for (int32_t i = 0; i < engine_size; i++) {
      _engines[i] = new PnSearchEngine(nodes);
      _availableEngines.push(_engines[i]);
    }
  } catch (...) {
    // Free constructed engines if construction fails; the pointer array does not own them.
    for (int32_t i = 0; i < engine_size; i++) {
      delete _engines[i];
    }
    throw;
  }
}

/**
 * Destroys the PnSearchManager object.
 */
PnSearchManager::~PnSearchManager() {
  for (int32_t i = 0; i < _engineSize; i++) {
    delete _engines[i];
  }
}

/**
 * Acquires a PN search engine object.
 * Blocks the calling thread if no PN search engine is available in the pool.
 * @return Pointer to the PN search engine object
 */
PnSearchEngine* PnSearchManager::acquire() {
  // Protect the available queue and engine preparation/abort notification with the same mutex
  std::unique_lock<std::mutex> lock(_mutex);

  // While none is available, release the mutex and wait; recheck availability after a notification
  _condition.wait(lock, [this]() { return !_availableEngines.empty(); });

  // Get the engine to lend from the head of the available queue
  PnSearchEngine* engine = _availableEngines.front();

  // Remove the engine from the queue so that it is not lent to another worker
  _availableEngines.pop();

  // Serialize lending and stop notification with the same mutex and keep aborts before the start
  engine->_prepareSearch();

  // Return the prepared engine; the mutex lock is released when the function exits
  return engine;
}

/**
 * Releases a PN search engine object back to the pool.
 * @param engine Pointer to the PN search engine object to release
 */
void PnSearchManager::release(PnSearchEngine* engine) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Ignore aborts after return and prepare a new search on the next lending
  engine->_searchState.store(PnSearchEngine::SearchState::IDLE, std::memory_order_release);
  _availableEngines.push(engine);
  _condition.notify_one();
}

/**
 * Request an abort of the lent search engines.
 * Do not wait for them to finish.
 */
void PnSearchManager::abortSearch() {
  // Available engines are IDLE, so they pass through without a state change
  std::lock_guard<std::mutex> lock(_mutex);

  for (int32_t i = 0; i < _engineSize; i++) {
    _engines[i]->abortSearch();
  }
}

}  // namespace deepshogi

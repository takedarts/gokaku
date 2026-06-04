#include "PnSearchManager.h"

namespace deepshogi {

/**
 * Constructs a PnSearchManager object.
 * @param nodes Maximum number of nodes for the search
 * @param size Number of PN search engines to pool
 */
PnSearchManager::PnSearchManager(int32_t nodes, int32_t size)
    : _mutex(),
      _condition(),
      _engines(new PnSearchEngine*[size]),
      _availableEngines() {
  for (int32_t i = 0; i < size; ++i) {
    _engines[i] = new PnSearchEngine(nodes);
    _availableEngines.push(_engines[i]);
  }
}

/**
 * Destroys the PnSearchManager object.
 */
PnSearchManager::~PnSearchManager() {
  for (size_t i = 0; i < _availableEngines.size(); ++i) {
    delete _engines[i];
  }
}

/**
 * Acquires a PN search engine object.
 * Blocks the calling thread if no PN search engine is available in the pool.
 * @return Pointer to the PN search engine object
 */
PnSearchEngine* PnSearchManager::acquire() {
  std::unique_lock<std::mutex> lock(_mutex);
  _condition.wait(lock, [this]() { return !_availableEngines.empty(); });

  PnSearchEngine* engine = _availableEngines.front();
  _availableEngines.pop();
  return engine;
}

/**
 * Releases a PN search engine object back to the pool.
 * @param engine Pointer to the PN search engine object to release
 */
void PnSearchManager::release(PnSearchEngine* engine) {
  std::unique_lock<std::mutex> lock(_mutex);
  _availableEngines.push(engine);
  _condition.notify_one();
}

}  // namespace deepshogi

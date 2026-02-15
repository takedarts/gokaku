#include "DfpnEnginePool.h"

namespace deepshogi {

/**
 * Construct an object that manages DFPN engine instances.
 * @param nodes maximum number of search nodes
 * @param size number of DFPN engines to keep in the pool
 */
DfpnEnginePool::DfpnEnginePool(int32_t nodes, int32_t size)
    : _mutex(),
      _condition(),
      _engines(new DfpnEngine*[size]),
      _availableEngines() {
  for (int32_t i = 0; i < size; ++i) {
    _engines[i] = new DfpnEngine(nodes);
    _availableEngines.push(_engines[i]);
  }
}

/**
 * Destroy the object that manages DFPN engine instances.
 */
DfpnEnginePool::~DfpnEnginePool() {
  for (size_t i = 0; i < _availableEngines.size(); ++i) {
    delete _engines[i];
  }
}

/**
 * Acquire a DFPN engine instance.
 * If no engine is available in the pool, this call blocks the calling thread.
 * @return pointer to a DFPN engine instance
 */
DfpnEngine* DfpnEnginePool::acquire() {
  std::unique_lock<std::mutex> lock(_mutex);
  _condition.wait(lock, [this]() { return !_availableEngines.empty(); });

  DfpnEngine* engine = _availableEngines.front();
  _availableEngines.pop();
  return engine;
}

/**
 * Return a DFPN engine instance to the pool.
 * @param engine pointer to the DFPN engine instance to return
 */
void DfpnEnginePool::release(DfpnEngine* engine) {
  std::unique_lock<std::mutex> lock(_mutex);
  _availableEngines.push(engine);
  _condition.notify_one();
}

}  // namespace deepshogi

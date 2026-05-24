#include "PnSearchManager.h"

namespace deepshogi {

/**
 * PN探索エンジンのオブジェクトを管理するクラスのオブジェクトを生成する。
 * @param nodes 探索の最大ノード数
 * @param size プールするPN探索エンジンの数
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
 * PN探索エンジンのオブジェクトを管理するクラスのオブジェクトを破棄する。
 */
PnSearchManager::~PnSearchManager() {
  for (size_t i = 0; i < _availableEngines.size(); ++i) {
    delete _engines[i];
  }
}

/**
 * PN探索エンジンのオブジェクトを取得する。
 * プールにPN探索エンジンのオブジェクトが存在しない場合は、呼び出し元のスレッドをブロックする。
 * @return PN探索エンジンのオブジェクトへのポインタ
 */
PnSearchEngine* PnSearchManager::acquire() {
  std::unique_lock<std::mutex> lock(_mutex);
  _condition.wait(lock, [this]() { return !_availableEngines.empty(); });

  PnSearchEngine* engine = _availableEngines.front();
  _availableEngines.pop();
  return engine;
}

/**
 * PN探索エンジンのオブジェクトを返却する。
 * @param engine 返却するPN探索エンジンのオブジェクトへのポインタ
 */
void PnSearchManager::release(PnSearchEngine* engine) {
  std::unique_lock<std::mutex> lock(_mutex);
  _availableEngines.push(engine);
  _condition.notify_one();
}

}  // namespace deepshogi

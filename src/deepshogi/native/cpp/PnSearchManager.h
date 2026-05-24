#pragma once

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>

#include "PnSearchEngine.h"

namespace deepshogi {

/**
 * PN探索エンジンのオブジェクトを管理するクラス。
 */
class PnSearchManager {
 public:
  /**
   * PN探索エンジンのオブジェクトを管理するクラスのオブジェクトを生成する。
   * @param nodes 探索の最大ノード数
   * @param size プールするPN探索エンジンの数
   */
  PnSearchManager(int32_t nodes, int32_t size);

  /**
   * PN探索エンジンのオブジェクトを管理するクラスのオブジェクトを破棄する。
   */
  virtual ~PnSearchManager();

  /**
   * PN探索エンジンのオブジェクトを取得する。
   * プールにPN探索エンジンのオブジェクトが存在しない場合は、呼び出し元のスレッドをブロックする。
   * @return PN探索エンジンのオブジェクトへのポインタ
   */
  PnSearchEngine* acquire();

  /**
   * PN探索エンジンのオブジェクトを返却する。
   * @param engine 返却するPN探索エンジンのオブジェクトへのポインタ
   */
  void release(PnSearchEngine* engine);

 private:
  /**
   * 同期用のミューテックス。
   */
  std::mutex _mutex;

  /**
   * 条件変数。
   */
  std::condition_variable _condition;

  /**
   * PN探索エンジンのオブジェクト一覧。
   */
  std::unique_ptr<PnSearchEngine*[]> _engines;

  /**
   * プール中のPN探索エンジンへのポインタ。
   */
  std::queue<PnSearchEngine*> _availableEngines;
};

}  // namespace deepshogi

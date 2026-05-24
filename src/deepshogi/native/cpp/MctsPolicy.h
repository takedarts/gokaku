#pragma once

#include <cstdint>

#include "Move.h"

namespace deepshogi {

/**
 * MCTSの探索ノードの予想着手確率を管理するクラス。
 * 着手情報、その着手の予想確率、その着手を探索した回数を管理する。
 */
class MctsPolicy {
 public:
  /**
   * 予想着手確率を管理するオブジェクトを生成する。
   * @param move 着手情報
   * @param probability 予想着手確率
   */
  MctsPolicy(Move move, float probability);

  /**
   * 予想着手確率を管理するオブジェクトをコピーする。
   * @param policy コピー元のオブジェクト
   */
  MctsPolicy(const MctsPolicy& policy) = default;

  /**
   * 予想着手確率を管理するオブジェクトを破棄する。
   */
  virtual ~MctsPolicy() = default;

  /**
   * 着手情報を返す。
   * @return 着手情報
   */
  inline Move getMove() const {
    return _move;
  }

  /**
   * 予想着手確率を返す。
   * @return 予想着手確率
   */
  inline float getProbability() const {
    return _probability;
  }

  /**
   * この着手を探索した回数を返す。
   * @return この着手を探索した回数
   */
  inline int32_t getVisits() const {
    return _visits;
  }

  /**
   * この着手を探索した回数を1増やす。
   */
  inline void incrementVisits() {
    _visits++;
  }

 private:
  /**
   * 着手情報。
   */
  Move _move;

  /**
   * 予想着手確率。
   */
  float _probability;

  /**
   * この着手を探索した回数。
   */
  int32_t _visits;
};

}  // namespace deepshogi

#pragma once

#include <cstdint>

#include "Move.h"

namespace deepshogi {

/**
 * 予測着手確率を計算する構造体。
 */
struct Policy {
  /**
   * 予測着手確率のオブジェクトを作成する。
   * @param move 着手
   * @param policy 予測着手確率
   * @param visits 探索回数
   */
  Policy(Move move, float policy, int32_t visits);

  /**
   * 着手情報。
   */
  Move move;

  /**
   * 予測着手確率。
   */
  float policy;

  /**
   * 探索回数。
   */
  int32_t visits;
};

}  // namespace deepshogi

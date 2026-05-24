#pragma once

#include <utility>
#include <vector>

#include "Move.h"

namespace deepshogi {

/**
 * 盤面評価結果を格納する構造体。
 */
struct InferenceResult {
  /**
   * 評価結果オブジェクトを生成する。
   */
  InferenceResult() : value(0.0f), policies() {}

  /**
   * 盤面評価値。
   */
  float value;

  /**
   * 次の着手の予想確率の一覧。
   */
  std::vector<std::pair<Move, float>> policies;
};

}  // namespace deepshogi

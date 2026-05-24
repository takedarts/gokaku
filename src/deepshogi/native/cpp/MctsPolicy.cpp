#include "MctsPolicy.h"

namespace deepshogi {

/**
 * 予想着手確率を管理するオブジェクトを生成する。
 * @param move 着手情報
 * @param probability 予想着手確率
 */
MctsPolicy::MctsPolicy(Move move, float probability)
    : _move(move),
      _probability(probability),
      _visits(0) {
}

}  // namespace deepshogi

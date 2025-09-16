#include "Policy.h"

#include "Config.h"

namespace deepshogi {

/**
 * 予測着手確率のオブジェクトを作成する。
 * @param move 着手
 * @param policy 予測着手確率
 * @param visits 探索回数
 */
Policy::Policy(Move move, float policy, int32_t visits)
    : move(move),
      policy(policy),
      visits(visits) {
}

}  // namespace deepshogi

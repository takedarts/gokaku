#include "Policy.h"

#include "Config.h"

namespace deepshogi {

/**
 * Create an object for predicted move probability.
 * @param move Move
 * @param policy Predicted move probability
 * @param visits Number of searches
 */
Policy::Policy(Move move, float policy, int32_t visits)
    : move(move),
      policy(policy),
      visits(visits) {
}

}  // namespace deepshogi

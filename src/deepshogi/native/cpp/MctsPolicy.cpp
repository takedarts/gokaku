#include "MctsPolicy.h"

namespace deepshogi {

/**
 * Creates an object for managing predicted move probabilities.
 * @param move Move information
 * @param probability Predicted move probability
 */
MctsPolicy::MctsPolicy(Move move, float probability)
    : _move(move),
      _probability(probability),
      _visits(0) {
}

}  // namespace deepshogi

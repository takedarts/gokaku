#pragma once

#include <cstdint>

#include "Move.h"

namespace deepshogi {

/**
 * Struct for calculating predicted move probabilities.
 */
struct Policy {
  /**
   * Create an object for predicted move probability.
   * @param move Move
   * @param policy Predicted move probability
   * @param visits Number of searches
   */
  Policy(Move move, float policy, int32_t visits);

  /**
   * Move information.
   */
  Move move;

  /**
   * Predicted move probability.
   */
  float policy;

  /**
   * Number of searches.
   */
  int32_t visits;
};

}  // namespace deepshogi

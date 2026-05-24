#pragma once

#include <utility>
#include <vector>

#include "Move.h"

namespace deepshogi {

/**
 * Structure for storing board evaluation results.
 */
struct InferenceResult {
  /**
   * Creates an evaluation result object.
   */
  InferenceResult() : value(0.0f), policies() {}

  /**
   * Board evaluation value.
   */
  float value;

  /**
   * List of predicted probabilities for the next move.
   */
  std::vector<std::pair<Move, float>> policies;
};

}  // namespace deepshogi

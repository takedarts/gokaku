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
   * Construct an inference result.
   */
  InferenceResult() : value(0.0f), remainingTurns(0.0f), policies() {}

  /**
   * Position evaluation.
   */
  float value;

  /**
   * Predicted moves remaining until the game ends.
   */
  float remainingTurns;

  /**
   * Predicted next-move probabilities.
   */
  std::vector<std::pair<Move, float>> policies;
};

}  // namespace deepshogi

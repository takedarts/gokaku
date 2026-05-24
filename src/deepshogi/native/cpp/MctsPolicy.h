#pragma once

#include <cstdint>

#include "Move.h"

namespace deepshogi {

/**
 * A class for managing the predicted move probabilities of MCTS search nodes.
 * Manages the move information, its predicted probability, and the number of times it has been searched.
 */
class MctsPolicy {
 public:
  /**
   * Creates an object for managing predicted move probabilities.
   * @param move Move information
   * @param probability Predicted move probability
   */
  MctsPolicy(Move move, float probability);

  /**
   * Copies an object for managing predicted move probabilities.
   * @param policy The source object to copy from
   */
  MctsPolicy(const MctsPolicy& policy) = default;

  /**
   * Destroys the object for managing predicted move probabilities.
   */
  virtual ~MctsPolicy() = default;

  /**
   * Returns the move information.
   * @return Move information
   */
  inline Move getMove() const {
    return _move;
  }

  /**
   * Returns the predicted move probability.
   * @return Predicted move probability
   */
  inline float getProbability() const {
    return _probability;
  }

  /**
   * Returns the number of times this move has been searched.
   * @return Number of times this move has been searched
   */
  inline int32_t getVisits() const {
    return _visits;
  }

  /**
   * Increments the number of times this move has been searched by 1.
   */
  inline void incrementVisits() {
    _visits++;
  }

 private:
  /**
   * Move information.
   */
  Move _move;

  /**
   * Predicted move probability.
   */
  float _probability;

  /**
   * Number of times this move has been searched.
   */
  int32_t _visits;
};

}  // namespace deepshogi

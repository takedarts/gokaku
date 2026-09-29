#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "Move.h"

namespace deepshogi {

/**
 * Candidate move class.
 */
class Candidate {
 public:
  /**
   * Create candidate move data.
   * @param move Move.
   * @param color Player color.
   * @param visits Visit count.
   * @param policy Predicted move probability.
   * @param value Evaluation value.
   * @param remainingTurns Predicted moves remaining until the game ends.
   * @param variations Principal variation.
   */
  Candidate(
      Move move, int32_t color, int32_t visits, float policy, float value,
      float remainingTurns, std::vector<Move> variations);

  /**
   * Create candidate move data.
   * @param move Move.
   * @param color Player color.
   * @param visits Visit count.
   * @param policy Predicted move probability.
   * @param value Evaluation value.
   * @param remainingTurns Predicted moves remaining until the game ends.
   */
  Candidate(
      Move move, int32_t color, int32_t visits, float policy, float value,
      float remainingTurns);

  /**
   * Destroy the instance.
   */
  virtual ~Candidate() = default;

  /**
   * Returns a string representation of the candidate move.
   * @return String representation of the candidate move.
   */
  std::string toString() const;

  /**
   * Returns the move.
   * @return Move
   */
  inline Move getMove() const {
    return _move;
  }

  /**
   * Returns the turn color.
   * @return Turn color
   */
  inline int32_t getColor() const {
    return _color;
  }

  /**
   * Returns the visit count.
   * @return Visit count
   */
  inline int32_t getVisits() const {
    return _visits;
  }

  /**
   * Return the predicted move probability.
   * @return Predicted move probability.
   */
  inline float getPolicy() const {
    return _policy;
  }

  /**
   * Returns the evaluation value.
   * @return Evaluation value
   */
  inline float getValue() const {
    return _value;
  }

  /**
   * Return the predicted moves remaining until the game ends.
   * @return Predicted moves remaining until the game ends.
   */
  inline float getRemainingTurns() const {
    return _remainingTurns;
  }

  /**
   * Return the principal variation.
   * @return Principal variation.
   */
  inline std::vector<Move> getVariations() const {
    return _variations;
  }

  /**
   * Writes the candidate move information to an output stream.
   * @param os Output stream.
   * @param candidate Candidate move object.
   * @return Output stream.
   */
  friend std::ostream& operator<<(std::ostream& os, const Candidate& candidate) {
    os << candidate.toString();
    return os;
  }

 private:
  /**
   * Move.
   */
  Move _move;

  /**
   * Turn color.
   */
  int32_t _color;

  /**
   * Visit count.
   */
  int32_t _visits;

  /**
   * Predicted move probability.
   */
  float _policy;

  /**
   * Evaluation value.
   */
  float _value;

  /**
   * Predicted moves remaining until the game ends.
   */
  float _remainingTurns;

  /**
   * Principal variation.
   */
  std::vector<Move> _variations;
};

}  // namespace deepshogi

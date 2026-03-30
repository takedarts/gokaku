#pragma once

#include <cstdint>
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
   * @param move Move
   * @param color Side to move
   * @param visits Number of visits
   * @param playouts Number of playouts
   * @param policy Predicted move probability
   * @param value Predicted win rate
   * @param minimax Minimax value
   * @param variations Predicted sequence
   */
  Candidate(
      Move move, int32_t color, int32_t visits, int32_t playouts,
      float policy, float value, float minimax, std::vector<Move> variations);

  /**
   * Create candidate move data.
   * @param move Move
   * @param color Side to move
   * @param visits Number of visits
   * @param playouts Number of playouts
   * @param policy Predicted move probability
   * @param value Predicted win rate
   * @param minimax Minimax value
   */
  Candidate(
      Move move, int32_t color, int32_t visits, int32_t playouts,
      float policy, float value, float minimax);

  /**
   * Destroy the instance.
   */
  virtual ~Candidate() = default;

  /**
   * Get the move.
   * @return Move
   */
  Move getMove() const;

  /**
   * Get the side to move.
   * @return Side to move
   */
  int32_t getColor() const;

  /**
   * Get the number of visits.
   * @return Number of visits
   */
  int32_t getVisits() const;

  /**
   * Get the number of playouts.
   * @return Number of playouts
   */
  int32_t getPlayouts() const;

  /**
   * Get the predicted move probability.
   * @return Predicted move probability
   */
  float getPolicy() const;

  /**
   * Get the predicted win rate.
   * @return Predicted win rate
   */
  float getValue() const;

  /**
   * Get the minimax value.
   * @return Minimax value
   */
  float getMinimax() const;

  /**
   * Get the predicted sequence.
   * @return Predicted sequence
   */
  std::vector<Move> getVariations() const;

  /**
   * Return the string representation of the candidate move.
   * @return String representation of the candidate move.
   */
  std::string toString() const;

  /**
   * Write the candidate move information to the output stream.
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
   * Side to move.
   */
  int32_t _color;

  /**
   * Number of visits.
   */
  int32_t _visits;

  /**
   * Number of playouts.
   */
  int32_t _playouts;

  /**
   * Predicted move probability.
   */
  float _policy;

  /**
   * Predicted win rate.
   */
  float _value;

  /**
   * Minimax value.
   */
  float _minimax;

  /**
   * Predicted sequence.
   */
  std::vector<Move> _variations;
};

}  // namespace deepshogi

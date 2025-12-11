#include "Candidate.h"

namespace deepshogi {

/**
 * Create candidate move data.
 * @param move Move
 * @param visits Number of visits
 * @param playouts Number of playouts
 * @param policy Predicted move probability
 * @param value Predicted win rate
 * @param minimax Minimax value
 * @param variations Predicted sequence
 */
Candidate::Candidate(
    Move move, int32_t color, int32_t visits, int32_t playouts,
    float policy, float value, float minimax, std::vector<Move> variations)
    : _move(move),
      _color(color),
      _visits(visits),
      _playouts(playouts),
      _policy(policy),
      _value(value),
      _minimax(minimax),
      _variations(variations) {
}

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
Candidate::Candidate(
    Move move, int32_t color, int32_t visits, int32_t playouts,
    float policy, float value, float minimax)
    : Candidate(
          move, color, visits, playouts,
          policy, value, minimax, std::vector<Move>()) {
  _variations.push_back(move);
}

/**
 * Get the move.
 * @return Move
 */
Move Candidate::getMove() const {
  return _move;
}

/**
 * Get the side to move.
 * @return Side to move
 */
int32_t Candidate::getColor() const {
  return _color;
}

/**
 * Get the number of visits.
 * @return Number of visits
 */
int32_t Candidate::getVisits() const {
  return _visits;
}

/**
 * Get the number of playouts.
 * @return Number of playouts
 */
int32_t Candidate::getPlayouts() const {
  return _playouts;
}

/**
 * Get the predicted move probability.
 * @return Predicted move probability
 */
float Candidate::getPolicy() const {
  return _policy;
}

/**
 * Get the predicted win rate.
 * @return Predicted win rate
 */
float Candidate::getValue() const {
  return _value;
}

/**
 * Get the minimax value.
 * @return Minimax value
 */
float Candidate::getMinimax() const {
  return _minimax;
}

/**
 * Get the predicted sequence.
 * @return Predicted sequence
 */
std::vector<Move> Candidate::getVariations() const {
  return _variations;
}

}  // namespace deepshogi

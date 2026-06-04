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
   * Creates candidate move data.
   * @param move Move
   * @param color Turn color
   * @param visits Visit count
   * @param playouts Playout count
   * @param policy Predicted move probability
   * @param minimax Minimax evaluation value
   * @param variations Predicted line
   */
  Candidate(
      Move move, int32_t color, int32_t visits, int32_t playouts,
      float policy, float value, std::vector<Move> variations);

  /**
   * Creates candidate move data.
   * @param move Move
   * @param color Turn color
   * @param visits Visit count
   * @param playouts Playout count
   * @param policy Predicted move probability
   * @param value Evaluation value
   */
  Candidate(
      Move move, int32_t color, int32_t visits, int32_t playouts,
      float policy, float value);

  /**
   * Destroys the instance.
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
   * Returns the playout count.
   * @return Playout count
   */
  inline int32_t getPlayouts() const {
    return _playouts;
  }

  /**
   * Returns the predicted move probability.
   * @return Predicted move probability
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
   * Returns the predicted line.
   * @return Predicted line
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
   * Playout count.
   */
  int32_t _playouts;

  /**
   * Predicted move probability.
   */
  float _policy;

  /**
   * Evaluation value.
   */
  float _value;

  /**
   * Predicted line.
   */
  std::vector<Move> _variations;
};

}  // namespace deepshogi

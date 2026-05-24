#include "Candidate.h"

#include <sstream>

namespace deepshogi {

/**
 * Creates candidate move data.
 * @param move Move
 * @param visits Visit count
 * @param playouts Playout count
 * @param policy Predicted move probability
 * @param value Evaluation value
 * @param variations Predicted line
 */
Candidate::Candidate(
    Move move, int32_t color, int32_t visits, int32_t playouts,
    float policy, float value, std::vector<Move> variations)
    : _move(move),
      _color(color),
      _visits(visits),
      _playouts(playouts),
      _policy(policy),
      _value(value),
      _variations(variations) {
}

/**
 * Creates candidate move data.
 * @param move Move
 * @param color Turn color
 * @param visits Visit count
 * @param playouts Playout count
 * @param policy Predicted move probability
 * @param value Evaluation value
 */
Candidate::Candidate(
    Move move, int32_t color, int32_t visits, int32_t playouts,
    float policy, float value)
    : Candidate(
          move, color, visits, playouts,
          policy, value, std::vector<Move>()) {
  _variations.push_back(move);
}

/**
 * Returns a string representation of the candidate move.
 * @return String representation of the candidate move.
 */
std::string Candidate::toString() const {
  std::stringstream ss;
  ss << "Move: " << _move.toString()
     << ", Color: " << ((_color == COLOR_BLACK) ? "Black" : "White")
     << ", Visits: " << _visits
     << ", Playouts: " << _playouts
     << ", Policy: " << _policy
     << ", Value: " << _value;

  if (!_variations.empty()) {
    ss << ", Variations: [";

    for (size_t i = 0; i < _variations.size(); ++i) {
      ss << _variations[i].toString();

      if (i < _variations.size() - 1) {
        ss << ", ";
      }
    }

    ss << "]";
  }

  return ss.str();
}

}  // namespace deepshogi

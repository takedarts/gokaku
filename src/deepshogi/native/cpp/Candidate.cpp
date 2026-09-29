#include "Candidate.h"

#include <algorithm>
#include <sstream>

namespace deepshogi {

/**
 * Create candidate move data.
 * @param move Move.
 * @param visits Visit count.
 * @param policy Predicted move probability.
 * @param value Evaluation value.
 * @param remainingTurns Predicted moves remaining until the game ends.
 * @param variations Principal variation.
 */
Candidate::Candidate(
    Move move, int32_t color, int32_t visits, float policy, float value,
    float remainingTurns, std::vector<Move> variations)
    : _move(move),
      _color(color),
      _visits(visits),
      _policy(policy),
      _value(value),
      _remainingTurns(std::max(remainingTurns, 0.0f)),
      _variations(variations) {
}

/**
 * Create candidate move data.
 * @param move Move.
 * @param color Player color.
 * @param visits Visit count.
 * @param policy Predicted move probability.
 * @param value Evaluation value.
 * @param remainingTurns Predicted moves remaining until the game ends.
 */
Candidate::Candidate(
    Move move, int32_t color, int32_t visits, float policy, float value,
    float remainingTurns)
    : Candidate(
          move, color, visits, policy, value, remainingTurns,
          std::vector<Move>()) {
  _variations.push_back(move);
}

/**
 * Return a string representation of the candidate.
 * @return String representation of the candidate.
 */
std::string Candidate::toString() const {
  std::stringstream ss;
  ss << "Move: " << _move.toString()
     << ", Color: " << ((_color == COLOR_BLACK) ? "Black" : "White")
     << ", Visits: " << _visits
     << ", Policy: " << _policy
     << ", Value: " << _value
     << ", Remaining Turns: " << _remainingTurns;

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

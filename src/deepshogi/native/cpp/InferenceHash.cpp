#include "InferenceHash.h"

#include <algorithm>

namespace deepshogi {

/**
 * Create a hash for caching inference results.
 * @param board Board object.
 */
InferenceHash::InferenceHash(const Board* board)
    : _boardHash(board) {
  _nyugyokuScores[0] = board->_nyugyokuScores[0];
  _nyugyokuScores[1] = board->_nyugyokuScores[1];
  _remainingTurn = static_cast<int8_t>(
      std::clamp<int32_t>(board->_drawTurn - board->_turn, 0, 50));
}

}  // namespace deepshogi

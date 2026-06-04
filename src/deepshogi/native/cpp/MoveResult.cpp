#include "MoveResult.h"

#include "Config.h"

namespace deepshogi {

/**
 * Creates a move result object.
 * The move is initialized to MOVE_INVALID.
 * The captured piece is initialized to PIECE_EMPTY.
 */
MoveResult::MoveResult()
    : _move(MOVE_INVALID),
      _captured(PIECE_EMPTY) {
}

/**
 * Creates an object holding the specified move and captured piece.
 * Specify PIECE_EMPTY as the captured piece if no piece was captured.
 * @param move Move object
 * @param captured Integer value representing the type of the captured piece
 */
MoveResult::MoveResult(const Move& move, uint8_t captured)
    : _move(move),
      _captured(captured) {
}

}  // namespace deepshogi

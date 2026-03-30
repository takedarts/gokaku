#include "Result.h"

#include "Config.h"

namespace deepshogi {

/**
 * Creates a move result object.
 * The move is initialized to MOVE_INVALID.
 * The captured piece is initialized to PIECE_EMPTY.
 */
Result::Result()
    : _move(MOVE_INVALID),
      _captured(PIECE_EMPTY) {
}

/**
 * Creates an object that holds information about the specified move and captured piece.
 * If there is no captured piece, specify PIECE_EMPTY as the captured piece.
 * @param move the move object
 * @param captured an integer value representing the type of captured piece
 */
Result::Result(const Move& move, uint8_t captured)
    : _move(move),
      _captured(captured) {
}

}  // namespace deepshogi

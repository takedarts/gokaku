#pragma once

#include <cstdint>

#include "Move.h"

namespace deepshogi {

/**
 * A class representing the result of a move.
 * This class holds the move and the piece captured by it.
 * This class is used to restore the board state.
 */
class MoveResult {
 public:
  /**
   * Creates a move result object.
   * The move is initialized to MOVE_INVALID.
   * The captured piece is initialized to PIECE_EMPTY.
   */
  MoveResult();

  /**
   * Creates an object holding the specified move and captured piece.
   * Specify PIECE_EMPTY as the captured piece if no piece was captured.
   * @param move Move object
   * @param captured Integer value representing the type of the captured piece
   */
  MoveResult(const Move& move, uint8_t captured);

  /**
   * Uses the default copy constructor implementation.
   */
  MoveResult(const MoveResult& other) = default;

  /**
   * Destroys the object.
   */
  virtual ~MoveResult() = default;

  /**
   * Returns the move.
   * @return Move object representing the move
   */
  inline Move getMove() const {
    return _move;
  }

  /**
   * Returns the type of the captured piece.
   * Returns PIECE_EMPTY if no piece was captured.
   * @return Integer value representing the type of the captured piece
   */
  inline uint8_t getCaptured() const {
    return _captured;
  }

 private:
  /**
   * Move.
   */
  Move _move;

  /**
   * Type of the piece captured by the move. PIECE_EMPTY if no piece was captured.
   */
  uint8_t _captured;
};

}  // namespace deepshogi

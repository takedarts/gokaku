#pragma once

#include <cstdint>

#include "Move.h"

namespace deepshogi {

/**
 * A class representing the result of a move.
 * This class holds information about a move and the piece captured during that move.
 * This class is used to restore the board state.
 */
class Result {
 public:
  /**
   * Creates a move result object.
   * The move is initialized to MOVE_INVALID.
   * The captured piece is initialized to PIECE_EMPTY.
   */
  Result();

  /**
   * Creates an object holding information about the specified move and captured piece.
   * If no piece was captured, specify PIECE_EMPTY as the captured piece.
   * @param move the Move object
   * @param captured an integer value representing the type of captured piece
   */
  Result(const Move& move, uint8_t captured);

  /**
   * Uses the default implementation for the copy constructor.
   */
  Result(const Result& other) = default;

  /**
   * Destroys the object.
   */
  virtual ~Result() = default;

  /**
   * Gets the move.
   * @return a Move object representing the move
   */
  inline Move getMove() const {
    return _move;
  }

  /**
   * Gets the type of captured piece.
   * Returns PIECE_EMPTY if no piece was captured.
   * @return an integer value representing the type of captured piece
   */
  inline uint8_t getCaptured() const {
    return _captured;
  }

 private:
  /**
   * The move.
   */
  Move _move;

  /**
   * The type of piece captured as a result of the move. PIECE_EMPTY if no piece was captured.
   */
  uint8_t _captured;
};

}  // namespace deepshogi

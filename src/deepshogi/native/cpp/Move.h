#pragma once

#include <cstdint>
#include <ostream>
#include <string>

#include "Position.h"

namespace deepshogi {

/**
 * A class that manages move information.
 *
 * A move is represented as a 16-bit integer with the following bit layout:
 * - Bits 0-6:  Destination square index (0-80)
 * - Bits 7-13: Source square index (0-80)
 * - Bit 14:    Promotion flag (0: no promotion, 1: promote)
 * - Bit 15:    Always 0 (1 for invalid moves)
 *
 * A negative value (typically -1) is used for invalid moves.
 */
class Move {
 public:
  /**
   * Creates an invalid move object.
   */
  Move();

  /**
   * Creates an object with the specified source/destination square indices and promotion flag.
   * @param src Source square index
   * @param dst Destination square index
   * @param promote Whether to promote
   */
  Move(const Position& src, const Position& dst, bool promote);

  /**
   * Creates an object from the given move value.
   * @param move Move value
   */
  Move(int16_t move);

  /**
   * Copy constructor.
   */
  Move(const Move& move) = default;

  /**
   * Destroys the object.
   */
  virtual ~Move() = default;

  /**
   * Returns the string representation of the move.
   * @return String representation of the move.
   */
  std::string toString() const;

  /**
   * Returns the source square.
   * @return Source square
   */
  inline Position getSrc() const {
    return Position((_move >> 7) & 0x7F);
  }

  /**
   * Returns the destination square.
   * @return Destination square
   */
  inline Position getDst() const {
    return Position((_move >> 0) & 0x7F);
  }

  /**
   * Returns true if this move includes a promotion.
   * @return True if this move includes a promotion
   */
  inline bool isPromote() const {
    return (_move >> 14) & 0x1;
  }

  /**
   * Returns the move value.
   * @return Move value
   */
  inline int16_t getValue() const {
    return _move;
  }

  /**
   * Returns true if the move is valid.
   * @return True if the move is valid
   */
  inline bool isValid() const {
    return _move >= 0;
  }

  /**
   * Returns true if the moves are equal.
   */
  inline bool operator==(const Move& move) const {
    return _move == move._move;
  }

  /**
   * Returns true if the moves are not equal.
   */
  inline bool operator!=(const Move& move) const {
    return _move != move._move;
  }

  /**
   * Returns true if this move is less than the specified move.
   */
  inline bool operator<(const Move& move) const {
    return _move < move._move;
  }

  /**
   * Writes the string representation of the move to the output stream.
   * @param os Output stream.
   * @param move Move object.
   * @return Output stream.
   */
  friend std::ostream& operator<<(std::ostream& os, const Move& move) {
    os << move.toString();
    return os;
  }

 private:
  /**
   * Move value representing this move.
   */
  int16_t _move;
};

// Constant representing an invalid move
const Move MOVE_INVALID(-1);

}  // namespace deepshogi

#pragma once

#include <cstdint>
#include <ostream>
#include <string>

#include "Position.h"

namespace deepshogi {

/**
 * Class to manage move information.
 *
 * A move is represented as a 16-bit integer with the following bit allocation:
 * - Bits 0-6: Destination coordinate number (0-80)
 * - Bits 7-13: Source coordinate number (0-80)
 * - Bit 14: Promotion flag (0: no promotion, 1: promotion)
 * - Bit 15: Always 0 (1 for invalid moves)
 *
 * For invalid moves, a negative value (typically -1) is used.
 */
class Move {
 public:
  /**
   * Creates an invalid move object.
   */
  Move();

  /**
   * Creates an object by specifying the source and destination coordinate numbers and promotion flag.
   * @param src Source coordinate number
   * @param dst Destination coordinate number
   * @param promote Promotion flag
   */
  Move(const Position& src, const Position& dst, bool promote);

  /**
   * Creates an object by specifying a move number.
   * @param move Move number
   */
  Move(int16_t move);

  /**
   * Copy constructor.
   */
  Move(const Move& move) = default;

  /**
   * Destructs the object.
   */
  virtual ~Move() = default;

  /**
   * Returns the string representation of the move.
   * @return String representation of the move.
   */
  std::string toString() const;

  /**
   * Gets the source coordinate.
   * @return Source coordinate
   */
  inline Position getSrc() const {
    return Position((_move >> 7) & 0x7F);
  }

  /**
   * Gets the destination coordinate.
   * @return Destination coordinate
   */
  inline Position getDst() const {
    return Position((_move >> 0) & 0x7F);
  }

  /**
   * Returns true if the move includes promotion.
   * @return true if the move includes promotion
   */
  inline bool isPromote() const {
    return (_move >> 14) & 0x1;
  }

  /**
   * Returns the move number.
   * @return Move number
   */
  inline int16_t getValue() const {
    return _move;
  }

  /**
   * Returns true if the move is valid.
   * @return true if the move is valid
   */
  inline bool isValid() const {
    return _move >= 0;
  }

  /**
   * Returns true if the moves are the same.
   */
  inline bool operator==(const Move& move) const {
    return _move == move._move;
  }

  /**
   * Returns true if the moves are different.
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
   * Writes the string representation of the move object to the output stream.
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
   * The move number representing this move.
   */
  int16_t _move;
};

// Constant representing an invalid move
const Move MOVE_INVALID(-1);

}  // namespace deepshogi

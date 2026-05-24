#pragma once

#include <cstdint>
#include <ostream>
#include <string>

#include "Config.h"

namespace deepshogi {

/**
 * A class representing a coordinate on the board.
 *
 * The X and Y coordinates use the top-right corner as (0, 0),
 * with X increasing to the right and Y increasing downward.
 *   (8, 0), ... (2, 0), (1, 0), (0, 0)
 *   (8, 1), ... (2, 1), (1, 1), (0, 1)
 *   ...
 *   (8, 8), ... (2, 8), (1, 8), (0, 8)
 *
 * The coordinate index used as the internal representation is assigned
 * in column-major order from the top-right.
 *   index = Y * BOARD_SIZE + X
 *
 * The coordinate index is represented as an 8-bit integer (int8_t).
 * An invalid coordinate is represented by a negative index (typically -1).
 */
class Position {
 public:
  /**
   * Creates an object representing an invalid coordinate.
   */
  Position();

  /**
   * Creates an object with the specified coordinates.
   * @param x X coordinate.
   * @param y Y coordinate.
   */
  Position(int8_t x, int8_t y);

  /**
   * Creates an object with the specified coordinate index.
   * @param index Coordinate index.
   */
  Position(int8_t index);

  /**
   * Creates an object by copying the specified coordinate.
   */
  Position(const Position& other) = default;

  /**
   * Destroys the object.
   */
  virtual ~Position() = default;

  /**
   * Returns the string representation of the coordinate.
   * @return String representation of the coordinate.
   */
  std::string toString() const;

  /**
   * Returns the coordinate index.
   * @return Coordinate index.
   */
  inline int8_t getIndex() const {
    return _index;
  }

  /**
   * Returns the X coordinate.
   * @return X coordinate.
   */
  inline int8_t getX() const {
    return _index / BOARD_SIZE;
  }

  /**
   * Returns the Y coordinate.
   * @return Y coordinate.
   */
  inline int8_t getY() const {
    return _index % BOARD_SIZE;
  }

  /**
   * Returns whether the coordinate is valid.
   * @return true if the coordinate is valid, false otherwise.
   */
  inline bool isValid() const {
    return _index >= 0;
  }

  /**
   * Returns whether the coordinates are equal.
   */
  inline bool operator==(const Position& other) const {
    return _index == other._index;
  }

  /**
   * Returns whether the coordinates are not equal.
   */
  inline bool operator!=(const Position& other) const {
    return _index != other._index;
  }

  /**
   * Returns true if this coordinate is less than the specified coordinate.
   */
  inline bool operator<(const Position& other) const {
    return _index < other._index;
  }

  /**
   * Writes the string representation of a coordinate object to an output stream.
   * @param os Output stream.
   * @param position Coordinate object.
   * @return Output stream.
   */
  friend std::ostream& operator<<(std::ostream& os, const Position& position) {
    os << position.toString();
    return os;
  }

 private:
  /**
   * Coordinate index.
   */
  int8_t _index;
};

// Object representing an invalid coordinate
const Position POSITION_INVALID = Position(-1);

}  // namespace deepshogi

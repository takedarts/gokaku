#pragma once

#include <cstdint>
#include <ostream>
#include <string>

#include "Config.h"

namespace deepshogi {

/**
 * A class representing coordinates on the board.
 *
 * The X and Y coordinates have (0, 0) at the top-right, with X increasing rightward and Y increasing downward.
 *   (8, 0), ... (2, 0), (1, 0), (0, 0)
 *   (8, 1), ... (2, 1), (1, 1), (0, 1)
 *   ...
 *   (8, 8), ... (2, 8), (1, 8), (0, 8)
 *
 * The index used as the internal representation is assigned from top-right in row-major order.
 *   index = Y coordinate * BOARD_SIZE + X coordinate
 *
 * The index is represented as an 8-bit integer (int8_t).
 * Invalid coordinates are represented by a negative index value (typically -1).
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
   * Creates an object with the specified index.
   * @param index Index.
   */
  Position(int8_t index);

  /**
   * Creates an object by copying the specified coordinates.
   */
  Position(const Position& other) = default;

  /**
   * Destroys the object.
   */
  virtual ~Position() = default;

  /**
   * Returns the string representation of the coordinates.
   * @return String representation of the coordinates.
   */
  std::string toString() const;

  /**
   * Returns the index of the coordinates.
   * @return Index of the coordinates.
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
   * Returns whether the coordinates are valid.
   * @return True if the coordinates are valid, false otherwise.
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
   * Returns whether this coordinate is less than the specified coordinate.
   */
  inline bool operator<(const Position& other) const {
    return _index < other._index;
  }

  /**
   * Writes the string representation of the coordinates to the output stream.
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
   * Index of the coordinates.
   */
  int8_t _index;
};

// Object representing invalid coordinates
const Position POSITION_INVALID = Position(-1);

}  // namespace deepshogi

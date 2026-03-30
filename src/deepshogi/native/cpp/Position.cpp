#include "Position.h"

#include <sstream>

namespace deepshogi {

/**
 * Creates an object representing an invalid position.
 */
Position::Position()
    : _index(-1) {
}

/**
 * Creates an object with the specified coordinates.
 * @param x X coordinate.
 * @param y Y coordinate.
 */
Position::Position(int8_t x, int8_t y)
    : _index(y + x * BOARD_SIZE) {
}

/**
 * Creates an object with the specified position index.
 * @param index Position index.
 */
Position::Position(int8_t index)
    : _index(index) {
}

/**
 * Returns the string representation of the position.
 * @return String representation of the position.
 */
std::string Position::toString() const {
  if (_index < 0) {
    return "(N/A)";
  }

  std::stringstream ss;
  ss << "(" << static_cast<int>(getX()) << ", " << static_cast<int>(getY()) << ")";
  return ss.str();
}

}  // namespace deepshogi

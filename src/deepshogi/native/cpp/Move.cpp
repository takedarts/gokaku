#include "Move.h"

#include <sstream>

#include "Config.h"

namespace deepshogi {

/**
 * Creates an invalid move object.
 */
Move::Move()
    : _move(-1) {
}

/**
 * Creates an object with the specified source/destination square indices and promotion flag.
 * @param src Source square index
 * @param dst Destination square index
 * @param promote Whether to promote
 */
Move::Move(const Position& src, const Position& dst, bool promote)
    : _move(((src.getIndex() & 0x7F) << 7) |
            ((dst.getIndex() & 0x7F) << 0) |
            ((promote ? 1 : 0) << 14)) {
}

/**
 * Creates an object from the given move value.
 * @param move Move value
 */
Move::Move(int16_t move)
    : _move(move) {
}

/**
 * Returns the string representation of the move.
 * @return String representation of the move.
 */
std::string Move::toString() const {
  if (_move < 0) {
    return "invalid";
  }

  std::stringstream ss;
  ss << getSrc() << " -> " << getDst();
  ss << " (pro=" << (isPromote() ? "1" : "0") << ")";
  return ss.str();
}

}  // namespace deepshogi

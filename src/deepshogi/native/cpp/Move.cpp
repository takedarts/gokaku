#include "Move.h"

#include "Config.h"

namespace deepshogi {

/**
 * Create a move object.
 */
Move::Move() : Move(-1, -1, -1, -1, false) {}

/**
 * Create a move object.
 * @param srcX Source X coordinate
 * @param srcY Source Y coordinate
 * @param dstX Destination X coordinate
 * @param dstY Destination Y coordinate
 * @param promote True if promotion
 */
Move::Move(int32_t srcX, int32_t srcY, int32_t dstX, int32_t dstY, bool promote)
    : _srcX(srcX), _srcY(srcY), _dstX(dstX), _dstY(dstY), _promote(promote) {
}

/**
 * Get the source X coordinate.
 * @return X coordinate
 */
int32_t Move::getSrcX() const {
  return _srcX;
}

/**
 * Get the source Y coordinate.
 * @return Y coordinate
 */
int32_t Move::getSrcY() const {
  return _srcY;
}

/**
 * Get the destination X coordinate.
 * @return X coordinate
 */
int32_t Move::getDstX() const {
  return _dstX;
}

/**
 * Get the destination Y coordinate.
 * @return Y coordinate
 */
int32_t Move::getDstY() const {
  return _dstY;
}

/**
 * Return true if promotion.
 * @return True if promotion
 */
bool Move::isPromote() const {
  return _promote;
}

/**
 * Return true if this is a pass move.
 * @return True if this is a pass move
 */
bool Move::isPass() const {
  return _srcX == -1;
}

/**
 * Return the value representing this move.
 * This value is the same as cshogi's move representation `move16`.
 * @return Value representing the move
 */
int32_t Move::getValue() const {
  return (
      ((_srcX * BOARD_SIZE + _srcY) << 7) |
      ((_dstX * BOARD_SIZE + _dstY) << 0) |
      (_promote << 14));
}

/**
 * Return true if the moves are the same.
 */
bool Move::operator==(const Move& move) const {
  return (
      _srcX == move._srcX &&
      _srcY == move._srcY &&
      _dstX == move._dstX &&
      _dstY == move._dstY &&
      _promote == move._promote);
}

/**
 * Return true if the moves are different.
 */
bool Move::operator!=(const Move& move) const {
  return (
      _srcX != move._srcX ||
      _srcY != move._srcY ||
      _dstX != move._dstX ||
      _dstY != move._dstY ||
      _promote != move._promote);
}

/**
 * Return true if this move is less than the specified move.
 */
bool Move::operator<(const Move& move) const {
  return (
      _srcX < move._srcX ||
      _srcY < move._srcY ||
      _dstX < move._dstX ||
      _dstY < move._dstY ||
      _promote < move._promote);
}

}  // namespace deepshogi

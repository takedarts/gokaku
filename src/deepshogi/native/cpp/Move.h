#pragma once

#include <cstdint>

namespace deepshogi {

/**
 * Class to manage move information.
 */
class Move {
 public:
  /**
   * Create a move object.
   */
  Move();

  /**
   * Create a move object.
   * @param srcX Source X coordinate
   * @param srcY Source Y coordinate
   * @param dstX Destination X coordinate
   * @param dstY Destination Y coordinate
   * @param promote True if promotion
   */
  Move(int32_t srcX, int32_t srcY, int32_t dstX, int32_t dstY, bool promote);

  /**
   * Copy constructor.
   */
  Move(const Move& move) = default;

  /**
   * Destroy the object.
   */
  virtual ~Move() = default;

  /**
   * Get the source X coordinate.
   * @return X coordinate
   */
  int32_t getSrcX() const;

  /**
   * Get the source Y coordinate.
   * @return Y coordinate
   */
  int32_t getSrcY() const;

  /**
   * Get the destination X coordinate.
   * @return X coordinate
   */
  int32_t getDstX() const;

  /**
   * Get the destination Y coordinate.
   * @return Y coordinate
   */
  int32_t getDstY() const;

  /**
   * Return true if promotion.
   * @return True if promotion
   */
  bool isPromote() const;

  /**
   * Return true if this is a pass move.
   * @return True if this is a pass move
   */
  bool isPass() const;

  /**
   * Return the value representing this move.
   * This value is the same as cshogi's move representation `move16`.
   * @return Value representing the move
   */
  int32_t getValue() const;

  /**
   * Return true if the moves are the same.
   */
  bool operator==(const Move& move) const;

  /**
   * Return true if the moves are different.
   */
  bool operator!=(const Move& move) const;

  /**
   * Return true if this move is less than the specified move.
   */
  bool operator<(const Move& move) const;

 private:
  /**
   * Source X coordinate.
   */
  int32_t _srcX;

  /**
   * Source Y coordinate.
   */
  int32_t _srcY;

  /**
   * Destination X coordinate.
   */
  int32_t _dstX;

  /**
   * Destination Y coordinate.
   */
  int32_t _dstY;

  /**
   * True if promotion.
   */
  bool _promote;
};

// Pass move object.
const Move MOVE_PASS = Move(-1, -1, -1, -1, false);

}  // namespace deepshogi

#pragma once

#include <array>
#include <cstdint>

#include "BitBoard.h"
#include "Board.h"

namespace deepshogi {

/**
 * Hold a position hash.
 */
class BoardHash {
 public:
  /**
   * Creates an object that holds the hash value of a board state.
   * @param board Board object
   */
  BoardHash(const Board* board);

  /**
   * Copies an object that holds the hash value of a board state.
   * @param other The source object holding the board hash value to copy from
   */
  BoardHash(const BoardHash& other) = default;

  /**
   * Destroys the object that holds the hash value of a board state.
   */
  virtual ~BoardHash() = default;

  /**
   * Convert all comparison fields into an integer array for cache lookup.
   * @return std::array<uint64_t, 6> position key.
   */
  inline std::array<uint64_t, 6> getCacheKey() const {
    // Preserve all comparison fields without virtual-table data or padding.
    return {_cellHash, _colorBitBoards[0].getLower(), _colorBitBoards[1].getLower(),
            _handBits[0], _handBits[1],
            uint64_t(_colorBitBoards[0].getUpper()) |
                (uint64_t(_colorBitBoards[1].getUpper()) << 32)};
  }

  /**
   * Compare whether this object is greater than the specified object.
   * @param other BoardHash object to compare.
   * @return True if this object is less than the specified object.
   */
  inline bool operator<(const BoardHash& other) const {
    if (_cellHash != other._cellHash) {
      return _cellHash < other._cellHash;
    } else if (_colorBitBoards[0] != other._colorBitBoards[0]) {
      return _colorBitBoards[0] < other._colorBitBoards[0];
    } else if (_colorBitBoards[1] != other._colorBitBoards[1]) {
      return _colorBitBoards[1] < other._colorBitBoards[1];
    } else if (_handBits[0] != other._handBits[0]) {
      return _handBits[0] < other._handBits[0];
    } else if (_handBits[1] != other._handBits[1]) {
      return _handBits[1] < other._handBits[1];
    } else {
      return false;
    }
  }

  /**
   * Return true if this position is identical or inferior to the specified position.
   * An inferior position has the same side to move and on-board arrangement,
   * with no more pieces of any type in hand for the specified color.
   * Matching position hashes are used to check on-board piece arrangements.
   * Hash collisions are considered sufficiently rare to ignore here.
   * To check the hand-piece condition, the set bits in this hand representation
   * must form a subset of those in the other hand representation.
   * @param other const BoardHash& position to compare.
   * @param color int8_t color whose hand pieces are compared.
   * @return bool; true for an identical or inferior position.
   */
  inline bool isLesserThanOrEqual(const BoardHash& other, int8_t color) const {
    // Different hashes, including the side to move, indicate different positions.
    if (_cellHash != other._cellHash) {
      return false;
    }

    // Return false if the on-board arrangements differ.
    if (_colorBitBoards[0] != other._colorBitBoards[0] ||
        _colorBitBoards[1] != other._colorBitBoards[1]) {
      return false;
    }

    // Check that the set bits of this hand representation
    // form a subset of the other hand representation.
    int8_t color_idx = (color == COLOR_BLACK) ? 0 : 1;

    return (_handBits[color_idx] & other._handBits[color_idx]) == _handBits[color_idx];
  }

  /**
   * Return true if this position is inferior to the specified position.
   * @param other const BoardHash& position to compare.
   * @param color int8_t color whose hand pieces are compared.
   * @return bool; true for an inferior position.
   */
  inline bool isLesserThan(const BoardHash& other, int8_t color) const {
    // Different hashes, including the side to move, indicate different positions.
    if (_cellHash != other._cellHash) {
      return false;
    }

    // Return false if the on-board arrangements differ.
    if (_colorBitBoards[0] != other._colorBitBoards[0] ||
        _colorBitBoards[1] != other._colorBitBoards[1]) {
      return false;
    }

    // Equal hand bit representations imply equal hand-piece counts.
    if (_handBits[0] == other._handBits[0] && _handBits[1] == other._handBits[1]) {
      return false;
    }

    // Check that the set bits of this hand representation
    // form a subset of the other hand representation.
    int8_t color_idx = (color == COLOR_BLACK) ? 0 : 1;

    return (_handBits[color_idx] & other._handBits[color_idx]) == _handBits[color_idx];
  }

 private:
  /**
   * Hash of the on-board arrangement, including the side to move.
   */
  uint64_t _cellHash;

  /**
   * Bitboards representing the piece placement.
   */
  BitBoard _colorBitBoards[2];

  /**
   * Bit representation of the number of pieces in hand.
   */
  uint64_t _handBits[2];
};

}  // namespace deepshogi

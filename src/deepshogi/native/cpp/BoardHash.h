#pragma once

#include <cstdint>

#include "BitBoard.h"
#include "Board.h"

namespace deepshogi {

/**
 * A class that holds the hash value of a board state.
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
   * Compares whether this object is less than the specified object.
   * @param other The BoardHash object to compare against
   * @return true if this object is less than the specified object
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

 private:
  /**
   * Hash value representing the arrangement of pieces on the board.
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

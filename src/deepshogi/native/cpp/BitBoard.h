#pragma once

#include <bit>
#include <cstdint>
#include <ostream>
#include <string>

namespace deepshogi {

/**
 * A class for representing the board state as a bit string and performing efficient calculations.
 * Each bit represents a specific position on the board, and is used to indicate the presence and type of pieces.
 * Bit 0 is assigned to the upper-right of the board, with bits assigned in column-major order.
 * For example, in a 9x9 shogi board, bit 0 represents the upper-right square, and bit 8 represents the lower-right square.
 * Since this is designed for a shogi board, the following two variables are used to manage the bit state:
 *  - lower (int64_t): Bit string representing the lower bits of the board (bits 0-53).
 *  - upper (int32_t): Bit string representing the upper bits of the board (bits 54-80).
 */
class BitBoard {
 public:
  /**
   * Constructs an object with all bits set to 0.
   */
  BitBoard();

  /**
   * Constructs an object with the specified bit strings.
   * @param lower Bit string representing the lower bits of the board.
   * @param upper Bit string representing the upper bits of the board.
   */
  BitBoard(uint64_t lower, uint32_t upper);

  /**
   * Constructs an object with the bit at the specified position set.
   * @param index Integer value representing the position to set the bit
   */
  BitBoard(int8_t index);

  /**
   * Constructs an object by copying the specified bit string.
   */
  BitBoard(const BitBoard& other) = default;

  /**
   * Destroys the object.
   */
  virtual ~BitBoard() = default;

  /**
   * Sets the bit at the specified position.
   * @param index Integer value representing the position to set the bit
   */
  void setBit(int8_t index);

  /**
   * Clears the bit at the specified position.
   * @param index Integer value representing the position to clear the bit
   */
  void clearBit(int8_t index);

  /**
   * Returns whether the bit at the specified position is set.
   * @param index Integer value representing the position to check the bit state
   * @return true if the bit at the specified position is set
   */
  bool hasBit(int8_t index) const;

  /**
   * Returns the position of the leftmost set bit.
   * Returns -1 if all bits are 0.
   * @return Integer value representing the position of the leftmost set bit
   */
  int8_t getLeftmostBitIndex() const;

  /**
   * Returns the position of the rightmost set bit.
   * Returns -1 if all bits are 0.
   * @return Integer value representing the position of the rightmost set bit
   */
  int8_t getRightmostBitIndex() const;

  /**
   * Clears the rightmost set bit and returns its position.
   * Returns -1 if all bits are 0.
   * @return Integer value representing the position of the rightmost set bit
   */
  int8_t popRightmostBitIndex();

  /**
   * Returns a string representation of the bit string.
   * @return String representing the bit string
   */
  std::string toString() const;

  /**
   * Returns the result of computing the bitwise NOT of the bit string.
   * @return BitBoard object with the bitwise NOT applied
   */
  BitBoard operator~() const;

  /**
   * Left-shifts the bit string and assigns the result to this object.
   * @param shift Number of bits to shift
   * @return Reference to the BitBoard object with the left-shifted result
   */
  BitBoard& operator<<=(int32_t shift);

  /**
   * Right-shifts the bit string and assigns the result to this object.
   * @param shift Number of bits to shift
   * @return Reference to the BitBoard object with the right-shifted result
   */
  BitBoard& operator>>=(int32_t shift);

  /**
   * Resets all bits to 0.
   */
  inline void clearAll() {
    _lower = 0;
    _upper = 0;
  }

  /**
   * Returns the lower part of the bit string representing the board.
   * @return Lower bits of the board
   */
  inline uint64_t getLower() const {
    return _lower;
  }

  /**
   * Returns the upper part of the bit string representing the board.
   * @return Upper bits of the board
   */
  inline uint32_t getUpper() const {
    return _upper;
  }

  /**
   * Returns the number of set bits in the bit string.
   * @return Integer value representing the number of set bits in the bit string
   */
  inline int8_t countBit() const {
    return std::popcount(_lower) + std::popcount(_upper);
  }

  /**
   * Returns true if any bit in the bit string is set.
   * @return true if any bit in the bit string is set
   */
  explicit inline operator bool() const {
    return _lower != 0 || _upper != 0;
  }

  /**
   * Returns true if the bit strings are equal.
   */
  inline bool operator==(const BitBoard& other) const {
    return _lower == other._lower && _upper == other._upper;
  }

  /**
   * Returns true if the bit strings are different.
   */
  inline bool operator!=(const BitBoard& other) const {
    return !(*this == other);
  }

  /**
   * Computes the bitwise AND with the bit string and assigns the result to this object.
   * @param other BitBoard object to AND with
   * @return Reference to the BitBoard object with the AND result
   */
  inline BitBoard& operator&=(const BitBoard& other) {
    _lower &= other._lower;
    _upper &= other._upper;
    return *this;
  }

  /**
   * Computes the bitwise OR with the bit string and assigns the result to this object.
   * @param other BitBoard object to OR with
   * @return Reference to the BitBoard object with the OR result
   */
  inline BitBoard& operator|=(const BitBoard& other) {
    _lower |= other._lower;
    _upper |= other._upper;
    return *this;
  }

  /**
   * Computes the bitwise XOR with the bit string and assigns the result to this object.
   * @param other BitBoard object to XOR with
   * @return Reference to the BitBoard object with the XOR result
   */
  inline BitBoard& operator^=(const BitBoard& other) {
    _lower ^= other._lower;
    _upper ^= other._upper;
    return *this;
  }

  /**
   * Returns the bitwise AND of the bit strings.
   * @param other BitBoard object to AND with
   * @return BitBoard object with the AND result
   */
  inline BitBoard operator&(const BitBoard& other) const {
    return BitBoard(_lower & other._lower, _upper & other._upper);
  }

  /**
   * Returns the bitwise OR of the bit strings.
   * @param other BitBoard object to OR with
   * @return BitBoard object with the OR result
   */
  inline BitBoard operator|(const BitBoard& other) const {
    return BitBoard(_lower | other._lower, _upper | other._upper);
  }

  /**
   * Returns the bitwise XOR of the bit strings.
   * @param other BitBoard object to XOR with
   * @return BitBoard object with the XOR result
   */
  inline BitBoard operator^(const BitBoard& other) const {
    return BitBoard(_lower ^ other._lower, _upper ^ other._upper);
  }

  /**
   * Returns the result of left-shifting the bit string.
   * @param shift Number of bits to shift
   * @return BitBoard object with the left-shifted result
   */
  inline BitBoard operator<<(int32_t shift) const {
    BitBoard result(*this);
    result <<= shift;
    return result;
  }

  /**
   * Returns the result of right-shifting the bit string.
   * @param shift Number of bits to shift
   * @return BitBoard object with the right-shifted result
   */
  inline BitBoard operator>>(int32_t shift) const {
    BitBoard result(*this);
    result >>= shift;
    return result;
  }

  /**
   * Compares the bit strings in order.
   * The upper part is compared first; if equal, the lower part is compared.
   * @param other BitBoard object to compare against
   * @return true if this object's bit string is less than the other's
   */
  inline bool operator<(const BitBoard& other) const {
    if (_upper != other._upper) {
      return _upper < other._upper;
    } else {
      return _lower < other._lower;
    }
  }

  /**
   * Writes the string representation of the bit string to an output stream.
   * @param os Output stream
   * @param bitboard BitBoard object
   * @return Output stream
   */
  friend std::ostream& operator<<(std::ostream& os, const BitBoard& bitboard) {
    os << bitboard.toString();
    return os;
  }

 private:
  /**
   * Bit string representing the lower bits of the board.
   */
  uint64_t _lower;

  /**
   * Bit string representing the upper bits of the board.
   */
  uint32_t _upper;
};

}  // namespace deepshogi

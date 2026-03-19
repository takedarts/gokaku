#pragma once

#include <bit>
#include <cstdint>
#include <ostream>
#include <string>

namespace deepshogi {

/**
 * A class for efficiently performing calculations by representing the board state as a bit sequence.
 * Each bit represents a specific position on the board and is used to indicate the presence and type of pieces.
 * Bit 0 corresponds to the top-right of the board, with bits assigned in vertical and horizontal order.
 * For example, in a 9x9 shogi board, bit 0 represents the top-right square and bit 8 represents the bottom-right square.
 * Since shogi boards are targeted, the following two variables are used to manage the bit state:
 *  - lower (int64_t): Bit sequence representing the lower bits of the board (bits 0-53).
 *  - upper (int32_t): Bit sequence representing the upper bits of the board (bits 54-80).
 */
class BitBoard {
 public:
  /**
   * Creates an object with all bits set to 0.
   */
  BitBoard();

  /**
   * Creates an object with the specified bit sequence set.
   * @param lower Bit sequence representing the lower bits of the board.
   * @param upper Bit sequence representing the upper bits of the board.
   */
  BitBoard(uint64_t lower, uint32_t upper);

  /**
   * Creates an object with a bit set at the specified position.
   * @param index Integer value representing the position to set the bit.
   */
  BitBoard(int8_t index);

  /**
   * Creates an object by copying the specified bit sequence.
   */
  BitBoard(const BitBoard& other) = default;

  /**
   * Destroys the object.
   */
  virtual ~BitBoard() = default;

  /**
   * Sets a bit at the specified position.
   * @param index Integer value representing the position to set the bit.
   */
  void setBit(int8_t index);

  /**
   * Clears the bit at the specified position.
   * @param index Integer value representing the position to clear the bit.
   */
  void clearBit(int8_t index);

  /**
   * Returns whether the bit at the specified position is set.
   * @param index Integer value representing the position to check the bit state.
   * @return true if the bit at the specified position is set.
   */
  bool hasBit(int8_t index) const;

  /**
   * Returns the position of the leftmost set bit.
   * Returns -1 if all bits are 0.
   * @return Integer value representing the position of the leftmost set bit.
   */
  int8_t getLeftmostBitIndex() const;

  /**
   * Returns the position of the rightmost set bit.
   * Returns -1 if all bits are 0.
   * @return Integer value representing the position of the rightmost set bit.
   */
  int8_t getRightmostBitIndex() const;

  /**
   * Clears the rightmost set bit and returns its position.
   * Returns -1 if all bits are 0.
   * @return Integer value representing the position of the rightmost set bit.
   */
  int8_t popRightmostBitIndex();

  /**
   * Represents the bit sequence in string format.
   * @return String representing the bit sequence.
   */
  std::string toString() const;

  /**
   * Returns the result of the logical negation of the bit sequence.
   * @return BitBoard object with the logical negation applied.
   */
  BitBoard operator~() const;

  /**
   * Left-shifts the bit sequence and assigns it to this object.
   * @param shift Number of bits to shift.
   * @return Reference to the BitBoard object with the left-shift applied.
   */
  BitBoard& operator<<=(int32_t shift);

  /**
   * Right-shifts the bit sequence and assigns it to this object.
   * @param shift Number of bits to shift.
   * @return Reference to the BitBoard object with the right-shift applied.
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
   * Returns the lower part of the bit sequence representing the board.
   * @return Lower bits of the board.
   */
  inline uint64_t getLower() const {
    return _lower;
  }

  /**
   * Returns the upper part of the bit sequence representing the board.
   * @return Upper bits of the board.
   */
  inline uint32_t getUpper() const {
    return _upper;
  }

  /**
   * Returns the count of set bits in the bit sequence.
   * @return Integer value representing the count of set bits.
   */
  inline int8_t countBit() const {
    return std::popcount(_lower) + std::popcount(_upper);
  }

  /**
   * Returns true if any bit in the bit sequence is set.
   * @return true if any bit is set.
   */
  explicit inline operator bool() const {
    return _lower != 0 || _upper != 0;
  }

  /**
   * Returns true if the bit sequences are equal.
   */
  inline bool operator==(const BitBoard& other) const {
    return _lower == other._lower && _upper == other._upper;
  }

  /**
   * Returns true if the bit sequences are different.
   */
  inline bool operator!=(const BitBoard& other) const {
    return !(*this == other);
  }

  /**
   * Performs bitwise AND and assigns the result to this object.
   * @param other BitBoard object to perform AND with.
   * @return Reference to the BitBoard object with the AND result.
   */
  inline BitBoard& operator&=(const BitBoard& other) {
    _lower &= other._lower;
    _upper &= other._upper;
    return *this;
  }

  /**
   * Performs bitwise OR and assigns the result to this object.
   * @param other BitBoard object to perform OR with.
   * @return Reference to the BitBoard object with the OR result.
   */
  inline BitBoard& operator|=(const BitBoard& other) {
    _lower |= other._lower;
    _upper |= other._upper;
    return *this;
  }

  /**
   * Performs bitwise XOR and assigns the result to this object.
   * @param other BitBoard object to perform XOR with.
   * @return Reference to the BitBoard object with the XOR result.
   */
  inline BitBoard& operator^=(const BitBoard& other) {
    _lower ^= other._lower;
    _upper ^= other._upper;
    return *this;
  }

  /**
   * Returns the bitwise AND of the bit sequences.
   * @param other BitBoard object to perform AND with.
   * @return BitBoard object with the AND result.
   */
  inline BitBoard operator&(const BitBoard& other) const {
    return BitBoard(_lower & other._lower, _upper & other._upper);
  }

  /**
   * Returns the bitwise OR of the bit sequences.
   * @param other BitBoard object to perform OR with.
   * @return BitBoard object with the OR result.
   */
  inline BitBoard operator|(const BitBoard& other) const {
    return BitBoard(_lower | other._lower, _upper | other._upper);
  }

  /**
   * Returns the bitwise XOR of the bit sequences.
   * @param other BitBoard object to perform XOR with.
   * @return BitBoard object with the XOR result.
   */
  inline BitBoard operator^(const BitBoard& other) const {
    return BitBoard(_lower ^ other._lower, _upper ^ other._upper);
  }

  /**
   * Returns the result of left-shifting the bit sequence.
   * @param shift Number of bits to shift.
   * @return BitBoard object with the left-shift applied.
   */
  inline BitBoard operator<<(int32_t shift) const {
    BitBoard result(*this);
    result <<= shift;
    return result;
  }

  /**
   * Returns the result of right-shifting the bit sequence.
   * @param shift Number of bits to shift.
   * @return BitBoard object with the right-shift applied.
   */
  inline BitBoard operator>>(int32_t shift) const {
    BitBoard result(*this);
    result >>= shift;
    return result;
  }

  /**
   * Writes the string representation of the bit sequence to an output stream.
   * @param os Output stream.
   * @param board BitBoard object.
   * @return Output stream.
   */
  friend std::ostream& operator<<(std::ostream& os, const BitBoard& bitboard) {
    os << bitboard.toString();
    return os;
  }

 private:
  /**
   * Bit sequence representing the lower bits of the board.
   */
  uint64_t _lower;

  /**
   * Bit sequence representing the upper bits of the board.
   */
  uint32_t _upper;
};

}  // namespace deepshogi

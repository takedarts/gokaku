#include "BitBoard.h"

#include <sstream>

#include "Config.h"

namespace deepshogi {

static constexpr int8_t LOWER_SIZE(BOARD_SIZE * 6);                     // lower bit size
static constexpr int8_t UPPER_SIZE(BOARD_SIZE * 3);                     // upper bit size
static constexpr uint64_t LOWER_MASK((uint64_t(1) << LOWER_SIZE) - 1);  // lower bit mask
static constexpr uint32_t UPPER_MASK((uint32_t(1) << UPPER_SIZE) - 1);  // upper bit mask

/**
 * Constructs an object with all bits set to 0.
 */
BitBoard::BitBoard()
    : _lower(0),
      _upper(0) {
}

/**
 * Constructs an object with the specified bit strings.
 * @param lower Bit string representing the lower bits of the board.
 * @param upper Bit string representing the upper bits of the board.
 */
BitBoard::BitBoard(uint64_t lower, uint32_t upper)
    : _lower(lower),
      _upper(upper) {
}

/**
 * Constructs an object with the bit at the specified position set.
 * @param index Integer value representing the position to set the bit
 */
BitBoard::BitBoard(int8_t index) {
  if (index < LOWER_SIZE) {
    _lower = uint64_t(1) << index;
    _upper = 0;
  } else {
    _lower = 0;
    _upper = uint32_t(1) << (index - LOWER_SIZE);
  }
}

/**
 * Sets the bit at the specified position.
 * @param index Integer value representing the position to set the bit
 */
void BitBoard::setBit(int8_t index) {
  if (index < LOWER_SIZE) {
    _lower |= uint64_t(1) << index;
  } else {
    _upper |= uint32_t(1) << (index - LOWER_SIZE);
  }
}

/**
 * Clears the bit at the specified position.
 * @param index Integer value representing the position to clear the bit
 */
void BitBoard::clearBit(int8_t index) {
  if (index < LOWER_SIZE) {
    _lower &= ~(uint64_t(1) << index);
  } else {
    _upper &= ~(uint32_t(1) << (index - LOWER_SIZE));
  }
}

/**
 * Returns whether the bit at the specified position is set.
 * @param index Integer value representing the position to check the bit state
 * @return true if the bit at the specified position is set
 */
bool BitBoard::hasBit(int8_t index) const {
  if (index < LOWER_SIZE) {
    return (_lower & (uint64_t(1) << index)) != 0;
  } else {
    return (_upper & (uint32_t(1) << (index - LOWER_SIZE))) != 0;
  }
}

/**
 * Returns the position of the leftmost set bit.
 * Returns -1 if all bits are 0.
 * @return Integer value representing the position of the leftmost set bit
 */
int8_t BitBoard::getLeftmostBitIndex() const {
  if (_upper != 0) {
    return LOWER_SIZE + (31 - std::countl_zero(_upper));
  } else if (_lower != 0) {
    return 63 - std::countl_zero(_lower);
  } else {
    return -1;
  }
}

/**
 * Returns the position of the rightmost set bit.
 * Returns -1 if all bits are 0.
 * @return Integer value representing the position of the rightmost set bit
 */
int8_t BitBoard::getRightmostBitIndex() const {
  if (_lower != 0) {
    return std::countr_zero(_lower);
  } else if (_upper != 0) {
    return LOWER_SIZE + std::countr_zero(_upper);
  } else {
    return -1;
  }
}

/**
 * Returns the position of the rightmost set bit.
 * Returns -1 if all bits are 0.
 * @return Integer value representing the position of the rightmost set bit
 */
int8_t BitBoard::popRightmostBitIndex() {
  if (_lower != 0) {
    int8_t index = std::countr_zero(_lower);
    _lower &= _lower - 1;  // clear the rightmost bit
    return index;
  } else if (_upper != 0) {
    int8_t index = LOWER_SIZE + std::countr_zero(_upper);
    _upper &= _upper - 1;  // clear the rightmost bit
    return index;
  } else {
    return -1;
  }
}

/**
 * Returns the result of computing the bitwise NOT of the bit string.
 * @return BitBoard object with the bitwise NOT applied
 */
BitBoard BitBoard::operator~() const {
  return BitBoard(~_lower & LOWER_MASK, ~_upper & UPPER_MASK);
}

/**
 * Left-shifts the bit string and assigns the result to this object.
 * @param shift Number of bits to shift
 * @return Reference to the BitBoard object with the left-shifted result
 */
BitBoard& BitBoard::operator<<=(int32_t shift) {
  if (shift < UPPER_SIZE) {
    _upper = (uint32_t)(_lower >> (LOWER_SIZE - shift)) | (_upper << shift);
    _lower <<= shift;
  } else if (shift < LOWER_SIZE) {
    _upper = (uint32_t)(_lower >> (LOWER_SIZE - shift));
    _lower <<= shift;
  } else {
    _upper = (uint32_t)(_lower << (shift - LOWER_SIZE));
    _lower = 0;
  }

  _lower &= LOWER_MASK;
  _upper &= UPPER_MASK;

  return *this;
}

/**
 * Right-shifts the bit string and assigns the result to this object.
 * @param shift Number of bits to shift
 * @return Reference to the BitBoard object with the right-shifted result
 */
BitBoard& BitBoard::operator>>=(int32_t shift) {
  if (shift < UPPER_SIZE) {
    _lower = (uint64_t(_upper) << (LOWER_SIZE - shift)) | (_lower >> shift);
    _upper >>= shift;
  } else if (shift < LOWER_SIZE) {
    _lower = (uint64_t(_upper) << (LOWER_SIZE - shift)) | (_lower >> shift);
    _upper = 0;
  } else {
    _lower = (_upper >> (shift - LOWER_SIZE));
    _upper = 0;
  }

  _lower &= LOWER_MASK;
  _upper &= UPPER_MASK;

  return *this;
}

/**
 * Returns a string representation of the bit string.
 * @return String representing the bit string
 */
std::string BitBoard::toString() const {
  std::stringstream ss;

  for (int32_t y = 0; y < BOARD_SIZE; ++y) {
    for (int32_t x = BOARD_SIZE - 1; x >= 0; --x) {
      uint8_t index = y + x * BOARD_SIZE;

      if (index < LOWER_SIZE) {
        ss << ((_lower & (1ULL << index)) != 0 ? " * " : " . ");
      } else {
        ss << ((_upper & (1U << (index - LOWER_SIZE))) != 0 ? " * " : " . ");
      }
    }

    if (y < BOARD_SIZE - 1) {
      ss << '\n';
    }
  }

  return ss.str();
}

}  // namespace deepshogi

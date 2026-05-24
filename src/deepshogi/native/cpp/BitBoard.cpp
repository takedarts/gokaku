#include "BitBoard.h"

#include <sstream>

#include "Config.h"

namespace deepshogi {

static constexpr int8_t LOWER_SIZE(BOARD_SIZE * 6);                     // 下位ビットサイズ
static constexpr int8_t UPPER_SIZE(BOARD_SIZE * 3);                     // 上位ビットサイズ
static constexpr uint64_t LOWER_MASK((uint64_t(1) << LOWER_SIZE) - 1);  // 下位ビットのマスク
static constexpr uint32_t UPPER_MASK((uint32_t(1) << UPPER_SIZE) - 1);  // 上位ビットのマスク

/**
 * 全てのビットを0に設定したオブジェクトを生成する。
 */
BitBoard::BitBoard()
    : _lower(0),
      _upper(0) {
}

/**
 * 指定されたビット列を設定したオブジェクトを生成する。
 * @param lower 盤面の下位ビットを表すビット列。
 * @param upper 盤面の上位ビットを表すビット列。
 */
BitBoard::BitBoard(uint64_t lower, uint32_t upper)
    : _lower(lower),
      _upper(upper) {
}

/**
 * 指定された位置にビットを立てたオブジェクトを生成する。
 * @param index ビットを立てる位置を表す整数値
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
 * 指定された位置にビットを立てる。
 * @param index ビットを立てる位置を表す整数値
 */
void BitBoard::setBit(int8_t index) {
  if (index < LOWER_SIZE) {
    _lower |= uint64_t(1) << index;
  } else {
    _upper |= uint32_t(1) << (index - LOWER_SIZE);
  }
}

/**
 * 指定された位置のビットをクリアする。
 * @param index ビットをクリアする位置を表す整数値
 */
void BitBoard::clearBit(int8_t index) {
  if (index < LOWER_SIZE) {
    _lower &= ~(uint64_t(1) << index);
  } else {
    _upper &= ~(uint32_t(1) << (index - LOWER_SIZE));
  }
}

/**
 * 指定された位置のビットが立っているかを返す。
 * @param index ビットの状態を確認する位置を表す整数値
 * @return 指定された位置のビットが立っている場合はtrue
 */
bool BitBoard::hasBit(int8_t index) const {
  if (index < LOWER_SIZE) {
    return (_lower & (uint64_t(1) << index)) != 0;
  } else {
    return (_upper & (uint32_t(1) << (index - LOWER_SIZE))) != 0;
  }
}

/**
 * 最も左のビットが立っている位置を返す。
 * ビット列が全て0である場合は-1を返す。
 * @return 最も左のビットが立っている位置を表す整数値
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
 * 最も右のビットが立っている位置を返す。
 * ビット列が全て0である場合は-1を返す。
 * @return 最も右のビットが立っている位置を表す整数値
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
 * 最も右のビットが立っている位置をクリアし、その位置を返す。
 * ビット列が全て0である場合は-1を返す。
 * @return 最も右のビットが立っている位置を表す整数値
 */
int8_t BitBoard::popRightmostBitIndex() {
  if (_lower != 0) {
    int8_t index = std::countr_zero(_lower);
    _lower &= _lower - 1;  // 最も右のビットをクリア
    return index;
  } else if (_upper != 0) {
    int8_t index = LOWER_SIZE + std::countr_zero(_upper);
    _upper &= _upper - 1;  // 最も右のビットをクリア
    return index;
  } else {
    return -1;
  }
}

/**
 * ビット列の論理否定を計算した結果を返す。
 * @return 論理否定を取った結果のBitBoardオブジェクト
 */
BitBoard BitBoard::operator~() const {
  return BitBoard(~_lower & LOWER_MASK, ~_upper & UPPER_MASK);
}

/**
 * ビット列を左にシフトしてこのオブジェクトに代入する。
 * @param shift シフトするビット数
 * @return 左シフトした結果のBitBoardオブジェクトへの参照
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
 * ビット列を右にシフトしてこのオブジェクトに代入する。
 * @param shift シフトするビット数
 * @return 右シフトした結果のBitBoardオブジェクトへの参照
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
 * ビット列を文字列形式で表現する。
 * @return ビット列を表す文字列
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

#include "Position.h"

#include <sstream>

namespace deepshogi {

/**
 * 無効な座標を表すオブジェクトを生成する。
 */
Position::Position()
    : _index(-1) {
}

/**
 * 座標を指定してオブジェクトを生成する。
 * @param x X座標。
 * @param y Y座標。
 */
Position::Position(int8_t x, int8_t y)
    : _index(y + x * BOARD_SIZE) {
}

/**
 * 座標番号を指定してオブジェクトを生成する。
 * @param index 座標番号。
 */
Position::Position(int8_t index)
    : _index(index) {
}

/**
 * 座標の文字列表現を返す。
 * @return 座標の文字列表現。
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

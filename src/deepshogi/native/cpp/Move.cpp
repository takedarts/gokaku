#include "Move.h"

#include <sstream>

#include "Config.h"

namespace deepshogi {

/**
 * 無効な着手オブジェクトを作成する。
 */
Move::Move()
    : _move(-1) {
}

/**
 * 移動元と移動先の座標番号、成りの有無を指定してオブジェクトを作成する。
 * @param src 移動元の座標番号
 * @param dst 移動先の座標番号
 * @param promote 成りの有無
 */
Move::Move(const Position& src, const Position& dst, bool promote)
    : _move(((src.getIndex() & 0x7F) << 7) |
            ((dst.getIndex() & 0x7F) << 0) |
            ((promote ? 1 : 0) << 14)) {
}

/**
 * 着手番号を指定してオブジェクトを作成する。
 * @param move 着手番号
 */
Move::Move(int16_t move)
    : _move(move) {
}

/**
 * 着手の文字列表現を返す。
 * @return 着手の文字列表現。
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

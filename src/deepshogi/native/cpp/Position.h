#pragma once

#include <cstdint>
#include <ostream>
#include <string>

#include "Config.h"

namespace deepshogi {

/**
 * 盤面上の座標を表現するクラス。
 *
 * X座標とY座標は右上を(0, 0)とし、右方向にX座標・下方向にY座標が増加する。
 *   (8, 0), ... (2, 0), (1, 0), (0, 0)
 *   (8, 1), ... (2, 1), (1, 1), (0, 1)
 *   ...
 *   (8, 8), ... (2, 8), (1, 8), (0, 8)
 *
 * 内部表現としても使用する座標番号は右上から縦横の順で割り当てられる。
 *   座標番号（index） = Y座標 * BOARD_SIZE + X座標
 *
 * 座標番号は8ビットの整数（int8_t）で表現される。
 * 無効な座標は座標番号が負の値（通常は-1）で表現される。
 */
class Position {
 public:
  /**
   * 無効な座標を表すオブジェクトを生成する。
   */
  Position();

  /**
   * 座標を指定してオブジェクトを生成する。
   * @param x X座標。
   * @param y Y座標。
   */
  Position(int8_t x, int8_t y);

  /**
   * 座標番号を指定してオブジェクトを生成する。
   * @param index 座標番号。
   */
  Position(int8_t index);

  /**
   * 指定された座標をコピーしてオブジェクトを生成する。
   */
  Position(const Position& other) = default;

  /**
   * オブジェクトを破棄する。
   */
  virtual ~Position() = default;

  /**
   * 座標の文字列表現を返す。
   * @return 座標の文字列表現。
   */
  std::string toString() const;

  /**
   * 座標番号を返す。
   * @return 座標番号。
   */
  inline int8_t getIndex() const {
    return _index;
  }

  /**
   * X座標を返す。
   * @return X座標。
   */
  inline int8_t getX() const {
    return _index / BOARD_SIZE;
  }

  /**
   * Y座標を返す。
   * @return Y座標。
   */
  inline int8_t getY() const {
    return _index % BOARD_SIZE;
  }

  /**
   * 座標が有効かどうかを返す。
   * @return 座標が有効ならtrue、無効ならfalse。
   */
  inline bool isValid() const {
    return _index >= 0;
  }

  /**
   * 座標が等しいかどうかを返す。
   */
  inline bool operator==(const Position& other) const {
    return _index == other._index;
  }

  /**
   * 座標が等しくないかどうかを返す。
   */
  inline bool operator!=(const Position& other) const {
    return _index != other._index;
  }

  /**
   * この座標が指定された座標より小さいならtrueを返す。
   */
  inline bool operator<(const Position& other) const {
    return _index < other._index;
  }

  /**
   * 座標オブジェクトの文字列表現を出力ストリームに書き込む。
   * @param os 出力ストリーム。
   * @param position 座標オブジェクト。
   * @return 出力ストリーム。
   */
  friend std::ostream& operator<<(std::ostream& os, const Position& position) {
    os << position.toString();
    return os;
  }

 private:
  /**
   * 座標番号。
   */
  int8_t _index;
};

// 無効な座標を表すオブジェクト
const Position POSITION_INVALID = Position(-1);

}  // namespace deepshogi

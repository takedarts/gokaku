#pragma once

#include <cstdint>
#include <ostream>
#include <string>

#include "Position.h"

namespace deepshogi {

/**
 * 着手の情報を管理するクラス。
 *
 * 着手は16ビットの整数で表され、以下のビット割り当てがされている。
 * - 0-6ビット目: 移動先の座標番号 (0-80)
 * - 7-13ビット目: 移動元の座標番号 (0-80)
 * - 14ビット目: 成りの有無 (0: 成らない, 1: 成る)
 * - 15ビット目: 常に0（無効な着手の場合は1）
 *
 * 無効な着手の場合は負の値（通常は-1）が使用される。
 */
class Move {
 public:
  /**
   * 無効な着手オブジェクトを作成する。
   */
  Move();

  /**
   * 移動元と移動先の座標番号、成りの有無を指定してオブジェクトを作成する。
   * @param src 移動元の座標番号
   * @param dst 移動先の座標番号
   * @param promote 成りの有無
   */
  Move(const Position& src, const Position& dst, bool promote);

  /**
   * 着手番号を指定してオブジェクトを作成する。
   * @param move 着手番号
   */
  Move(int16_t move);

  /**
   * コピーコンストラクタ。
   */
  Move(const Move& move) = default;

  /**
   * オブジェクトを破棄する。
   */
  virtual ~Move() = default;

  /**
   * 着手の文字列表現を返す。
   * @return 着手の文字列表現。
   */
  std::string toString() const;

  /**
   * 移動元の座標を取得する。
   * @return 移動元の座標
   */
  inline Position getSrc() const {
    return Position((_move >> 7) & 0x7F);
  }

  /**
   * 移動先の座標を取得する。
   * @return 移動先の座標
   */
  inline Position getDst() const {
    return Position((_move >> 0) & 0x7F);
  }

  /**
   * 成りを含む着手であればtrueを返す。
   * @return 成りを含む着手であればtrue
   */
  inline bool isPromote() const {
    return (_move >> 14) & 0x1;
  }

  /**
   * 着手番号を返す。
   * @return 着手番号
   */
  inline int16_t getValue() const {
    return _move;
  }

  /**
   * 着手が有効であればtrueを返す。
   * @return 着手が有効であればtrue
   */
  inline bool isValid() const {
    return _move >= 0;
  }

  /**
   * 同じ着手であればtrueを返す。
   */
  inline bool operator==(const Move& move) const {
    return _move == move._move;
  }

  /**
   * 異なる着手であればtrueを返す。
   */
  inline bool operator!=(const Move& move) const {
    return _move != move._move;
  }

  /**
   * この着手が指定された着手よりも小さいならばtrueを返す。
   */
  inline bool operator<(const Move& move) const {
    return _move < move._move;
  }

  /**
   * 着手オブジェクトの文字列表現を出力ストリームに書き込む。
   * @param os 出力ストリーム。
   * @param move 着手オブジェクト。
   * @return 出力ストリーム。
   */
  friend std::ostream& operator<<(std::ostream& os, const Move& move) {
    os << move.toString();
    return os;
  }

 private:
  /**
   * この着手を表現する着手番号。
   */
  int16_t _move;
};

// 無効な着手を表す定数
const Move MOVE_INVALID(-1);

}  // namespace deepshogi

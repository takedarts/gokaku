#pragma once

#include "cshogi/cshogi.h"

namespace deepshogi {

/**
 * 着手の情報を管理するクラス。
 */
class Move {
 public:
  /**
   * 着手オブジェクトを作成する。
   */
  Move();

  /**
   * 着手オブジェクトを作成する。
   * @param srcX 移動元のX座標
   * @param srcY 移動元のY座標
   * @param dstX 移動先のX座標
   * @param dstY 移動先のY座標
   * @param promote 成るならtrue
   */
  Move(int32_t srcX, int32_t srcY, int32_t dstX, int32_t dstY, bool promote);

  /**
   * cshogiの着手表現から着手オブジェクトを作成する。
   * @param cshogiMove cshogiの着手表現
   */
  Move(int32_t cshogiMove);

  /**
   * コピーコンストラクタ。
   */
  Move(const Move& move) = default;

  /**
   * オブジェクトを破棄する。
   */
  virtual ~Move() = default;

  /**
   * 移動元のX座標を取得する。
   * @return X座標
   */
  int32_t getSrcX() const;

  /**
   * 移動元のY座標を取得する。
   * @return Y座標
   */
  int32_t getSrcY() const;

  /**
   * 移動先のX座標を取得する。
   * @return X座標
   */
  int32_t getDstX() const;

  /**
   * 移動先のY座標を取得する。
   * @return Y座標
   */
  int32_t getDstY() const;

  /**
   * 成るならtrueを返す。
   * @return 成るならtrue
   */
  bool isPromote() const;

  /**
   * パスの着手であればtrueを返す。
   * @return パスの着手であればtrue
   */
  bool isPass() const;

  /**
   * この着手を表現する数値を返す。
   * この数値はcshogiの着手表現`move16`と同じとなる。
   * @return 着手を表現する数値
   */
  int32_t getValue() const;

  /**
   * 同じ着手であればtrueを返す。
   */
  bool operator==(const Move& move) const;

  /**
   * 異なる着手であればtrueを返す。
   */
  bool operator!=(const Move& move) const;

  /**
   * この着手が指定された着手よりも小さいならばtrueを返す。
   */
  bool operator<(const Move& move) const;

 private:
  /**
   * 移動元のX座標。
   */
  int32_t _srcX;

  /**
   * 移動元のY座標。
   */
  int32_t _srcY;

  /**
   * 移動先のX座標。
   */
  int32_t _dstX;

  /**
   * 移動先のY座標。
   */
  int32_t _dstY;

  /**
   * 成るならtrue。
   */
  bool _promote;
};

// パスの着手オブジェクト。
const Move MOVE_PASS = Move(-1, -1, -1, -1, false);

}  // namespace deepshogi

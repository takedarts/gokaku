#include "Move.h"

#include "Config.h"

namespace deepshogi {

/**
 * 着手オブジェクトを作成する。
 */
Move::Move() : Move(-1, -1, -1, -1, false) {}

/**
 * 着手オブジェクトを作成する。
 * @param srcX 移動元のX座標
 * @param srcY 移動元のY座標
 * @param dstX 移動先のX座標
 * @param dstY 移動先のY座標
 * @param promote 成るならtrue
 */
Move::Move(int32_t srcX, int32_t srcY, int32_t dstX, int32_t dstY, bool promote)
    : _srcX(srcX), _srcY(srcY), _dstX(dstX), _dstY(dstY), _promote(promote) {
}

/**
 * cshogiの着手表現から着手オブジェクトを作成する。
 * @param cshogiMove cshogiの着手表現
 */
Move::Move(int32_t cshogiMove)
    : _srcX(((cshogiMove >> 7) & 0x7f) / BOARD_SIZE),
      _srcY(((cshogiMove >> 7) & 0x7f) % BOARD_SIZE),
      _dstX((cshogiMove & 0x7f) / BOARD_SIZE),
      _dstY((cshogiMove & 0x7f) % BOARD_SIZE),
      _promote(((cshogiMove >> 14) & 0x01) != 0) {
}

/**
 * 移動元のX座標を取得する。
 * @return X座標
 */
int32_t Move::getSrcX() const {
  return _srcX;
}

/**
 * 移動元のY座標を取得する。
 * @return Y座標
 */
int32_t Move::getSrcY() const {
  return _srcY;
}

/**
 * 移動先のX座標を取得する。
 * @return X座標
 */
int32_t Move::getDstX() const {
  return _dstX;
}

/**
 * 移動先のY座標を取得する。
 * @return Y座標
 */
int32_t Move::getDstY() const {
  return _dstY;
}

/**
 * 成るならtrueを返す。
 * @return 成るならtrue
 */
bool Move::isPromote() const {
  return _promote;
}

/**
 * パスの着手であればtrueを返す。
 * @return パスの着手であればtrue
 */
bool Move::isPass() const {
  return _srcX == -1;
}

/**
 * この着手を表現する数値を返す。
 * この数値はcshogiの着手表現`move16`と同じとなる。
 * @return 着手を表現する数値
 */
int32_t Move::getValue() const {
  return (
      ((_srcX * BOARD_SIZE + _srcY) << 7) |
      ((_dstX * BOARD_SIZE + _dstY) << 0) |
      (_promote << 14));
}

/**
 * 同じ着手であればtrueを返す。
 */
bool Move::operator==(const Move& move) const {
  return (
      _srcX == move._srcX &&
      _srcY == move._srcY &&
      _dstX == move._dstX &&
      _dstY == move._dstY &&
      _promote == move._promote);
}

/**
 * 異なる着手であればtrueを返す。
 */
bool Move::operator!=(const Move& move) const {
  return (
      _srcX != move._srcX ||
      _srcY != move._srcY ||
      _dstX != move._dstX ||
      _dstY != move._dstY ||
      _promote != move._promote);
}

/**
 * この着手が指定された着手よりも小さいならばtrueを返す。
 */
bool Move::operator<(const Move& move) const {
  return (
      _srcX < move._srcX ||
      _srcY < move._srcY ||
      _dstX < move._dstX ||
      _dstY < move._dstY ||
      _promote < move._promote);
}

}  // namespace deepshogi

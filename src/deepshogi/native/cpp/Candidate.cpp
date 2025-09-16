#include "Candidate.h"

namespace deepshogi {

/**
 * 候補手データを作成する。
 * @param move 着手
 * @param visits 訪問回数
 * @param playouts プレイアウト回数
 * @param policy 予想着手確率
 * @param value 予想勝率
 * @param variations 予想進行
 */
Candidate::Candidate(
    Move move, int32_t color, int32_t visits, int32_t playouts,
    float policy, float value, std::vector<Move> variations)
    : _move(move),
      _color(color),
      _visits(visits),
      _playouts(playouts),
      _policy(policy),
      _value(value),
      _variations(variations) {
}

/**
 * 候補手データを作成する。
 * @param move 着手
 * @param color 手番
 * @param visits 訪問回数
 * @param playouts プレイアウト回数
 * @param policy 予想着手確率
 * @param value 予想勝率
 */
Candidate::Candidate(
    Move move, int32_t color, int32_t visits, int32_t playouts,
    float policy, float value)
    : Candidate(move, color, visits, playouts, policy, value, std::vector<Move>()) {
  _variations.push_back(move);
}

/**
 * 着手を取得する。
 * @return 着手
 */
Move Candidate::getMove() const {
  return _move;
}

/**
 * 手番を取得する。
 * @return 手番
 */
int32_t Candidate::getColor() const {
  return _color;
}

/**
 * 訪問回数を取得する。
 * @return 訪問回数
 */
int32_t Candidate::getVisits() const {
  return _visits;
}

/**
 * プレイアウト回数を取得する。
 * @return プレイアウト回数
 */
int32_t Candidate::getPlayouts() const {
  return _playouts;
}

/**
 * 予想着手確率を取得する。
 * @return 予想着手確率
 */
float Candidate::getPolicy() const {
  return _policy;
}

/**
 * 予想勝率を取得する。
 * @return 予想勝率
 */
float Candidate::getValue() const {
  return _value;
}

/**
 * 予想進行を取得する。
 * @return 予想進行
 */
std::vector<Move> Candidate::getVariations() const {
  return _variations;
}

}  // namespace deepshogi

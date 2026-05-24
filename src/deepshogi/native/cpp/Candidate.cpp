#include "Candidate.h"

#include <sstream>

namespace deepshogi {

/**
 * 候補手データを作成する。
 * @param move 着手
 * @param visits 訪問回数
 * @param playouts プレイアウト回数
 * @param policy 予想着手確率
 * @param value 評価値
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
 * @param value 評価値
 */
Candidate::Candidate(
    Move move, int32_t color, int32_t visits, int32_t playouts,
    float policy, float value)
    : Candidate(
          move, color, visits, playouts,
          policy, value, std::vector<Move>()) {
  _variations.push_back(move);
}

/**
 * 候補手の文字列表現を返す。
 * @return 候補手の文字列表現。
 */
std::string Candidate::toString() const {
  std::stringstream ss;
  ss << "Move: " << _move.toString()
     << ", Color: " << ((_color == COLOR_BLACK) ? "Black" : "White")
     << ", Visits: " << _visits
     << ", Playouts: " << _playouts
     << ", Policy: " << _policy
     << ", Value: " << _value;

  if (!_variations.empty()) {
    ss << ", Variations: [";

    for (size_t i = 0; i < _variations.size(); ++i) {
      ss << _variations[i].toString();

      if (i < _variations.size() - 1) {
        ss << ", ";
      }
    }

    ss << "]";
  }

  return ss.str();
}

}  // namespace deepshogi

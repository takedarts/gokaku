#pragma once

#include <cstdint>
#include <ostream>
#include <vector>

#include "Move.h"

namespace deepshogi {

/**
 * 候補手クラス。
 */
class Candidate {
 public:
  /**
   * 候補手データを作成する。
   * @param move 着手
   * @param color 手番
   * @param visits 訪問回数
   * @param playouts プレイアウト回数
   * @param policy 予想着手確率
   * @param minimax ミニマックス評価値
   * @param variations 予想進行
   */
  Candidate(
      Move move, int32_t color, int32_t visits, int32_t playouts,
      float policy, float value, std::vector<Move> variations);

  /**
   * 候補手データを作成する。
   * @param move 着手
   * @param color 手番
   * @param visits 訪問回数
   * @param playouts プレイアウト回数
   * @param policy 予想着手確率
   * @param value 評価値
   */
  Candidate(
      Move move, int32_t color, int32_t visits, int32_t playouts,
      float policy, float value);

  /**
   * インスタンスを破棄する。
   */
  virtual ~Candidate() = default;

  /**
   * 候補手の文字列表現を返す。
   * @return 候補手の文字列表現。
   */
  std::string toString() const;

  /**
   * 着手を取得する。
   * @return 着手
   */
  inline Move getMove() const {
    return _move;
  }

  /**
   * 手番を取得する。
   * @return 手番
   */
  inline int32_t getColor() const {
    return _color;
  }

  /**
   * 訪問回数を取得する。
   * @return 訪問回数
   */
  inline int32_t getVisits() const {
    return _visits;
  }

  /**
   * プレイアウト回数を取得する。
   * @return プレイアウト回数
   */
  inline int32_t getPlayouts() const {
    return _playouts;
  }

  /**
   * 予想着手確率を取得する。
   * @return 予想着手確率
   */
  inline float getPolicy() const {
    return _policy;
  }

  /**
   * 評価値を取得する。
   * @return 評価値
   */
  inline float getValue() const {
    return _value;
  }

  /**
   * 予想進行を取得する。
   * @return 予想進行
   */
  inline std::vector<Move> getVariations() const {
    return _variations;
  }

  /**
   * 候補手の情報を出力ストリームに書き込む。
   * @param os 出力ストリーム。
   * @param candidate 候補手オブジェクト。
   * @return 出力ストリーム。
   */
  friend std::ostream& operator<<(std::ostream& os, const Candidate& candidate) {
    os << candidate.toString();
    return os;
  }

 private:
  /**
   * 着手。
   */
  Move _move;

  /**
   * 手番。
   */
  int32_t _color;

  /**
   * 訪問回数。
   */
  int32_t _visits;

  /**
   * プレイアウト回数。
   */
  int32_t _playouts;

  /**
   * 予想着手確率。
   */
  float _policy;

  /**
   * 評価値。
   */
  float _value;

  /**
   * 予想進行。
   */
  std::vector<Move> _variations;
};

}  // namespace deepshogi

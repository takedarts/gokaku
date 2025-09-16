#pragma once

#include <cstdint>
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
   * @param value 予想勝率
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
   * @param value 予想勝率
   */
  Candidate(
      Move move, int32_t color, int32_t visits, int32_t playouts,
      float policy, float value);

  /**
   * インスタンスを破棄する。
   */
  virtual ~Candidate() = default;

  /**
   * 着手を取得する。
   * @return 着手
   */
  Move getMove() const;

  /**
   * 手番を取得する。
   * @return 手番
   */
  int32_t getColor() const;

  /**
   * 訪問回数を取得する。
   * @return 訪問回数
   */
  int32_t getVisits() const;

  /**
   * プレイアウト回数を取得する。
   * @return プレイアウト回数
   */
  int32_t getPlayouts() const;

  /**
   * 予想着手確率を取得する。
   * @return 予想着手確率
   */
  float getPolicy() const;

  /**
   * 予想勝率を取得する。
   * @return 予想勝率
   */
  float getValue() const;

  /**
   * 予想進行を取得する。
   * @return 予想進行
   */
  std::vector<Move> getVariations() const;

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
   * 予想勝率。
   */
  float _value;

  /**
   * 予想進行。
   */
  std::vector<Move> _variations;
};

}  // namespace deepshogi

#pragma once

#include <cstdint>

#include "Move.h"

namespace deepshogi {

/**
 * 着手の結果を表すクラス。
 * このクラスは着手とその時に取られた駒の情報を保持する。
 * このクラスは盤面の状態を戻すために使用される。
 */
class MoveResult {
 public:
  /**
   * 着手結果オブジェクトを作成する。
   * 着手はMOVE_INVALIDに初期化される。
   * 取られた駒はPIECE_EMPTYに初期化される。
   */
  MoveResult();

  /**
   * 指定された着手と取られた駒の情報を保持するオブジェクトを作成する。
   * 取られた駒がない場合は取られた駒としてPIECE_EMPTYを指定する。
   * @param move 着手オブジェクト
   * @param captured 取られた駒の種類を表す整数値
   */
  MoveResult(const Move& move, uint8_t captured);

  /**
   * コピーコンストラクタはデフォルトの実装を使用する。
   */
  MoveResult(const MoveResult& other) = default;

  /**
   * オブジェクトを破棄する。
   */
  virtual ~MoveResult() = default;

  /**
   * 着手を取得する。
   * @return 着手を表すMoveオブジェクト
   */
  inline Move getMove() const {
    return _move;
  }

  /**
   * 取られた駒の種類を取得する。
   * 取られた駒がない場合はPIECE_EMPTYを返す。
   * @return 取られた駒の種類を表す整数値
   */
  inline uint8_t getCaptured() const {
    return _captured;
  }

 private:
  /**
   * 着手。
   */
  Move _move;

  /**
   * 着手の結果として取られた駒の種類。取られた駒がない場合はPIECE_EMPTY。
   */
  uint8_t _captured;
};

}  // namespace deepshogi

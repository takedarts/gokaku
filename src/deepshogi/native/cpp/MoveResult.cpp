#include "MoveResult.h"

#include "Config.h"

namespace deepshogi {

/**
 * 着手結果オブジェクトを作成する。
 * 着手はMOVE_INVALIDに初期化される。
 * 取られた駒はPIECE_EMPTYに初期化される。
 */
MoveResult::MoveResult()
    : _move(MOVE_INVALID),
      _captured(PIECE_EMPTY) {
}

/**
 * 指定された着手と取られた駒の情報を保持するオブジェクトを作成する。
 * 取られた駒がない場合は取られた駒としてPIECE_EMPTYを指定する。
 * @param move 着手オブジェクト
 * @param captured 取られた駒の種類を表す整数値
 */
MoveResult::MoveResult(const Move& move, uint8_t captured)
    : _move(move),
      _captured(captured) {
}

}  // namespace deepshogi

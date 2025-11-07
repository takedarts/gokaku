/*************************************************************************************************
 * 2025/04/29  Atsushi Takeda @takedarts
 * このファイルは cshogi : https://github.com/TadaoYamaoka/cshogi から一部機能を修正したものです。
 * 修正理由：
 *   深さ優先探索を用いて詰み手順を探しているが、詰み手順の長さを考慮していなかった。
 *   そのため、探索結果にしたがって着手を続けると千日手となる可能性があった。
 * 修正内容：
 *   詰み手順の長さを比較し、最も短い詰み手順を返すように修正した。
 *   具体的な修正点は以下の通り：
 *     mateMoveInOddPlyReturnMove：最小の詰み手数の着手を返す。詰まない場合は moveNone を返す。
 *     mateMoveInOddPly：最小の詰み手数を返すように変更。詰まない場合は「探索深さ+1」を返す。
 *     mateMoveInEvenPly：最大の詰み手数を返すように変更。詰まない場合は 0 を返す。
 *     3手詰め関数（mateMoveIn3Ply）を削除。
 *************************************************************************************************/
#include "mate.h"

#include "generateMoves.hpp"
#include "move.hpp"
#include "position.hpp"

namespace deepshogi::cshogi {

const constexpr size_t MaxCheckMoves = 91;

// 詰み探索用のMovePicker
template <bool or_node, bool INCHECK>
class MovePicker {
 public:
  explicit MovePicker(const Position& pos) {
    if (or_node) {
      last_ = generateMoves<CheckAll>(moveList_, pos);
      if (INCHECK) {
        // 自玉が王手の場合、逃げる手かつ王手をかける手を生成
        ExtMove* curr = moveList_;
        while (curr != last_) {
          if (!pos.checkMoveIsEvasion(curr->move))
            curr->move = (--last_)->move;
          else
            ++curr;
        }
      }
    } else {
      last_ = generateMoves<Evasion>(moveList_, pos);
      // 玉の移動による自殺手と、pinされている駒の移動による自殺手を削除
      ExtMove* curr = moveList_;
      const Bitboard pinned = pos.pinnedBB();
      while (curr != last_) {
        if (!pos.pseudoLegalMoveIsLegal<false, false>(curr->move, pinned))
          curr->move = (--last_)->move;
        else
          ++curr;
      }
    }
    assert(size() <= MaxCheckMoves);
  }
  size_t size() const { return static_cast<size_t>(last_ - moveList_); }
  ExtMove* begin() { return &moveList_[0]; }
  ExtMove* end() { return last_; }
  bool empty() const { return size() == 0; }

 private:
  ExtMove moveList_[MaxCheckMoves];
  ExtMove* last_;
};

// 奇数手詰めチェック
// 詰ます手を返す
template <bool INCHECK>
Move mateMoveInOddPlyReturnMove(Position& pos, const int depth) {
  // OR節点

  // すべての王手がかかる合法手について深さ優先探索を行う
  // 最も詰み手数の短い手を返す
  const CheckInfo ci(pos);
  Move min_check_move = Move::moveNone();
  int min_check_depth = depth + 1;

  for (const auto& ml : MovePicker<true, INCHECK>(pos)) {
    // 即詰みが見つかっていれば終了する
    if (min_check_depth == 1) {
      return min_check_move;
    }

    // 1手動かす
    StateInfo state;
    pos.doMove(ml.move, state, ci, true);

    // 千日手チェック
    switch (pos.isDraw(16)) {
      case NotRepetition:
        break;
      case RepetitionLose:  // 相手が負け
      {
        // 即詰みが見つかった時点で終了
        pos.undoMove(ml.move);
        return ml.move;
      }
      case RepetitionDraw:
      case RepetitionWin:       // 相手が勝ち
      case RepetitionSuperior:  // 相手が駒得
      {
        pos.undoMove(ml.move);
        continue;
      }
      case RepetitionInferior:
        break;  // 相手が駒損
      default:
        UNREACHABLE;
    }

    // std::cout << ml.move().toUSI() << std::endl;
    //  偶数手詰めチェック
    int check_depth = mateMoveInEvenPly(pos, depth - 1);

    if (check_depth > 0 && check_depth < min_check_depth) {
      min_check_move = ml.move;
      min_check_depth = check_depth;
    }

    pos.undoMove(ml.move);
  }

  return min_check_move;
}
template Move mateMoveInOddPlyReturnMove<true>(Position& pos, const int depth);
template Move mateMoveInOddPlyReturnMove<false>(Position& pos, const int depth);

// 奇数手詰めチェック
// 詰み手数を返す
// 詰まない場合は depth + 1 を返す
template <bool INCHECK>
int mateMoveInOddPly(Position& pos, const int depth) {
  // OR節点

  // すべての王手がかかる合法手について深さ優先探索を行う
  // 積み手数を depth + 1 に設定する
  const CheckInfo ci(pos);
  int min_check_depth = depth + 1;

  for (const auto& ml : MovePicker<true, INCHECK>(pos)) {
    // std::cout << depth << " : " << pos.toSFEN() << " : " << ml.move.toUSI() << std::endl;
    //  1手動かす
    StateInfo state;
    pos.doMove(ml.move, state, ci, true);

    // 千日手チェック
    switch (pos.isDraw(16)) {
      case NotRepetition:
        break;
      case RepetitionLose: {  // 相手が負け
        pos.undoMove(ml.move);
        return 1;  // 詰みとして扱う
      }
      case RepetitionDraw:        // 引き分け
      case RepetitionWin:         // 相手の勝ち
      case RepetitionSuperior: {  // 相手が駒得
        pos.undoMove(ml.move);
        continue;  // 不詰みとして扱う
      }
      case RepetitionInferior:
        break;  // 相手が駒損
      default:
        UNREACHABLE;
    }

    // 偶数手詰めチェック
    int check_depth = mateMoveInEvenPly(pos, depth - 1);

    if (check_depth > 0 && check_depth + 1 < min_check_depth) {
      min_check_depth = check_depth + 1;
    }

    pos.undoMove(ml.move);
  }

  return min_check_depth;
}

// 偶数手詰めチェック
// 手番側が王手されていること
// 詰みまでの手数を返す
// 詰まない場合は0を返す
int mateMoveInEvenPly(Position& pos, const int depth) {
  // AND節点

  // すべてのEvasionについて
  // 合法手が存在しないなら詰み扱いとするため max_check_depth を1にする
  const CheckInfo ci(pos);
  int max_check_depth = 1;

  for (const auto& ml : MovePicker<false, false>(pos)) {
    // std::cout << depth << " : " << pos.toSFEN() << " : " << ml.move.toUSI() << std::endl;
    const bool givesCheck = pos.moveGivesCheck(ml.move, ci);

    // 1手動かす
    StateInfo state;
    pos.doMove(ml.move, state, ci, givesCheck);

    // 千日手チェック
    switch (pos.isDraw(16)) {
      case NotRepetition:
        break;
      case RepetitionWin: {  // 自分が勝ち
        pos.undoMove(ml.move);
        continue;  // 詰みとして扱う
      }
      case RepetitionDraw:        // 引き分け
      case RepetitionLose:        // 自分が負け
      case RepetitionInferior: {  // 自分が駒損
        pos.undoMove(ml.move);
        return 0;  // 詰まない
      }
      case RepetitionSuperior:  // 自分が駒得
        break;
      default:
        UNREACHABLE;
    }

    // 探索深さが2の場合は1手詰めのみをチェックする
    if (depth == 2) {
      // この指し手で逆王手になるなら、不詰めとして扱う
      if (givesCheck) {
        pos.undoMove(ml.move);
        return 0;
      }

      // 1手詰めで詰みが見つからなかった時点で終了
      if (!pos.mateMoveIn1Ply()) {
        pos.undoMove(ml.move);
        return 0;
      }
    }
    // 探索深さが4以上の場合は奇数手詰めをチェックする
    else {
      // 奇数手詰めをチェックする
      int check_depth =
          givesCheck ? mateMoveInOddPly<true>(pos, depth - 1)
                     : mateMoveInOddPly<false>(pos, depth - 1);

      // 詰みが見つからなかった場合は終了
      if (check_depth > depth - 1) {
        pos.undoMove(ml.move);
        return 0;
      }

      // 詰みが見つかった場合は詰み手数を更新する
      if (check_depth + 1 > max_check_depth) {
        max_check_depth = check_depth + 1;
      }
    }

    pos.undoMove(ml.move);
  }

  return max_check_depth;
}

}  // namespace deepshogi::cshogi

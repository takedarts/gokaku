#pragma once

#include "move.hpp"
#include "position.hpp"

namespace deepshogi::cshogi {

// 奇数手詰めチェック
// 詰ます手を返すバージョン
template <bool INCHECK = false>
Move mateMoveInOddPlyReturnMove(Position& pos, const int depth);

// 奇数手詰めチェック
template <bool INCHECK = false>
int mateMoveInOddPly(Position& pos, const int depth);

// 偶数手詰めチェック
// 手番側が王手されていること
int mateMoveInEvenPly(Position& pos, const int depth);

}  // namespace deepshogi::cshogi

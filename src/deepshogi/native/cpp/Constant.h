#pragma once

#include "BitBoard.h"

namespace deepshogi {

// 敵陣を表すビットボード
extern const BitBoard BITBOARD_ENEMY_AREAS[2];

// 歩を打てる場所を表すビットボード
extern const BitBoard BITBOARD_PAWN_DROPABLES[2];

// 桂を打てる場所を表すビットボード
extern const BitBoard BITBOARD_KNIGHT_DROPABLES[2];

// 歩の利きを表すビットボード
extern const BitBoard BITBOARD_PAWN_ATTACKS[2][81];

// 香の利きを表すビットボード
extern const BitBoard BITBOARD_LANCE_ATTACKS[2][81];

// 桂の利きを表すビットボード
extern const BitBoard BITBOARD_KNIGHT_ATTACKS[2][81];

// 銀の利きを表すビットボード
extern const BitBoard BITBOARD_SILVER_ATTACKS[2][81];

// 金の利きを表すビットボード
extern const BitBoard BITBOARD_GOLD_ATTACKS[2][81];

// 王の利きを表すビットボード
extern const BitBoard BITBOARD_KING_ATTACKS[81];

// 長い利きを表すビットボード
// 以下の方向の利きを表すビットボードを格納している
// 0:上, 1:右上, 2:右, 3:右下, 4:下, 5:左下, 6:左, 7:左上
extern const BitBoard BITBOARD_LONG_ATTACKS[8][81];

// 方向のインデックスを表す定数
// 2つの座標の方向を表す整数値を格納している
// 0:上, 1:右上, 2:右, 3:右下, 4:下, 5:左下, 6:左, 7:左上, 8:その他
extern const int DIRECTION_INDICES[81][81];

// 盤面のハッシュ値を計算するための定数
// 手番、位置、駒の種類に対してランダムな64ビット整数値を格納している
extern const uint64_t BOARD_HASH_VALUES[81][31];

}  //  namespace deepshogi

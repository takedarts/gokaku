#pragma once

#include "BitBoard.h"

namespace deepshogi {

// Bitboard representing the enemy territory
extern const BitBoard BITBOARD_ENEMY_AREAS[2];

// Bitboard representing squares where a pawn can be dropped
extern const BitBoard BITBOARD_PAWN_DROPABLES[2];

// Bitboard representing squares where a knight can be dropped
extern const BitBoard BITBOARD_KNIGHT_DROPABLES[2];

// Bitboard representing pawn attack squares
extern const BitBoard BITBOARD_PAWN_ATTACKS[2][81];

// Bitboard representing lance attack squares
extern const BitBoard BITBOARD_LANCE_ATTACKS[2][81];

// Bitboard representing knight attack squares
extern const BitBoard BITBOARD_KNIGHT_ATTACKS[2][81];

// Bitboard representing silver attack squares
extern const BitBoard BITBOARD_SILVER_ATTACKS[2][81];

// Bitboard representing gold attack squares
extern const BitBoard BITBOARD_GOLD_ATTACKS[2][81];

// Bitboard representing king attack squares
extern const BitBoard BITBOARD_KING_ATTACKS[81];

// Bitboard representing long-range attack squares
// Stores bitboards for attacks in the following directions:
// 0:up, 1:upper-right, 2:right, 3:lower-right, 4:down, 5:lower-left, 6:left, 7:upper-left
extern const BitBoard BITBOARD_LONG_ATTACKS[8][81];

// Constants representing direction indices
// Stores integer values representing the direction between two coordinates
// 0:up, 1:upper-right, 2:right, 3:lower-right, 4:down, 5:lower-left, 6:left, 7:upper-left, 8:other
extern const int DIRECTION_INDICES[81][81];

// Constants for computing the board hash value
// Stores random 64-bit integer values for each turn, position, and piece type
extern const uint64_t BOARD_HASH_VALUES[81][31];

}  //  namespace deepshogi

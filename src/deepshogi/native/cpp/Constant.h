#pragma once

#include "BitBoard.h"

namespace deepshogi {

// BitBoard representing enemy territory
extern const BitBoard BITBOARD_ENEMY_AREAS[2];

// BitBoard representing squares where pawns can be dropped
extern const BitBoard BITBOARD_PAWN_DROPABLES[2];

// BitBoard representing squares where knights can be dropped
extern const BitBoard BITBOARD_KNIGHT_DROPABLES[2];

// BitBoard representing pawn attacks
extern const BitBoard BITBOARD_PAWN_ATTACKS[2][81];

// BitBoard representing lance attacks
extern const BitBoard BITBOARD_LANCE_ATTACKS[2][81];

// BitBoard representing knight attacks
extern const BitBoard BITBOARD_KNIGHT_ATTACKS[2][81];

// BitBoard representing silver attacks
extern const BitBoard BITBOARD_SILVER_ATTACKS[2][81];

// BitBoard representing gold attacks
extern const BitBoard BITBOARD_GOLD_ATTACKS[2][81];

// BitBoard representing king attacks
extern const BitBoard BITBOARD_KING_ATTACKS[81];

// BitBoard representing long-range attacks
// The following directions are represented in the bitboards
// 0: up, 1: up-right, 2: right, 3: down-right, 4: down, 5: down-left, 6: left, 7: up-left
extern const BitBoard BITBOARD_LONG_ATTACKS[8][81];

// Constants representing direction indices
// These integers represent the direction between two coordinates
// 0: up, 1: up-right, 2: right, 3: down-right, 4: down, 5: down-left, 6: left, 7: up-left, 8: other
extern const int DIRECTION_INDICES[81][81];

// Constants for calculating board hash values
// Random 64-bit integers for each combination of turn, position, and piece type
extern const uint64_t BOARD_HASH_VALUES[81][30];

// Constants for calculating hand hash values
// Random 64-bit integers for each combination of turn, piece type, and piece count
extern const uint64_t HAND_HASH_VALUES[2][7][19];

}  //  namespace deepshogi

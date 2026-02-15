#include "Board.h"

#include <string.h>

#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>

#include "Config.h"

namespace deepshogi {

// Array of piece types in hand pieces in SFEN format
static const uint8_t SFEN_HAND_PIECES[] = {
    PIECE_HAND_ROOK, PIECE_HAND_BISHOP, PIECE_HAND_GOLD,
    PIECE_HAND_SILVER, PIECE_HAND_KNIGHT, PIECE_HAND_LANCE, PIECE_HAND_PAWN};

// Array of piece names in hand pieces in SFEN format
static const char* SFEN_HAND_BLACK_NAMES[] = {"R", "B", "G", "S", "N", "L", "P"};
static const char* SFEN_HAND_WHITE_NAMES[] = {"r", "b", "g", "s", "n", "l", "p"};

/**
 * Set the specified bit.
 * @param inputs Bit sequence
 * @param index Position of the bit to set
 */
inline void setInputBit(int32_t* inputs, int32_t index, int32_t value = 1) {
  inputs[index / 32] |= (value << (index % 32));
}

/**
 * Determine whether a piece can move to the specified coordinates.
 * @param piece Type of piece
 * @param src_x X coordinate of the source position
 * @param src_y Y coordinate of the source position
 * @param dst_x X coordinate of the destination position
 * @param dst_y Y coordinate of the destination position
 * @return True if the piece can move
 */
inline bool isMovable(
    uint8_t piece, int32_t src_x, int32_t src_y, int32_t dst_x, int32_t dst_y) {
  if (src_x == dst_x && src_y == dst_y) {
    return false;
  }

  if (piece == PIECE_BLACK_PAWN) {
    if (src_x == dst_x && src_y - 1 == dst_y) {
      return true;
    }
  } else if (piece == PIECE_BLACK_LANCE) {
    if (src_x == dst_x && src_y > dst_y) {
      return true;
    }
  } else if (piece == PIECE_BLACK_KNIGHT) {
    if ((src_x - 1 == dst_x && src_y - 2 == dst_y) ||
        (src_x + 1 == dst_x && src_y - 2 == dst_y)) {
      return true;
    }
  } else if (piece == PIECE_BLACK_SILVER) {
    if ((src_x == dst_x && src_y - 1 == dst_y) ||
        (src_x - 1 == dst_x && src_y - 1 == dst_y) ||
        (src_x + 1 == dst_x && src_y - 1 == dst_y) ||
        (src_x - 1 == dst_x && src_y + 1 == dst_y) ||
        (src_x + 1 == dst_x && src_y + 1 == dst_y)) {
      return true;
    }
  } else if ((piece == PIECE_BLACK_GOLD) ||
             (piece == PIECE_BLACK_PRO_PAWN) ||
             (piece == PIECE_BLACK_PRO_LANCE) ||
             (piece == PIECE_BLACK_PRO_KNIGHT) ||
             (piece == PIECE_BLACK_PRO_SILVER)) {
    if ((src_x == dst_x && src_y - 1 == dst_y) ||
        (src_x - 1 == dst_x && src_y - 1 == dst_y) ||
        (src_x + 1 == dst_x && src_y - 1 == dst_y) ||
        (src_x - 1 == dst_x && src_y == dst_y) ||
        (src_x + 1 == dst_x && src_y == dst_y) ||
        (src_x == dst_x && src_y + 1 == dst_y)) {
      return true;
    }
  } else if (piece == PIECE_WHITE_PAWN) {
    if (src_x == dst_x && src_y + 1 == dst_y) {
      return true;
    }
  } else if (piece == PIECE_WHITE_LANCE) {
    if (src_x == dst_x && src_y < dst_y) {
      return true;
    }
  } else if (piece == PIECE_WHITE_KNIGHT) {
    if ((src_x - 1 == dst_x && src_y + 2 == dst_y) ||
        (src_x + 1 == dst_x && src_y + 2 == dst_y)) {
      return true;
    }
  } else if (piece == PIECE_WHITE_SILVER) {
    if ((src_x == dst_x && src_y + 1 == dst_y) ||
        (src_x - 1 == dst_x && src_y + 1 == dst_y) ||
        (src_x + 1 == dst_x && src_y + 1 == dst_y) ||
        (src_x - 1 == dst_x && src_y - 1 == dst_y) ||
        (src_x + 1 == dst_x && src_y - 1 == dst_y)) {
      return true;
    }
  } else if ((piece == PIECE_WHITE_GOLD) ||
             (piece == PIECE_WHITE_PRO_PAWN) ||
             (piece == PIECE_WHITE_PRO_LANCE) ||
             (piece == PIECE_WHITE_PRO_KNIGHT) ||
             (piece == PIECE_WHITE_PRO_SILVER)) {
    if ((src_x == dst_x && src_y + 1 == dst_y) ||
        (src_x - 1 == dst_x && src_y + 1 == dst_y) ||
        (src_x + 1 == dst_x && src_y + 1 == dst_y) ||
        (src_x - 1 == dst_x && src_y == dst_y) ||
        (src_x + 1 == dst_x && src_y == dst_y) ||
        (src_x == dst_x && src_y - 1 == dst_y)) {
      return true;
    }
  } else if ((piece == PIECE_BLACK_BISHOP) ||
             (piece == PIECE_WHITE_BISHOP)) {
    if (abs(src_x - dst_x) == abs(src_y - dst_y)) {
      return true;
    }
  } else if ((piece == PIECE_BLACK_ROOK) ||
             (piece == PIECE_WHITE_ROOK)) {
    if (src_x == dst_x || src_y == dst_y) {
      return true;
    }
  } else if ((piece == PIECE_BLACK_HORSE) ||
             (piece == PIECE_WHITE_HORSE)) {
    if (abs(src_x - dst_x) == abs(src_y - dst_y) ||
        (abs(src_x - dst_x) <= 1 && abs(src_y - dst_y) <= 1)) {
      return true;
    }
  } else if ((piece == PIECE_BLACK_DRAGON) ||
             (piece == PIECE_WHITE_DRAGON)) {
    if (src_x == dst_x || src_y == dst_y ||
        (abs(src_x - dst_x) <= 1 && abs(src_y - dst_y) <= 1)) {
      return true;
    }
  } else if ((piece == PIECE_BLACK_KING) ||
             (piece == PIECE_WHITE_KING)) {
    if (abs(src_x - dst_x) <= 1 && abs(src_y - dst_y) <= 1) {
      return true;
    }
  }

  return false;
}

/**
 * Get a list of coordinates of pieces attacking the specified square.
 * @param cells Piece information for each square on the board
 * @param x X coordinate
 * @param y Y coordinate
 * @return List of coordinates of attacking pieces
 */
static std::vector<std::pair<int32_t, int32_t>> getAttackerPositions(
    const uint8_t cells[BOARD_SIZE][BOARD_SIZE], int32_t x, int32_t y) {
  std::vector<std::pair<int32_t, int32_t>> attackers;

  // Search for pieces in the upward direction
  std::pair<int32_t, int32_t> upper = std::make_pair(-1, -1);

  for (int32_t dy = y - 1; dy >= 0; dy--) {
    if (cells[dy][x] != PIECE_EMPTY) {
      upper = std::make_pair(x, dy);
      break;
    }
  }

  if (upper.first != -1) {
    uint8_t piece = cells[upper.second][upper.first];

    if (isMovable(piece, upper.first, upper.second, x, y)) {
      attackers.push_back(upper);
    }
  }

  // Search for pieces in the downward direction
  std::pair<int32_t, int32_t> lower = std::make_pair(-1, -1);

  for (int32_t dy = y + 1; dy < BOARD_SIZE; dy++) {
    if (cells[dy][x] != PIECE_EMPTY) {
      lower = std::make_pair(x, dy);
      break;
    }
  }

  if (lower.first != -1) {
    uint8_t piece = cells[lower.second][lower.first];

    if (isMovable(piece, lower.first, lower.second, x, y)) {
      attackers.push_back(lower);
    }
  }

  // Search for pieces in the left direction
  std::pair<int32_t, int32_t> left = std::make_pair(-1, -1);

  for (int32_t dx = x - 1; dx >= 0; dx--) {
    if (cells[y][dx] != PIECE_EMPTY) {
      left = std::make_pair(dx, y);
      break;
    }
  }

  if (left.first != -1) {
    uint8_t piece = cells[left.second][left.first];

    if (isMovable(piece, left.first, left.second, x, y)) {
      attackers.push_back(left);
    }
  }

  // Search for pieces in the right direction
  std::pair<int32_t, int32_t> right = std::make_pair(-1, -1);

  for (int32_t dx = x + 1; dx < BOARD_SIZE; dx++) {
    if (cells[y][dx] != PIECE_EMPTY) {
      right = std::make_pair(dx, y);
      break;
    }
  }

  if (right.first != -1) {
    uint8_t piece = cells[right.second][right.first];

    if (isMovable(piece, right.first, right.second, x, y)) {
      attackers.push_back(right);
    }
  }

  // Search for pieces in the upper-left direction
  std::pair<int32_t, int32_t> upper_left = std::make_pair(-1, -1);

  for (int32_t d = 1; x - d >= 0 && y - d >= 0; d++) {
    if (cells[y - d][x - d] != PIECE_EMPTY) {
      upper_left = std::make_pair(x - d, y - d);
      break;
    }
  }

  if (upper_left.first != -1) {
    uint8_t piece = cells[upper_left.second][upper_left.first];

    if (isMovable(piece, upper_left.first, upper_left.second, x, y)) {
      attackers.push_back(upper_left);
    }
  }

  // Search for pieces in the upper-right direction
  std::pair<int32_t, int32_t> upper_right = std::make_pair(-1, -1);

  for (int32_t d = 1; x + d < BOARD_SIZE && y - d >= 0; d++) {
    if (cells[y - d][x + d] != PIECE_EMPTY) {
      upper_right = std::make_pair(x + d, y - d);
      break;
    }
  }

  if (upper_right.first != -1) {
    uint8_t piece = cells[upper_right.second][upper_right.first];

    if (isMovable(piece, upper_right.first, upper_right.second, x, y)) {
      attackers.push_back(upper_right);
    }
  }

  // Search for pieces in the lower-left direction
  std::pair<int32_t, int32_t> lower_left = std::make_pair(-1, -1);

  for (int32_t d = 1; x - d >= 0 && y + d < BOARD_SIZE; d++) {
    if (cells[y + d][x - d] != PIECE_EMPTY) {
      lower_left = std::make_pair(x - d, y + d);
      break;
    }
  }

  if (lower_left.first != -1) {
    uint8_t piece = cells[lower_left.second][lower_left.first];

    if (isMovable(piece, lower_left.first, lower_left.second, x, y)) {
      attackers.push_back(lower_left);
    }
  }

  // Search for pieces in the lower-right direction
  std::pair<int32_t, int32_t> lower_right = std::make_pair(-1, -1);

  for (int32_t d = 1; x + d < BOARD_SIZE && y + d < BOARD_SIZE; d++) {
    if (cells[y + d][x + d] != PIECE_EMPTY) {
      lower_right = std::make_pair(x + d, y + d);
      break;
    }
  }

  if (lower_right.first != -1) {
    uint8_t piece = cells[lower_right.second][lower_right.first];

    if (isMovable(piece, lower_right.first, lower_right.second, x, y)) {
      attackers.push_back(lower_right);
    }
  }

  // Search for pieces in the knight's move directions
  if (x - 1 >= 0 && y - 2 >= 0 &&
      isMovable(cells[y - 2][x - 1], x - 1, y - 2, x, y)) {
    attackers.push_back(std::make_pair(x - 1, y - 2));
  }

  if (x + 1 < BOARD_SIZE && y - 2 >= 0 &&
      isMovable(cells[y - 2][x + 1], x + 1, y - 2, x, y)) {
    attackers.push_back(std::make_pair(x + 1, y - 2));
  }

  if (x - 1 >= 0 && y + 2 < BOARD_SIZE &&
      isMovable(cells[y + 2][x - 1], x - 1, y + 2, x, y)) {
    attackers.push_back(std::make_pair(x - 1, y + 2));
  }

  if (x + 1 < BOARD_SIZE && y + 2 < BOARD_SIZE &&
      isMovable(cells[y + 2][x + 1], x + 1, y + 2, x, y)) {
    attackers.push_back(std::make_pair(x + 1, y + 2));
  }

  return attackers;
}

/**
 * Determines if the specified position is under attack by the opponent's pieces.
 * @param cells Piece information for each square on the board
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Color of the current player
 * @return True if the position is under attack, false otherwise
 */
static bool isCheckedPosition(
    const uint8_t cells[BOARD_SIZE][BOARD_SIZE],
    int32_t x, int32_t y, int32_t color) {
  // Get a list of attacking pieces
  std::vector<std::pair<int32_t, int32_t>> attackers = getAttackerPositions(cells, x, y);

  // Check if any of the attacking pieces belong to the opponent
  for (const auto& pos : attackers) {
    uint8_t piece = cells[pos.second][pos.first];

    if (color == COLOR_BLACK &&
        PIECE_WHITE_BEGIN <= piece && piece < PIECE_WHITE_END) {
      return true;
    } else if (color == COLOR_WHITE &&
               PIECE_BLACK_BEGIN <= piece && piece < PIECE_BLACK_END) {
      return true;
    }
  }

  return false;
}

/**
 * Gets a list of legal moves that can be made with the specified player's hand pieces.
 * Implemented for the black player, so for the white player, pass a flipped board.
 * @param cells Piece information for each square on the board
 * @param hands Information about the player's hand pieces
 * @return List of legal moves
 */
static std::vector<Move> getLegalHandMoves(
    const uint8_t cells[BOARD_SIZE][BOARD_SIZE],
    const uint8_t hands[PIECE_HAND_END - PIECE_HAND_BEGIN]) {
  std::vector<Move> legal_moves;

  // Check which columns have pawns already present
  uint8_t pawn_exists[BOARD_SIZE] = {0};

  for (int32_t y = 0; y < BOARD_SIZE; y++) {
    for (int32_t x = 0; x < BOARD_SIZE; x++) {
      uint8_t piece = cells[y][x];

      if (piece == PIECE_BLACK_PAWN) {
        pawn_exists[x] = 1;
      }
    }
  }

  // Add moves for dropping pawns from the hand
  int32_t hand_pawn_num = hands[PIECE_HAND_PAWN - PIECE_HAND_BEGIN];

  if (hand_pawn_num > 0) {
    for (int32_t x = 0; x < BOARD_SIZE; x++) {
      // Cannot drop a pawn in a column that already has one
      if (pawn_exists[x] > 0) {
        continue;
      }

      for (int32_t y = 1; y < BOARD_SIZE; y++) {
        uint8_t piece = cells[y][x];

        // Can only drop on an empty square
        if (piece != PIECE_EMPTY) {
          continue;
        }

        // If dropping a pawn would result in checkmate,
        // ensure it is not an illegal pawn drop
        if (cells[y - 1][x] == PIECE_WHITE_KING) {
          // Create a board after dropping the pawn
          uint8_t temp_cells[BOARD_SIZE][BOARD_SIZE];

          memcpy(temp_cells, cells, sizeof(temp_cells));
          temp_cells[y][x] = PIECE_BLACK_PAWN;

          // Check if the king's escape squares are under attack
          bool rescue_move_found = false;
          const std::vector<std::pair<int32_t, int32_t>> directions = {
              {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
          for (const auto& dir : directions) {
            int32_t dst_x = x + dir.first;
            int32_t dst_y = y - 1 + dir.second;

            if (0 <= dst_x && dst_x < BOARD_SIZE && 0 <= dst_y && dst_y < BOARD_SIZE) {
              uint8_t dst_piece = temp_cells[dst_y][dst_x];

              if ((dst_piece < PIECE_WHITE_BEGIN || PIECE_WHITE_END <= dst_piece) &&
                  !isCheckedPosition(temp_cells, dst_x, dst_y, COLOR_WHITE)) {
                rescue_move_found = true;
                break;
              }
            }
          }

          // If the opponent's king cannot escape, it would be an illegal pawn drop
          if (!rescue_move_found) {
            continue;
          }
        }

        // Add the move for dropping the pawn
        legal_moves.emplace_back(BOARD_SIZE, PIECE_HAND_PAWN, x, y, false);
      }
    }
  }

  // Add moves for dropping lances from the hand
  int32_t hand_lance_num = hands[PIECE_HAND_LANCE - PIECE_HAND_BEGIN];

  if (hand_lance_num > 0) {
    for (int32_t y = 1; y < BOARD_SIZE; y++) {
      for (int32_t x = 0; x < BOARD_SIZE; x++) {
        uint8_t piece = cells[y][x];

        // Can only drop on an empty square
        if (piece != PIECE_EMPTY) {
          continue;
        }

        // Add the move for dropping the lance
        legal_moves.emplace_back(BOARD_SIZE, PIECE_HAND_LANCE, x, y, false);
      }
    }
  }

  // Add moves for dropping knights from the hand
  int32_t hand_knight_num = hands[PIECE_HAND_KNIGHT - PIECE_HAND_BEGIN];

  if (hand_knight_num > 0) {
    for (int32_t y = 2; y < BOARD_SIZE; y++) {
      for (int32_t x = 0; x < BOARD_SIZE; x++) {
        uint8_t piece = cells[y][x];

        // Can only drop on an empty square
        if (piece != PIECE_EMPTY) {
          continue;
        }

        // Add the move for dropping the knight
        legal_moves.emplace_back(BOARD_SIZE, PIECE_HAND_KNIGHT, x, y, false);
      }
    }
  }

  // Add moves for dropping silver, gold, bishop, and rook from the hand
  for (int32_t piece : {PIECE_HAND_SILVER, PIECE_HAND_GOLD,
                        PIECE_HAND_BISHOP, PIECE_HAND_ROOK}) {
    int32_t hand_piece_num = hands[piece - PIECE_HAND_BEGIN];

    if (hand_piece_num > 0) {
      for (int32_t y = 0; y < BOARD_SIZE; y++) {
        for (int32_t x = 0; x < BOARD_SIZE; x++) {
          uint8_t board_piece = cells[y][x];

          // Can only drop on an empty square
          if (board_piece != PIECE_EMPTY) {
            continue;
          }

          // Add the move for dropping the piece
          legal_moves.emplace_back(BOARD_SIZE, piece, x, y, false);
        }
      }
    }
  }

  return legal_moves;
}

/**
 * Get a list of legal moves that can be made on the specified board.
 * Implemented for black's turn, so if it's white's turn, pass a flipped board.
 * @param cells The piece information for each square on the board
 * @param removeUnpromote If true, remove moves that do not promote pawns, bishops, or rooks
 * @return A list of legal moves
 */
static std::vector<Move> getLegalBoardMoves(
    const uint8_t cells[BOARD_SIZE][BOARD_SIZE], bool removeUnpromote) {
  std::vector<Move> legal_moves;

  // Add moves for moving pieces on the board
  for (int32_t y = 0; y < BOARD_SIZE; y++) {
    for (int32_t x = 0; x < BOARD_SIZE; x++) {
      uint8_t piece = cells[y][x];

      // Skip if it's not my piece
      if (piece < PIECE_BLACK_BEGIN || PIECE_BLACK_END <= piece) {
        continue;
      }

      // Add moves for moving pawns
      if (piece == PIECE_BLACK_PAWN) {
        if (y > 0) {
          uint8_t dst_piece = cells[y - 1][x];

          if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
            continue;
          } else if (y - 1 == 0) {
            legal_moves.emplace_back(x, y, x, y - 1, true);
          } else if (y - 1 > 2) {
            legal_moves.emplace_back(x, y, x, y - 1, false);
          } else if (removeUnpromote) {
            legal_moves.emplace_back(x, y, x, y - 1, true);
          } else {
            legal_moves.emplace_back(x, y, x, y - 1, false);
            legal_moves.emplace_back(x, y, x, y - 1, true);
          }
        }
      }
      // Add moves for moving lances
      else if (piece == PIECE_BLACK_LANCE) {
        for (int32_t dy = y - 1; dy >= 0; dy--) {
          uint8_t dst_piece = cells[dy][x];

          if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
            break;
          } else if (dy == 0) {
            legal_moves.emplace_back(x, y, x, dy, true);
          } else if (dy > 2) {
            legal_moves.emplace_back(x, y, x, dy, false);
          } else {
            legal_moves.emplace_back(x, y, x, dy, false);
            legal_moves.emplace_back(x, y, x, dy, true);
          }

          if (dst_piece != PIECE_EMPTY) {
            break;
          }
        }
      }
      // Add moves for moving knights
      else if (piece == PIECE_BLACK_KNIGHT) {
        for (int32_t dx : {-1, 1}) {
          int32_t dst_x = x + dx;
          int32_t dst_y = y - 2;

          if (dst_x < 0 || BOARD_SIZE <= dst_x ||
              dst_y < 0 || BOARD_SIZE <= dst_y) {
            continue;
          }

          uint8_t dst_piece = cells[dst_y][dst_x];

          if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
            continue;
          } else if (dst_y < 2) {
            legal_moves.emplace_back(x, y, dst_x, dst_y, true);
          } else if (dst_y > 2) {
            legal_moves.emplace_back(x, y, dst_x, dst_y, false);
          } else {
            legal_moves.emplace_back(x, y, dst_x, dst_y, false);
            legal_moves.emplace_back(x, y, dst_x, dst_y, true);
          }
        }
      }
      // Add moves for moving silver generals
      else if (piece == PIECE_BLACK_SILVER) {
        const std::vector<std::pair<int32_t, int32_t>> directions = {
            {-1, -1}, {0, -1}, {1, -1}, {-1, 1}, {1, 1}};
        for (const auto& dir : directions) {
          int32_t dst_x = x + dir.first;
          int32_t dst_y = y + dir.second;

          if (dst_x < 0 || BOARD_SIZE <= dst_x ||
              dst_y < 0 || BOARD_SIZE <= dst_y) {
            continue;
          }

          uint8_t dst_piece = cells[dst_y][dst_x];

          if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
            continue;
          } else if (y > 2 && dst_y > 2) {
            legal_moves.emplace_back(x, y, dst_x, dst_y, false);
          } else {
            legal_moves.emplace_back(x, y, dst_x, dst_y, true);
            legal_moves.emplace_back(x, y, dst_x, dst_y, false);
          }
        }
      }
      // Add moves for moving rooks and dragons
      else if (piece == PIECE_BLACK_ROOK || piece == PIECE_BLACK_DRAGON) {
        const std::vector<std::pair<int32_t, int32_t>> directions = {
            {0, -1}, {1, 0}, {0, 1}, {-1, 0}};
        for (const auto& dir : directions) {
          int32_t dst_x = x;
          int32_t dst_y = y;

          while (true) {
            dst_x += dir.first;
            dst_y += dir.second;

            if (dst_x < 0 || BOARD_SIZE <= dst_x ||
                dst_y < 0 || BOARD_SIZE <= dst_y) {
              break;
            }

            uint8_t dst_piece = cells[dst_y][dst_x];

            if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
              break;
            } else if ((y > 2 && dst_y > 2) || piece == PIECE_BLACK_DRAGON) {
              legal_moves.emplace_back(x, y, dst_x, dst_y, false);
            } else if (removeUnpromote) {
              legal_moves.emplace_back(x, y, dst_x, dst_y, true);
            } else {
              legal_moves.emplace_back(x, y, dst_x, dst_y, true);
              legal_moves.emplace_back(x, y, dst_x, dst_y, false);
            }

            if (dst_piece != PIECE_EMPTY) {
              break;
            }
          }
        }

        if (piece == PIECE_BLACK_DRAGON) {
          const std::vector<std::pair<int32_t, int32_t>> diag_directions = {
              {-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
          for (const auto& dir : diag_directions) {
            int32_t dst_x = x + dir.first;
            int32_t dst_y = y + dir.second;

            if (dst_x < 0 || BOARD_SIZE <= dst_x ||
                dst_y < 0 || BOARD_SIZE <= dst_y) {
              continue;
            }

            uint8_t dst_piece = cells[dst_y][dst_x];

            if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
              continue;
            } else {
              legal_moves.emplace_back(x, y, dst_x, dst_y, false);
            }
          }
        }
      }
      // Add moves for moving bishops and horses
      else if (piece == PIECE_BLACK_BISHOP || piece == PIECE_BLACK_HORSE) {
        const std::vector<std::pair<int32_t, int32_t>> directions = {
            {-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        for (const auto& dir : directions) {
          int32_t dst_x = x;
          int32_t dst_y = y;

          while (true) {
            dst_x += dir.first;
            dst_y += dir.second;

            if (dst_x < 0 || BOARD_SIZE <= dst_x ||
                dst_y < 0 || BOARD_SIZE <= dst_y) {
              break;
            }

            uint8_t dst_piece = cells[dst_y][dst_x];

            if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
              break;
            } else if ((y > 2 && dst_y > 2) || piece == PIECE_BLACK_HORSE) {
              legal_moves.emplace_back(x, y, dst_x, dst_y, false);
            } else if (removeUnpromote) {
              legal_moves.emplace_back(x, y, dst_x, dst_y, true);
            } else {
              legal_moves.emplace_back(x, y, dst_x, dst_y, true);
              legal_moves.emplace_back(x, y, dst_x, dst_y, false);
            }

            if (dst_piece != PIECE_EMPTY) {
              break;
            }
          }
        }

        if (piece == PIECE_BLACK_HORSE) {
          const std::vector<std::pair<int32_t, int32_t>> ortho_directions = {
              {0, -1}, {1, 0}, {0, 1}, {-1, 0}};
          for (const auto& dir : ortho_directions) {
            int32_t dst_x = x + dir.first;
            int32_t dst_y = y + dir.second;

            if (dst_x < 0 || BOARD_SIZE <= dst_x ||
                dst_y < 0 || BOARD_SIZE <= dst_y) {
              continue;
            }

            uint8_t dst_piece = cells[dst_y][dst_x];

            if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
              continue;
            } else {
              legal_moves.emplace_back(x, y, dst_x, dst_y, false);
            }
          }
        }
      }
      // Add moves for moving gold generals and promoted pieces
      else if (piece == PIECE_BLACK_GOLD ||
               piece == PIECE_BLACK_PRO_PAWN ||
               piece == PIECE_BLACK_PRO_LANCE ||
               piece == PIECE_BLACK_PRO_KNIGHT ||
               piece == PIECE_BLACK_PRO_SILVER) {
        const std::vector<std::pair<int32_t, int32_t>> directions = {
            {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {0, 1}};
        for (const auto& dir : directions) {
          int32_t dst_x = x + dir.first;
          int32_t dst_y = y + dir.second;

          if (dst_x < 0 || BOARD_SIZE <= dst_x ||
              dst_y < 0 || BOARD_SIZE <= dst_y) {
            continue;
          }

          uint8_t dst_piece = cells[dst_y][dst_x];

          if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
            continue;
          } else {
            legal_moves.emplace_back(x, y, dst_x, dst_y, false);
          }
        }
      }
      // Add moves for moving the king
      else if (piece == PIECE_BLACK_KING) {
        const std::vector<std::pair<int32_t, int32_t>> directions = {
            {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};
        for (const auto& dir : directions) {
          int32_t dst_x = x + dir.first;
          int32_t dst_y = y + dir.second;

          if (dst_x < 0 || BOARD_SIZE <= dst_x ||
              dst_y < 0 || BOARD_SIZE <= dst_y) {
            continue;
          }

          uint8_t dst_piece = cells[dst_y][dst_x];

          if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
            continue;
          } else {
            legal_moves.emplace_back(x, y, dst_x, dst_y, false);
          }
        }
      }
    }
  }

  return legal_moves;
}

/**
 * Search for checkmate moves from the specified board.
 * Returns an array of moves in reverse order leading to checkmate.
 * If no checkmate moves are found, returns an empty array.
 * @param boards Array holding board information
 * @param depth Depth to search
 * @return Array of moves in reverse order leading to checkmate
 */
static std::vector<Move> searchCheckmateMove(Board* boards, int32_t depth) {
  // Depth must be odd
  if (depth % 2 == 0) {
    depth -= 1;
  }

  // If depth is 0 or less, no checkmate moves were found, so return an empty array
  if (depth <= 0) {
    return {};
  }

  // If the number of turns exceeds the draw turn, return an empty array
  if (boards[depth].getTurn() >= boards[depth].getDrawTurn()) {
    return {};
  }

  // Try moves that put the opponent in check
  std::vector<Move> shortest_moves;

  for (Move& move : boards[depth].getLegalMoves(false, true)) {
    // Create the next board state
    boards[depth - 1].copyFrom(&boards[depth]);
    boards[depth - 1].play(move);

    // Check if there are any moves to escape from check
    std::vector<Move> rescue_moves = boards[depth - 1].getLegalMoves(false, false);

    // If there are no moves to escape from check,
    // a checkmate move has been found, so return that move
    if (rescue_moves.empty()) {
      // Skip if it's a pawn drop checkmate
      if (move.getSrcX() == BOARD_SIZE && move.getSrcY() == PIECE_HAND_PAWN) {
        continue;
      }

      return {move};
    }

    // If the search depth is 1, do not try moves to escape from check
    if (depth - 1 == 0) {
      continue;
    }

    // Try all moves to escape from check and find the deepest checkmate sequence
    bool all_rescue_moves_checkmated = true;
    std::vector<Move> longest_moves;

    for (Move& rescue_move : rescue_moves) {
      // Create the next board state
      boards[depth - 2].copyFrom(&boards[depth - 1]);
      boards[depth - 2].play(rescue_move);

      // Recursively search for checkmate moves
      std::vector<Move> moves = searchCheckmateMove(boards, depth - 2);

      if (moves.empty()) {
        all_rescue_moves_checkmated = false;
        break;
      } else if (longest_moves.empty() || moves.size() > longest_moves.size() - 1) {
        longest_moves = moves;
        longest_moves.push_back(rescue_move);
      }
    }

    // If all moves to escape from check are checkmated,
    // a checkmate move has been found, so return that move
    if (all_rescue_moves_checkmated) {
      if (shortest_moves.empty() || longest_moves.size() < shortest_moves.size() - 1) {
        shortest_moves = longest_moves;
        shortest_moves.push_back(move);
      }
    }
  }

  return shortest_moves;
}

/**
 * Create a board instance.
 */
Board::Board()
    : _cells{},
      _hands{},
      _kingPositions{-1, -1},
      _color(COLOR_BLACK),
      _turn(0),
      _nyugyokuScores{28, 27},
      _drawTurn(0x7fffffff),
      _lastMove() {
}

/**
 * Create an instance of the initial board.
 * @param nyugyokuScoreBlack Score required for nyugyoku declaration for black
 * @param nyugyokuScoreWhite Score required for nyugyoku declaration for white
 * @param drawTurn Number of moves until draw
 */
Board::Board(int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn)
    : Board() {
  _nyugyokuScores[0] = nyugyokuScoreBlack;
  _nyugyokuScores[1] = nyugyokuScoreWhite;
  _drawTurn = drawTurn;
}

/**
 * Create an instance by specifying the board.
 * @param board Object holding board information
 */
Board::Board(const Board& board)
    : Board() {
  copyFrom(&board);
}

/**
 * Initialize the board with an SFEN string.
 * @param sfen SFEN string
 */
void Board::initialize(const std::string& sfen) {
  // Initialize
  memset(_cells, 0, sizeof(_cells));
  memset(_hands, 0, sizeof(_hands));
  _kingPositions[0] = -1;
  _kingPositions[1] = -1;

  // Determine the split position of the SFEN string
  char* c_sfen = const_cast<char*>(sfen.c_str());
  int32_t board_sfen_length = 0;
  int32_t hand_sfen_length = 0;

  for (int32_t i = 0; c_sfen[i] != ' '; i++) {
    board_sfen_length++;
  }

  for (int32_t i = board_sfen_length + 3; c_sfen[i] != ' '; i++) {
    hand_sfen_length++;
  }

  // Reflect SFEN information for the board
  int32_t pos_x = BOARD_SIZE - 1;
  int32_t pos_y = 0;
  bool promote = false;

  for (int32_t i = 0; i < board_sfen_length; i++) {
    char c = c_sfen[i];

    // If it's a row separator, move to the next row
    if (c == '/') {
      pos_x = BOARD_SIZE - 1;
      pos_y += 1;
      continue;
    }

    // If it's a promotion symbol, set the promotion flag
    if (c == '+') {
      promote = true;
      continue;
    }

    // Check the position on the board
    if (pos_x < 0 || pos_y >= BOARD_SIZE) {
      break;
    }

    // If it's a number, set empty squares
    if ('1' <= c && c <= '9') {
      int32_t empty_count = c - '0';

      for (int32_t j = 0; j < empty_count && pos_x >= 0; j++) {
        _cells[pos_y][pos_x] = PIECE_EMPTY;
        pos_x -= 1;
      }

      continue;
    }

    // If it's a piece symbol, set the piece
    if (c == 'P') {
      _cells[pos_y][pos_x] = promote ? PIECE_BLACK_PRO_PAWN : PIECE_BLACK_PAWN;
    } else if (c == 'L') {
      _cells[pos_y][pos_x] = promote ? PIECE_BLACK_PRO_LANCE : PIECE_BLACK_LANCE;
    } else if (c == 'N') {
      _cells[pos_y][pos_x] = promote ? PIECE_BLACK_PRO_KNIGHT : PIECE_BLACK_KNIGHT;
    } else if (c == 'S') {
      _cells[pos_y][pos_x] = promote ? PIECE_BLACK_PRO_SILVER : PIECE_BLACK_SILVER;
    } else if (c == 'B') {
      _cells[pos_y][pos_x] = promote ? PIECE_BLACK_HORSE : PIECE_BLACK_BISHOP;
    } else if (c == 'R') {
      _cells[pos_y][pos_x] = promote ? PIECE_BLACK_DRAGON : PIECE_BLACK_ROOK;
    } else if (c == 'G') {
      _cells[pos_y][pos_x] = PIECE_BLACK_GOLD;
    } else if (c == 'K') {
      _cells[pos_y][pos_x] = PIECE_BLACK_KING;
      _kingPositions[0] = pos_y * BOARD_SIZE + pos_x;
    } else if (c == 'p') {
      _cells[pos_y][pos_x] = promote ? PIECE_WHITE_PRO_PAWN : PIECE_WHITE_PAWN;
    } else if (c == 'l') {
      _cells[pos_y][pos_x] = promote ? PIECE_WHITE_PRO_LANCE : PIECE_WHITE_LANCE;
    } else if (c == 'n') {
      _cells[pos_y][pos_x] = promote ? PIECE_WHITE_PRO_KNIGHT : PIECE_WHITE_KNIGHT;
    } else if (c == 's') {
      _cells[pos_y][pos_x] = promote ? PIECE_WHITE_PRO_SILVER : PIECE_WHITE_SILVER;
    } else if (c == 'b') {
      _cells[pos_y][pos_x] = promote ? PIECE_WHITE_HORSE : PIECE_WHITE_BISHOP;
    } else if (c == 'r') {
      _cells[pos_y][pos_x] = promote ? PIECE_WHITE_DRAGON : PIECE_WHITE_ROOK;
    } else if (c == 'g') {
      _cells[pos_y][pos_x] = PIECE_WHITE_GOLD;
    } else if (c == 'k') {
      _cells[pos_y][pos_x] = PIECE_WHITE_KING;
      _kingPositions[1] = pos_y * BOARD_SIZE + pos_x;
    }

    // Move to the next position
    promote = false;
    pos_x -= 1;
  }

  // Reflect SFEN information for hand pieces
  int32_t hand_piece_num = 1;

  for (int32_t i = 0; i < hand_sfen_length; i++) {
    char c = c_sfen[board_sfen_length + 3 + i];

    // If it's a number, set the number of pieces
    if ('1' <= c && c <= '9') {
      hand_piece_num = c - '0';
      continue;
    }

    // If it's a piece symbol, set the hand pieces
    if (c == 'P') {
      _hands[0][PIECE_HAND_PAWN - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'L') {
      _hands[0][PIECE_HAND_LANCE - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'N') {
      _hands[0][PIECE_HAND_KNIGHT - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'S') {
      _hands[0][PIECE_HAND_SILVER - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'B') {
      _hands[0][PIECE_HAND_BISHOP - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'R') {
      _hands[0][PIECE_HAND_ROOK - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'G') {
      _hands[0][PIECE_HAND_GOLD - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'p') {
      _hands[1][PIECE_HAND_PAWN - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'l') {
      _hands[1][PIECE_HAND_LANCE - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'n') {
      _hands[1][PIECE_HAND_KNIGHT - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 's') {
      _hands[1][PIECE_HAND_SILVER - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'b') {
      _hands[1][PIECE_HAND_BISHOP - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'r') {
      _hands[1][PIECE_HAND_ROOK - PIECE_HAND_BEGIN] += hand_piece_num;
    } else if (c == 'g') {
      _hands[1][PIECE_HAND_GOLD - PIECE_HAND_BEGIN] += hand_piece_num;
    }

    hand_piece_num = 1;
  }

  // Set the side to move
  if (c_sfen[board_sfen_length + 1] == 'b') {
    _color = COLOR_BLACK;
  } else if (c_sfen[board_sfen_length + 1] == 'w') {
    _color = COLOR_WHITE;
  }

  // Set the turn number
  _turn = std::stoi(sfen.substr(board_sfen_length + 3 + hand_sfen_length + 1)) - 1;
}

/**
 * Move a piece.
 * @param move Move
 * @return True if legal move
 */
bool Board::play(const Move& move) {
  // Move a piece
  if (move.getSrcX() == BOARD_SIZE) {
    uint8_t hand_piece = move.getSrcY();
    uint8_t dst_piece = _cells[move.getDstY()][move.getDstX()];

    if (_color == COLOR_BLACK) {
      if (_hands[0][hand_piece] < 1 && dst_piece != PIECE_EMPTY) {
        return false;
      }

      _hands[0][hand_piece] -= 1;
      _cells[move.getDstY()][move.getDstX()] = hand_piece + PIECE_BLACK_BEGIN;
    } else {
      if (_hands[1][hand_piece] < 1 && dst_piece != PIECE_EMPTY) {
        return false;
      }

      _hands[1][hand_piece] -= 1;
      _cells[move.getDstY()][move.getDstX()] = hand_piece + PIECE_WHITE_BEGIN;
    }
  } else {
    uint8_t src_piece = _cells[move.getSrcY()][move.getSrcX()];
    uint8_t dst_piece = _cells[move.getDstY()][move.getDstX()];
    uint8_t moved_piece = src_piece;

    if (move.isPromote()) {
      moved_piece += PIECE_PROMOTE;
    }

    if (_color == COLOR_BLACK) {
      if ((src_piece < PIECE_BLACK_BEGIN || PIECE_BLACK_END <= src_piece) ||
          (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) ||
          (moved_piece >= PIECE_BLACK_END)) {
        return false;
      }

      if (PIECE_WHITE_BEGIN <= dst_piece && dst_piece < PIECE_WHITE_END) {
        uint8_t captured_piece = dst_piece - PIECE_WHITE_BEGIN;

        if (captured_piece >= PIECE_PROMOTE) {
          captured_piece -= PIECE_PROMOTE;
        }

        _hands[0][captured_piece] += 1;
      }

      _cells[move.getSrcY()][move.getSrcX()] = PIECE_EMPTY;
      _cells[move.getDstY()][move.getDstX()] = moved_piece;

      if (moved_piece == PIECE_BLACK_KING) {
        _kingPositions[0] = move.getDstY() * BOARD_SIZE + move.getDstX();
      }
    } else {
      if ((src_piece < PIECE_WHITE_BEGIN || PIECE_WHITE_END <= src_piece) ||
          (PIECE_WHITE_BEGIN <= dst_piece && dst_piece < PIECE_WHITE_END) ||
          (moved_piece >= PIECE_WHITE_END)) {
        return false;
      }

      if (PIECE_BLACK_BEGIN <= dst_piece && dst_piece < PIECE_BLACK_END) {
        uint8_t captured_piece = dst_piece - PIECE_BLACK_BEGIN;

        if (captured_piece >= PIECE_PROMOTE) {
          captured_piece -= PIECE_PROMOTE;
        }

        _hands[1][captured_piece] += 1;
      }

      _cells[move.getSrcY()][move.getSrcX()] = PIECE_EMPTY;
      _cells[move.getDstY()][move.getDstX()] = moved_piece;

      if (moved_piece == PIECE_WHITE_KING) {
        _kingPositions[1] = move.getDstY() * BOARD_SIZE + move.getDstX();
      }
    }
  }

  // Update side to move and turn number
  _color = (_color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;
  _turn += 1;

  // Update the last move
  _lastMove = move;

  return true;
}

/**
 * Get side to move.
 * @return Side to move
 */
int32_t Board::getColor() const {
  return _color;
}

/**
 * Get current number of moves.
 * @return Current number of moves
 */
int32_t Board::getTurn() const {
  return _turn;
}

/**
 * Get the number of moves until draw.
 * @return Number of moves until draw
 */
int32_t Board::getDrawTurn() const {
  return _drawTurn;
}

/**
 * Get the piece at the specified coordinates.
 * @param x X coordinate
 * @param y Y coordinate
 * @return Type of piece
 */
int32_t Board::getPiece(int32_t x, int32_t y) const {
  return _cells[y][x];
}

/**
 * Get the type of piece after moving.
 * @param move Move
 * @return Type of piece
 */
int32_t Board::getMovedPiece(const Move& move) const {
  // When dropping from hand, the type of piece to drop is in the source Y coordinate
  if (move.getSrcX() == BOARD_SIZE) {
    int32_t hand_piece = move.getSrcY();

    if (_color == COLOR_WHITE) {
      return PIECE_WHITE_BEGIN + hand_piece;
    } else {
      return PIECE_BLACK_BEGIN + hand_piece;
    }
  }

  // Get the type of piece at the source
  uint8_t piece = _cells[move.getSrcY()][move.getSrcX()];

  // If promoted, convert to promoted piece
  if (move.isPromote()) {
    piece += PIECE_PROMOTE;
  }

  return piece;
}

/**
 * Get the number of specified hand pieces.
 * @param color Side to move
 * @param piece Type of piece
 * @return Number of hand pieces
 */
int32_t Board::getHandPieceNum(int32_t color, int32_t piece) const {
  if (color == COLOR_BLACK) {
    return _hands[0][piece - PIECE_HAND_BEGIN];
  } else {
    return _hands[1][piece - PIECE_HAND_BEGIN];
  }
}

/**
 * Get the last move.
 * @return Last move
 */
Move Board::getLastMove() const {
  return _lastMove;
}

/**
 * Get a list of coordinates of pieces attacking the specified square.
 * @param x X coordinate
 * @param y Y coordinate
 * @return List of coordinates of attacking pieces
 */
std::vector<std::pair<int32_t, int32_t>> Board::getAttackers(int32_t x, int32_t y) const {
  return getAttackerPositions(_cells, x, y);
}

/**
 * Get list of legal moves for the current board.
 * @param removeUnpromote True to remove unpromoted moves for pawns, bishops, and rooks
 * @param checkmateOnly True to get only checking moves
 * @return List of legal moves
 */
std::vector<Move> Board::getLegalMoves(
    bool removeUnpromote, bool checkmateOnly) const {
  std::vector<Move> all_moves;

  // Convert board information to match the current player's perspective
  uint8_t cells[BOARD_SIZE][BOARD_SIZE];
  uint8_t hands[PIECE_HAND_END - PIECE_HAND_BEGIN];

  if (_color == COLOR_BLACK) {
    // If it's black's turn, copy as is
    memcpy(cells, _cells, sizeof(cells));

    // Copy hand pieces as is
    memcpy(hands, _hands[0], sizeof(hands));
  } else {
    // If it's white's turn, flip and copy
    for (int32_t y = 0; y < BOARD_SIZE; y++) {
      for (int32_t x = 0; x < BOARD_SIZE; x++) {
        uint8_t piece = _cells[BOARD_SIZE - 1 - y][BOARD_SIZE - 1 - x];

        if (piece == PIECE_EMPTY) {
          cells[y][x] = PIECE_EMPTY;
        } else if (PIECE_BLACK_BEGIN <= piece && piece < PIECE_BLACK_END) {
          cells[y][x] = piece - PIECE_BLACK_BEGIN + PIECE_WHITE_BEGIN;
        } else if (PIECE_WHITE_BEGIN <= piece && piece < PIECE_WHITE_END) {
          cells[y][x] = piece - PIECE_WHITE_BEGIN + PIECE_BLACK_BEGIN;
        } else {  // If an invalid piece exists, set it to empty
          cells[y][x] = PIECE_EMPTY;
        }
      }
    }

    // Copy hand pieces as is
    memcpy(hands, _hands[1], sizeof(hands));
  }

  // Get moves for dropping pieces from hand
  std::vector<Move> hand_moves = getLegalHandMoves(cells, hands);
  all_moves.insert(all_moves.end(), hand_moves.begin(), hand_moves.end());

  // Get moves for moving pieces on the board
  std::vector<Move> board_moves = getLegalBoardMoves(cells, removeUnpromote);
  all_moves.insert(all_moves.end(), board_moves.begin(), board_moves.end());

  // Extract only moves that evade check
  std::vector<Move> legal_moves;
  int32_t my_king_pos = (_color == COLOR_BLACK) ? _kingPositions[0] : _kingPositions[1];
  int32_t op_king_pos = (_color == COLOR_BLACK) ? _kingPositions[1] : _kingPositions[0];

  if (_color == COLOR_WHITE) {
    my_king_pos = BOARD_SIZE * BOARD_SIZE - 1 - my_king_pos;
    op_king_pos = BOARD_SIZE * BOARD_SIZE - 1 - op_king_pos;
  }

  for (const Move& move : all_moves) {
    // Create the board state after dropping a hand piece and check for check
    if (move.getSrcX() == BOARD_SIZE) {
      // Create the board state after dropping a hand piece
      uint8_t dst_piece = cells[move.getDstY()][move.getDstX()];
      cells[move.getDstY()][move.getDstX()] = move.getSrcY() + PIECE_BLACK_BEGIN;

      // Check for checkmate
      bool my_king_checked = false;
      bool op_king_checked = true;

      if (my_king_pos >= 0) {
        my_king_checked = isCheckedPosition(
            cells, my_king_pos % BOARD_SIZE, my_king_pos / BOARD_SIZE, COLOR_BLACK);
      }

      if (checkmateOnly && op_king_pos >= 0) {
        op_king_checked = isCheckedPosition(
            cells, op_king_pos % BOARD_SIZE, op_king_pos / BOARD_SIZE, COLOR_WHITE);
      }

      // Restore the board state
      cells[move.getDstY()][move.getDstX()] = dst_piece;

      // If the king is in check, skip this move (do not register it)
      if (my_king_checked || !op_king_checked) {
        continue;
      }
    }
    // Create the board state after moving a piece and check for check
    else {
      // Create the board state after moving a piece
      uint8_t src_piece = cells[move.getSrcY()][move.getSrcX()];
      uint8_t dst_piece = cells[move.getDstY()][move.getDstX()];
      uint8_t moved_piece = src_piece;

      if (move.isPromote()) {
        moved_piece += PIECE_PROMOTE;
      }

      cells[move.getSrcY()][move.getSrcX()] = PIECE_EMPTY;
      cells[move.getDstY()][move.getDstX()] = moved_piece;

      // Check for checkmate
      bool my_king_checked = false;
      bool op_king_checked = true;

      if (src_piece == PIECE_BLACK_KING) {
        my_king_checked = isCheckedPosition(
            cells, move.getDstX(), move.getDstY(), COLOR_BLACK);
      } else if (my_king_pos >= 0) {
        my_king_checked = isCheckedPosition(
            cells, my_king_pos % BOARD_SIZE, my_king_pos / BOARD_SIZE, COLOR_BLACK);
      }

      if (checkmateOnly && op_king_pos >= 0) {
        op_king_checked = isCheckedPosition(
            cells, op_king_pos % BOARD_SIZE, op_king_pos / BOARD_SIZE, COLOR_WHITE);
      }

      // Restore the board state
      cells[move.getSrcY()][move.getSrcX()] = src_piece;
      cells[move.getDstY()][move.getDstX()] = dst_piece;

      // If the king is in check, skip this move (do not register it)
      if (my_king_checked || !op_king_checked) {
        continue;
      }
    }

    // If it's white's turn, register the move with flipped coordinates
    if (_color == COLOR_BLACK) {
      legal_moves.push_back(move);
    } else if (move.getSrcX() == BOARD_SIZE) {
      legal_moves.emplace_back(
          BOARD_SIZE,
          move.getSrcY(),
          BOARD_SIZE - 1 - move.getDstX(),
          BOARD_SIZE - 1 - move.getDstY(),
          move.isPromote());
    } else {
      legal_moves.emplace_back(
          BOARD_SIZE - 1 - move.getSrcX(),
          BOARD_SIZE - 1 - move.getSrcY(),
          BOARD_SIZE - 1 - move.getDstX(),
          BOARD_SIZE - 1 - move.getDstY(),
          move.isPromote());
    }
  }

  return legal_moves;
}

/**
 * Get list of moves in checkmate sequence for the current board.
 * @param depth Depth for checkmate sequence search
 * @return List of moves in checkmate sequence
 */
std::vector<Move> Board::getCheckmateMoves(int32_t depth) const {
  // Depth must be an odd number greater than or equal to 1
  if (depth < 1) {
    depth = 1;
  } else if (depth % 2 == 0) {
    depth = depth - 1;
  }

  // Create an array of boards for calculation
  std::unique_ptr<Board[]> boards(new Board[depth + 1]);

  boards[depth].copyFrom(this);

  // Execute checkmate search
  std::vector<Move> moves = searchCheckmateMove(boards.get(), depth);

  // Return the sequence of moves in the checkmate sequence
  // (stored in reverse order, so reverse the order)
  std::reverse(moves.begin(), moves.end());

  return moves;
}

/**
 * Return true if nyugyoku declaration is possible.
 * @param color Side to move
 * @return True if nyugyoku declaration is possible
 */
bool Board::isNyugyoku(int32_t color) const {
  // [Condition 1] It is the declaring side's turn.
  // [Condition 6] The declaring side has remaining time.

  // [Condition 5] The declaring side's king is not in check.
  if (isCheckmate(color)) {
    return false;
  }

  // Check pieces within the opponent's camp (innermost three rows)
  int32_t nyugyoku_score = 0;
  int32_t nyugyoku_count = 0;
  bool king_found = false;

  if (color == COLOR_BLACK) {
    for (int32_t y = 0; y <= 2; y++) {
      for (int32_t x = 0; x < BOARD_SIZE; x++) {
        uint8_t piece = _cells[y][x];

        if (piece == PIECE_EMPTY ||
            piece < PIECE_BLACK_BEGIN || PIECE_BLACK_END <= piece) {
          continue;
        }

        if (piece == PIECE_BLACK_KING) {
          king_found = true;
          continue;
        }

        if (piece == PIECE_BLACK_ROOK || piece == PIECE_BLACK_DRAGON ||
            piece == PIECE_BLACK_BISHOP || piece == PIECE_BLACK_HORSE) {
          nyugyoku_score += 5;
        } else {
          nyugyoku_score += 1;
        }

        nyugyoku_count += 1;
      }
    }
  } else {
    for (int32_t y = 6; y <= 8; y++) {
      for (int32_t x = 0; x < BOARD_SIZE; x++) {
        uint8_t piece = _cells[y][x];

        if (piece == PIECE_EMPTY ||
            piece < PIECE_WHITE_BEGIN || PIECE_WHITE_END <= piece) {
          continue;
        }

        if (piece == PIECE_WHITE_KING) {
          king_found = true;
          continue;
        }

        if (piece == PIECE_WHITE_ROOK || piece == PIECE_WHITE_DRAGON ||
            piece == PIECE_WHITE_BISHOP || piece == PIECE_WHITE_HORSE) {
          nyugyoku_score += 5;
        } else {
          nyugyoku_score += 1;
        }

        nyugyoku_count += 1;
      }
    }
  }

  // [Condition 2] The declaring side's king has entered the opponent's camp
  // (innermost three rows).
  if (!king_found) {
    return false;
  }

  // [Condition 4] The declaring side has 10 or more pieces (excluding the king)
  // in the opponent's camp (innermost three rows).
  if (nyugyoku_count < 10) {
    return false;
  }

  // [Condition 3] The declaring side's score is calculated with 5 points
  // for major pieces and 1 point for minor pieces.
  // Only the declaring side's hand pieces and the declaring side's pieces
  // (excluding the king) in the opponent's camp are counted for scoring.
  if (color == COLOR_BLACK) {
    nyugyoku_score +=
        _hands[0][PIECE_HAND_ROOK - PIECE_HAND_BEGIN] * 5 +
        _hands[0][PIECE_HAND_BISHOP - PIECE_HAND_BEGIN] * 5 +
        _hands[0][PIECE_HAND_PAWN - PIECE_HAND_BEGIN] +
        _hands[0][PIECE_HAND_LANCE - PIECE_HAND_BEGIN] +
        _hands[0][PIECE_HAND_KNIGHT - PIECE_HAND_BEGIN] +
        _hands[0][PIECE_HAND_SILVER - PIECE_HAND_BEGIN] +
        _hands[0][PIECE_HAND_GOLD - PIECE_HAND_BEGIN];
  } else {
    nyugyoku_score +=
        _hands[1][PIECE_HAND_ROOK - PIECE_HAND_BEGIN] * 5 +
        _hands[1][PIECE_HAND_BISHOP - PIECE_HAND_BEGIN] * 5 +
        _hands[1][PIECE_HAND_PAWN - PIECE_HAND_BEGIN] +
        _hands[1][PIECE_HAND_LANCE - PIECE_HAND_BEGIN] +
        _hands[1][PIECE_HAND_KNIGHT - PIECE_HAND_BEGIN] +
        _hands[1][PIECE_HAND_SILVER - PIECE_HAND_BEGIN] +
        _hands[1][PIECE_HAND_GOLD - PIECE_HAND_BEGIN];
  }

  // Get the required score for nyugyoku declaration
  int required_score = 0;

  if (color == COLOR_BLACK) {
    required_score = _nyugyokuScores[0];
  } else {
    required_score = _nyugyokuScores[1];
  }

  // Check if the required score for nyugyoku declaration is met
  return nyugyoku_score >= required_score;
}

/**
 * Return true if in checkmate.
 * @param color Side to move
 * @return True if in checkmate
 */
bool Board::isCheckmate(int32_t color) const {
  // Get the king's position
  int32_t king_position =
      (color == COLOR_BLACK) ? _kingPositions[0] : _kingPositions[1];

  // If the king does not exist, it is considered not in check
  if (king_position < 0) {
    return false;
  }

  // Check if the king's position is under attack
  int32_t king_x = king_position % BOARD_SIZE;
  int32_t king_y = king_position / BOARD_SIZE;

  return isCheckedPosition(_cells, king_x, king_y, color);
}

/**
 * Get SFEN string.
 * @return SFEN string
 */
std::string Board::getSfen() const {
  std::stringstream ss;

  // Set board information
  for (int32_t y = 0; y < BOARD_SIZE; y++) {
    int32_t empty_count = 0;

    for (int32_t x = BOARD_SIZE - 1; x >= 0; x--) {
      uint8_t piece = _cells[y][x];

      if (piece == PIECE_EMPTY) {
        empty_count++;
        continue;
      }

      if (empty_count > 0) {
        ss << empty_count;
        empty_count = 0;
      }

      if (piece == PIECE_BLACK_PAWN) {
        ss << "P";
      } else if (piece == PIECE_BLACK_LANCE) {
        ss << "L";
      } else if (piece == PIECE_BLACK_KNIGHT) {
        ss << "N";
      } else if (piece == PIECE_BLACK_SILVER) {
        ss << "S";
      } else if (piece == PIECE_BLACK_BISHOP) {
        ss << "B";
      } else if (piece == PIECE_BLACK_ROOK) {
        ss << "R";
      } else if (piece == PIECE_BLACK_GOLD) {
        ss << "G";
      } else if (piece == PIECE_BLACK_KING) {
        ss << "K";
      } else if (piece == PIECE_BLACK_PRO_PAWN) {
        ss << "+P";
      } else if (piece == PIECE_BLACK_PRO_LANCE) {
        ss << "+L";
      } else if (piece == PIECE_BLACK_PRO_KNIGHT) {
        ss << "+N";
      } else if (piece == PIECE_BLACK_PRO_SILVER) {
        ss << "+S";
      } else if (piece == PIECE_BLACK_HORSE) {
        ss << "+B";
      } else if (piece == PIECE_BLACK_DRAGON) {
        ss << "+R";
      } else if (piece == PIECE_WHITE_PAWN) {
        ss << "p";
      } else if (piece == PIECE_WHITE_LANCE) {
        ss << "l";
      } else if (piece == PIECE_WHITE_KNIGHT) {
        ss << "n";
      } else if (piece == PIECE_WHITE_SILVER) {
        ss << "s";
      } else if (piece == PIECE_WHITE_BISHOP) {
        ss << "b";
      } else if (piece == PIECE_WHITE_ROOK) {
        ss << "r";
      } else if (piece == PIECE_WHITE_GOLD) {
        ss << "g";
      } else if (piece == PIECE_WHITE_KING) {
        ss << "k";
      } else if (piece == PIECE_WHITE_PRO_PAWN) {
        ss << "+p";
      } else if (piece == PIECE_WHITE_PRO_LANCE) {
        ss << "+l";
      } else if (piece == PIECE_WHITE_PRO_KNIGHT) {
        ss << "+n";
      } else if (piece == PIECE_WHITE_PRO_SILVER) {
        ss << "+s";
      } else if (piece == PIECE_WHITE_HORSE) {
        ss << "+b";
      } else if (piece == PIECE_WHITE_DRAGON) {
        ss << "+r";
      }
    }

    if (empty_count > 0) {
      ss << empty_count;
    }

    if (y < BOARD_SIZE - 1) {
      ss << "/";
    }
  }

  // Set the side to move
  if (_color == COLOR_BLACK) {
    ss << " b ";
  } else {
    ss << " w ";
  }

  // Set information about hand pieces
  bool has_hand = false;

  for (int32_t i = 0; i < PIECE_HAND_END - PIECE_HAND_BEGIN; i++) {
    int32_t hand_piece = SFEN_HAND_PIECES[i];
    const char* hand_name = SFEN_HAND_BLACK_NAMES[i];
    uint8_t hand_count = _hands[0][hand_piece - PIECE_HAND_BEGIN];

    if (hand_count > 0) {
      if (hand_count > 1) {
        ss << static_cast<int32_t>(hand_count);
      }

      ss << hand_name;
      has_hand = true;
    }
  }

  for (int32_t i = 0; i < PIECE_HAND_END - PIECE_HAND_BEGIN; i++) {
    int32_t hand_piece = SFEN_HAND_PIECES[i];
    const char* hand_name = SFEN_HAND_WHITE_NAMES[i];
    uint8_t hand_count = _hands[1][hand_piece - PIECE_HAND_BEGIN];

    if (hand_count > 0) {
      if (hand_count > 1) {
        ss << static_cast<int32_t>(hand_count);
      }

      ss << hand_name;
      has_hand = true;
    }
  }

  if (!has_hand) {
    ss << "-";
  }

  // Set the turn number
  ss << " " << (_turn + 1);

  return ss.str();
}

/**
 * Get data to input to the model.
 * @param inputs Data to input to the model
 */
void Board::getInputs(int32_t* inputs) const {
  getInputs(inputs, getColor(), getTurn());
}

/**
 * Get data to input to the model.
 * @param inputs Data to input to the model
 * @param color Side to move
 * @param turn Number of moves
 */
void Board::getInputs(int32_t* inputs, int32_t color, int32_t turn) const {
  // Initialize with 0
  std::fill_n(inputs, MODEL_INPUT_PACK_SIZE, 0);

  // Get input data for the model
  _getBoardInputs(inputs, color);
  _getInfoInputs(inputs, color, turn);
}

/**
 * Copy the board state.
 * @param board Source board to copy from
 */
void Board::copyFrom(const Board* board) {
  memcpy(_cells, board->_cells, sizeof(_cells));
  memcpy(_hands, board->_hands, sizeof(_hands));
  memcpy(_kingPositions, board->_kingPositions, sizeof(_kingPositions));
  _color = board->_color;
  _turn = board->_turn;
  memcpy(_nyugyokuScores, board->_nyugyokuScores, sizeof(_nyugyokuScores));
  _drawTurn = board->_drawTurn;
  _lastMove = board->_lastMove;
}

/**
 * Get string for displaying board information.
 * @return String for displaying board information
 */
std::string Board::toString() const {
  // String in CSA format for displaying piece information.
  static char piece_names[][3] = {
      "FU", "KY", "KE", "GI", "KA", "HI", "KI", "OU",
      "TO", "NY", "NK", "NG", "UM", "RY"};
  static char hand_color_names[][3] = {"P+", "P-"};

  // Stream object for creating a string
  std::stringstream ss;

  // Create side to move and turn number
  ss << "Color: " << ((_color == COLOR_WHITE) ? "white" : "black");
  ss << ", Turn: " << _turn << std::endl;

  // Create piece information for the board
  for (int32_t y = 0; y < BOARD_SIZE; y++) {
    ss << "P" << y + 1;

    for (int32_t x = 0; x < BOARD_SIZE; x++) {
      uint8_t piece = _cells[y][BOARD_SIZE - 1 - x];

      if (PIECE_BLACK_BEGIN <= piece && piece < PIECE_BLACK_END) {
        ss << "+" << piece_names[piece - PIECE_BLACK_BEGIN];
      } else if (PIECE_WHITE_BEGIN <= piece && piece < PIECE_WHITE_END) {
        ss << "-" << piece_names[piece - PIECE_WHITE_BEGIN];
      } else {
        ss << " * ";
      }
    }

    ss << std::endl;
  }

  // Create information about hand pieces
  for (int32_t c = 0; c < 2; c++) {
    ss << hand_color_names[c];

    for (int32_t i = 0; i < PIECE_HAND_END - PIECE_HAND_BEGIN; i++) {
      for (int32_t j = 0; j < _hands[c][i]; j++) {
        ss << "00" << piece_names[i];
      }
    }

    ss << std::endl;
  }

  return ss.str();
}

/**
 * Get board data to input to the model.
 * @param inputs Board data to input to the model
 * @param color Side to move
 */
void Board::_getBoardInputs(int32_t* inputs, int32_t color) const {
  const size_t black_offset = 1;
  const size_t white_offset = black_offset + 14 + 14 + 6;
  const size_t other_offset = white_offset + 14 + 14 + 6;
  const int board_square = BOARD_SIZE * BOARD_SIZE;

  for (int32_t y = 0; y < BOARD_SIZE; y++) {
    for (int32_t x = 0; x < BOARD_SIZE; x++) {
      // Calculate the index of the location to set the value
      // If it's white's turn, rotate the board 180 degrees
      int32_t dst = y + x * BOARD_SIZE;

      if (color == COLOR_WHITE) {
        dst = board_square - 1 - dst;
      }

      // Set information about empty squares
      if (_cells[y][x] == PIECE_EMPTY) {
        setInputBit(inputs, 0 * board_square + dst);
      }

      // Set the piece placement value
      uint8_t piece = _cells[y][x];

      if (color == COLOR_BLACK) {
        if (PIECE_BLACK_BEGIN <= piece && piece < PIECE_BLACK_END) {
          int32_t idx = piece - PIECE_BLACK_BEGIN;
          setInputBit(inputs, (black_offset + idx) * board_square + dst);
        } else if (PIECE_WHITE_BEGIN <= piece && piece < PIECE_WHITE_END) {
          int32_t idx = piece - PIECE_WHITE_BEGIN;
          setInputBit(inputs, (white_offset + idx) * board_square + dst);
        }
      } else {
        if (PIECE_BLACK_BEGIN <= piece && piece < PIECE_BLACK_END) {
          int32_t idx = piece - PIECE_BLACK_BEGIN;
          setInputBit(inputs, (white_offset + idx) * board_square + dst);
        } else if (PIECE_WHITE_BEGIN <= piece && piece < PIECE_WHITE_END) {
          int32_t idx = piece - PIECE_WHITE_BEGIN;
          setInputBit(inputs, (black_offset + idx) * board_square + dst);
        }
      }

      // Set the piece attack value
      int32_t black_att_count = 0;
      int32_t white_att_count = 0;

      for (std::pair<int32_t, int32_t> pos : getAttackers(x, y)) {
        uint8_t att_piece = _cells[pos.second][pos.first];

        if (color == COLOR_BLACK) {
          if (PIECE_BLACK_BEGIN <= att_piece && att_piece < PIECE_BLACK_END) {
            int32_t idx = att_piece - PIECE_BLACK_BEGIN;
            setInputBit(inputs, (black_offset + 14 + idx) * board_square + dst);
            black_att_count += 1;
          } else if (PIECE_WHITE_BEGIN <= att_piece && att_piece < PIECE_WHITE_END) {
            int32_t idx = att_piece - PIECE_WHITE_BEGIN;
            setInputBit(inputs, (white_offset + 14 + idx) * board_square + dst);
            white_att_count += 1;
          }
        } else {
          if (PIECE_BLACK_BEGIN <= att_piece && att_piece < PIECE_BLACK_END) {
            int32_t idx = att_piece - PIECE_BLACK_BEGIN;
            setInputBit(inputs, (white_offset + 14 + idx) * board_square + dst);
            white_att_count += 1;
          } else if (PIECE_WHITE_BEGIN <= att_piece && att_piece < PIECE_WHITE_END) {
            int32_t idx = att_piece - PIECE_WHITE_BEGIN;
            setInputBit(inputs, (black_offset + 14 + idx) * board_square + dst);
            black_att_count += 1;
          }
        }
      }

      // Set the number of piece attacks
      black_att_count = std::min(black_att_count, 5);
      white_att_count = std::min(white_att_count, 5);

      setInputBit(inputs, (black_offset + 14 + 14 + black_att_count) * board_square + dst);
      setInputBit(inputs, (white_offset + 14 + 14 + white_att_count) * board_square + dst);

      // Set the coordinates of the last moved piece
      if (_lastMove.getSrcX() >= 0 && _lastMove.getDstX() == x && _lastMove.getDstY() == y) {
        setInputBit(inputs, other_offset * board_square + dst);
      }

      // Set the row and column numbers
      const size_t row_offset = other_offset + 1;
      const size_t col_offset = row_offset + BOARD_SIZE;

      if (color == COLOR_BLACK) {
        setInputBit(inputs, (row_offset + y) * board_square + dst);
        setInputBit(inputs, (col_offset + std::min(x, BOARD_SIZE - 1 - x)) * board_square + dst);
      } else {
        setInputBit(inputs, (row_offset + BOARD_SIZE - 1 - y) * board_square + dst);
        setInputBit(inputs, (col_offset + std::min(x, BOARD_SIZE - 1 - x)) * board_square + dst);
      }
    }
  }
}

/**
 * Get game data to input to the model.
 * @param inputs Game data to input to the model
 * @param color Side to move
 * @param turn Number of moves
 */
void Board::_getInfoInputs(int32_t* inputs, int32_t color, int32_t turn) const {
  const static size_t info_offset = MODEL_FEATURES * BOARD_SIZE * BOARD_SIZE;
  const static size_t hand_offsets[] = {0, 18, 22, 26, 30, 32, 34};
  const static size_t color_offset = 38;

  // Set information about pieces in hand
  int32_t black_color = (color == COLOR_WHITE) ? COLOR_WHITE : COLOR_BLACK;
  int32_t white_color = (color == COLOR_WHITE) ? COLOR_BLACK : COLOR_WHITE;

  for (int32_t hp = PIECE_HAND_BEGIN; hp < PIECE_HAND_END; ++hp) {
    int32_t black_num = getHandPieceNum(black_color, hp);
    int32_t white_num = getHandPieceNum(white_color, hp);

    for (int32_t i = 0; i < black_num; i++) {
      setInputBit(inputs, info_offset + hand_offsets[hp - PIECE_HAND_BEGIN] + i);
    }

    for (int32_t i = 0; i < white_num; i++) {
      setInputBit(inputs, info_offset + color_offset + hand_offsets[hp - PIECE_HAND_BEGIN] + i);
    }
  }

  // Set information about check
  if (isCheckmate(_color)) {
    setInputBit(inputs, info_offset + color_offset * 2);
  }

  // Set the points required for entering king declaration
  if (color == COLOR_BLACK) {
    inputs[MODEL_INPUT_PACK_SIZE - 3] = (int)((_nyugyokuScores[0] - 27.5) / 5.0 * 0xfffff);
    inputs[MODEL_INPUT_PACK_SIZE - 2] = (int)((_nyugyokuScores[1] - 27.5) / 5.0 * 0xfffff);
  } else {
    inputs[MODEL_INPUT_PACK_SIZE - 3] = (int)((_nyugyokuScores[1] - 27.5) / 5.0 * 0xfffff);
    inputs[MODEL_INPUT_PACK_SIZE - 2] = (int)((_nyugyokuScores[0] - 27.5) / 5.0 * 0xfffff);
  }

  // Set the remaining number of moves until a draw
  float remaining_turn = 1.0 - (_drawTurn - turn) / 50.0;

  remaining_turn = std::min(std::max(remaining_turn, 0.0f), 1.0f);
  inputs[MODEL_INPUT_PACK_SIZE - 1] = (int)(remaining_turn * 0xfffff);
}

}  // namespace deepshogi

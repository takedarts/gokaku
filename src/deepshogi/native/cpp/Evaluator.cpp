#include "Evaluator.h"

// Move directions
#define DIR_H (0)    // Move: Drop
#define DIR_U (1)    // Move: Up
#define DIR_D (2)    // Move: Down
#define DIR_R (3)    // Move: Right
#define DIR_L (4)    // Move: Left
#define DIR_UR (5)   // Move: Up-Right
#define DIR_UL (6)   // Move: Up-Left
#define DIR_DR (7)   // Move: Down-Right
#define DIR_DL (8)   // Move: Down-Left
#define DIR_KR (9)   // Move: Knight-Right
#define DIR_KL (10)  // Move: Knight-Left

namespace deepshogi {

/**
 * Get the Policy index for the specified move.
 * @param board Board
 * @param move Move
 * @return Policy index
 */
static int32_t getPolicyIndex(const Board* board, Move move) {
  // Calculate piece number and move direction
  int32_t move_x = 0;
  int32_t move_y = 0;
  int32_t piece = 0;

  if (move.getSrcX() >= BOARD_SIZE) {
    move_x = 0;
    move_y = 0;
    piece = move.getSrcY() - PIECE_HAND_BEGIN;
  } else {
    move_x = move.getDstX() - move.getSrcX();
    move_y = move.getDstY() - move.getSrcY();
    piece = board->getPiece(move.getSrcX(), move.getSrcY());

    if (move.isPromote()) {
      piece += PIECE_PROMOTE;
    }

    if (PIECE_BLACK_BEGIN <= piece && piece < PIECE_BLACK_END) {
      piece -= PIECE_BLACK_BEGIN;
    } else if (PIECE_WHITE_BEGIN <= piece && piece < PIECE_WHITE_END) {
      piece -= PIECE_WHITE_BEGIN;
    } else {
      std::cerr << "Invalid piece: " << piece << std::endl;
      throw std::invalid_argument("Invalid piece");
    }
  }

  // Reverse move coordinates for white's turn
  if (board->getColor() == COLOR_WHITE) {
    move_x = -move_x;
    move_y = -move_y;
  }

  // Determine move direction
  int32_t dir = 0;

  if (move_x == 0 && move_y == 0) {  // Drop
    dir = DIR_H;
  } else if (move_x == 0 && move_y < 0) {  // Up
    dir = DIR_U;
  } else if (move_x == 0 && move_y > 0) {  // Down
    dir = DIR_D;
  } else if (move_x < 0 && move_y == 0) {  // Right
    dir = DIR_R;
  } else if (move_x > 0 && move_y == 0) {  // Left
    dir = DIR_L;
  } else if (move_x < 0 && move_y == move_x) {  // Up-Right
    dir = DIR_UR;
  } else if (move_x > 0 && move_y == -move_x) {  // Up-Left
    dir = DIR_UL;
  } else if (move_x < 0 && move_y == -move_x) {  // Down-Right
    dir = DIR_DR;
  } else if (move_x > 0 && move_y == move_x) {  // Down-Left
    dir = DIR_DL;
  } else if (move_x == -1 && move_y == -2) {  // Knight-Right
    dir = DIR_KR;
  } else if (move_x == 1 && move_y == -2) {  // Knight-Left
    dir = DIR_KL;
  } else {
    std::cerr << "Invalid move direction: " << move_x << ", " << move_y << std::endl;
    throw std::invalid_argument("Invalid move direction");
  }

  // Calculate Policy index
  int32_t index = 0;

  if (piece == 0) {  // Pawn
    index =
        0 + ((dir == DIR_U)   ? 0
             : (dir == DIR_H) ? 1
                              : -1);
  } else if (piece == 1) {  // Lance
    index =
        2 + ((dir == DIR_U)   ? 0
             : (dir == DIR_H) ? 1
                              : -3);
  } else if (piece == 2) {  // Knight
    index =
        4 + ((dir == DIR_KR)   ? 0
             : (dir == DIR_KL) ? 1
             : (dir == DIR_H)  ? 2
                               : -5);
  } else if (piece == 3) {  // Silver
    index =
        7 + ((dir == DIR_U)    ? 0
             : (dir == DIR_UR) ? 1
             : (dir == DIR_UL) ? 2
             : (dir == DIR_DR) ? 3
             : (dir == DIR_DL) ? 4
             : (dir == DIR_H)  ? 5
                               : -8);
  } else if (piece == 4) {  // Bishop
    index =
        13 + ((dir == DIR_UR)   ? 0
              : (dir == DIR_UL) ? 1
              : (dir == DIR_DR) ? 2
              : (dir == DIR_DL) ? 3
              : (dir == DIR_H)  ? 4
                                : -14);
  } else if (piece == 5) {  // Rook
    index =
        18 + ((dir == DIR_U)   ? 0
              : (dir == DIR_D) ? 1
              : (dir == DIR_R) ? 2
              : (dir == DIR_L) ? 3
              : (dir == DIR_H) ? 4
                               : -19);
  } else if (piece == 6) {  // Gold
    index =
        23 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_H)  ? 6
                                : -24);
  } else if (piece == 7) {  // King
    index =
        30 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -31);
  } else if (piece == 8) {  // Promoted Pawn
    index =
        38 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
                                : -39);
  } else if (piece == 9) {  // Promoted Lance
    index =
        44 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
                                : -45);
  } else if (piece == 10) {  // Promoted Knight
    index =
        50 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_KR) ? 6
              : (dir == DIR_KL) ? 7
                                : -51);
  } else if (piece == 11) {  // Promoted Silver
    index =
        58 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -59);
  } else if (piece == 12) {  // Horse
    index =
        66 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -67);
  } else if (piece == 13) {  // Dragon
    index =
        74 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -75);
  } else {
    std::cerr << "Invalid piece: " << piece << std::endl;
    throw std::invalid_argument("Invalid piece");
  }

  if (index < 0) {
    std::cerr << "Invalid index: " << index << std::endl;
    throw std::invalid_argument("Invalid index");
  }

  // Return the index
  return index;
}

/**
 * Create evaluation result object.
 * @param processor Object to execute inference
 */
Evaluator::Evaluator(Processor* processor)
    : _processor(processor),
      _policies(),
      _value(0.0),
      _evaluated(false) {
}

/**
 * Clear evaluation results from the model.
 */
void Evaluator::clear() {
  _policies.clear();
  _value = 0.0;
  _evaluated = false;
}

/**
 * Execute evaluation by the model.
 * @param board Board to be evaluated
 */
void Evaluator::evaluate(Board* board) {
  // Do nothing if already evaluated.
  if (_evaluated) {
    return;
  }

  // Execute evaluation for the current board.
  float inputs[MODEL_INPUT_SIZE];
  float outputs[MODEL_OUTPUT_SIZE];

  board->getInputs(inputs);
  _processor->execute(inputs, outputs, 1);

  // Create list of candidate moves
  std::vector<Move> legal_moves = board->getLegalMoves();

  for (Move move : legal_moves) {
    // Calculate Policy index
    int32_t idx = getPolicyIndex(board, move);

    // Calculate destination coordinates
    int32_t x = move.getDstX();
    int32_t y = move.getDstY();

    if (board->getColor() == COLOR_WHITE) {
      x = BOARD_SIZE - 1 - x;
      y = BOARD_SIZE - 1 - y;
    }

    // Add Policy
    int32_t index = ((idx * BOARD_SIZE * BOARD_SIZE) + (x * BOARD_SIZE + y));

    _policies.emplace_back(move, outputs[index], 0);
  }

  // Get predicted win rate.
  _value = outputs[MODEL_PREDICTIONS * BOARD_SIZE * BOARD_SIZE + 0] * 2 - 1;

  // Reverse evaluation value for white's turn.
  if (board->getColor() == COLOR_WHITE) {
    _value = -_value;
  }

  // Set evaluated flag.
  _evaluated = true;
}

/**
 * Return true if evaluation results from the model are set.
 * @return True if evaluation results from the model are set
 */
bool Evaluator::isEvaluated() {
  return _evaluated;
}

/**
 * Get list of predicted candidate moves from model inference results.
 * @return List of predicted candidate moves
 */
std::vector<Policy> Evaluator::getPolicies() {
  return _policies;
}

/**
 * Get predicted win rate from model inference results.
 * @return Predicted win rate from model inference results
 */
float Evaluator::getValue() {
  return _value;
}

}  // namespace deepshogi

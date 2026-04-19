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

  if (move.getSrc().getX() >= BOARD_SIZE) {
    move_x = 0;
    move_y = 0;
    piece = move.getSrc().getY() - PIECE_HAND_BEGIN;
  } else {
    move_x = move.getDst().getX() - move.getSrc().getX();
    move_y = move.getDst().getY() - move.getSrc().getY();
    piece = board->getPiece(move.getSrc());

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
 * @param cacheSize Cache size for evaluation results
 */
Evaluator::Evaluator(Processor* processor, int32_t cacheSize)
    : _mutex(),
      _processor(processor),
      _cacheSize(cacheSize),
      _cacheKeys(),
      _cache() {
}

/**
 * Execute evaluation by the model.
 * @param board Board to be evaluated
 * @return Evaluation result
 */
Evaluation Evaluator::evaluate(const Board* board) {
  uint64_t hash = board->getHash();

  {
    // Check if the evaluation result exists in the cache.
    std::shared_lock lock(_mutex);

    auto it = _cache.find(hash);

    if (it != _cache.end()) {
      return it->second;
    }
  }

  // If not found in the cache, run evaluation by the model.
  Evaluation evaluation = _evaluate(board);

  {
    // Add the evaluation result to the cache.
    std::unique_lock lock(_mutex);

    while (!_cacheKeys.empty() && _cacheKeys.size() >= static_cast<size_t>(_cacheSize)) {
      _cache.erase(_cacheKeys.front());
      _cacheKeys.pop();
    }

    if (_cache.find(hash) == _cache.end()) {
      _cacheKeys.push(hash);
      _cache.emplace(hash, evaluation);
    }
  }

  return evaluation;
}

/**
 * Execute evaluation by the model.
 * @param board Board to be evaluated
 * @return Evaluation result
 */
Evaluation Evaluator::_evaluate(const Board* board) {
  // Execute evaluation for the current board.
  int32_t inputs[MODEL_INPUT_PACK_SIZE];
  float outputs[MODEL_OUTPUT_SIZE];

  board->getInputs(inputs);
  _processor->execute(inputs, outputs, 1);

  // Create list of candidate moves
  std::vector<Move> legal_moves = board->getLegalMoves(true, false);
  std::vector<Policy> policies;

  for (Move move : legal_moves) {
    // Calculate Policy index
    int32_t idx = getPolicyIndex(board, move);

    // Calculate destination coordinates
    int32_t x = move.getDst().getX();
    int32_t y = move.getDst().getY();

    if (board->getColor() == COLOR_WHITE) {
      x = BOARD_SIZE - 1 - x;
      y = BOARD_SIZE - 1 - y;
    }

    // Add Policy
    int32_t index = ((idx * BOARD_SIZE * BOARD_SIZE) + (x * BOARD_SIZE + y));

    policies.emplace_back(move, outputs[index], 0);
  }

  // Get predicted win rate.
  float value = outputs[MODEL_PREDICTIONS * BOARD_SIZE * BOARD_SIZE + 0] * 2 - 1;

  // Reverse evaluation value for white's turn.
  if (board->getColor() == COLOR_WHITE) {
    value = -value;
  }

  // Return evaluation result.
  return Evaluation(value, policies);
}

}  // namespace deepshogi

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Config.h"
#include "Move.h"

namespace deepshogi {

class Board {
 public:
  /**
   * Create a board instance.
   */
  Board();

  /**
   * Create an instance of the initial board.
   * @param nyugyokuScoreBlack Score required for nyugyoku declaration for black
   * @param nyugyokuScoreWhite Score required for nyugyoku declaration for white
   * @param drawTurn Number of moves until draw
   */
  Board(int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn);

  /**
   * Create an instance by specifying the board.
   * @param board Object holding board information
   */
  Board(const Board& board);

  /**
   * Destroy the instance.
   */
  virtual ~Board() = default;

  /**
   * Initialize the board with an SFEN string.
   * @param sfen SFEN string
   */
  void initialize(const std::string& sfen);

  /**
   * Move a piece.
   * @param move Move
   * @return True if legal move
   */
  bool play(const Move& move);

  /**
   * Get side to move.
   * @return Side to move
   */
  int32_t getColor() const;

  /**
   * Get current number of moves.
   * @return Current number of moves
   */
  int32_t getTurn() const;

  /**
   * Get the number of moves until draw.
   * @return Number of moves until draw
   */
  int32_t getDrawTurn() const;

  /**
   * Get the piece at the specified coordinates.
   * @param x X coordinate
   * @param y Y coordinate
   * @return Type of piece
   */
  int32_t getPiece(int32_t x, int32_t y) const;

  /**
   * Get the type of piece after moving.
   * @param move Move
   * @return Type of piece
   */
  int32_t getMovedPiece(const Move& move) const;

  /**
   * Get the number of specified hand pieces.
   * @param color Side to move
   * @param piece Type of piece
   * @return Number of hand pieces
   */
  int32_t getHandPieceNum(int32_t color, int32_t piece) const;

  /**
   * Get the last move.
   * @return Last move
   */
  Move getLastMove() const;

  /**
   * Get list of coordinates of pieces attacking the specified coordinates.
   * @param x X coordinate
   * @param y Y coordinate
   * @return List of coordinates of attacking pieces
   */
  std::vector<std::pair<int32_t, int32_t>> getAttackers(int32_t x, int32_t y) const;

  /**
   * Get list of legal moves for the current board.
   * @param removeUnpromote True to remove unpromoted moves for pawns, bishops, and rooks
   * @param checkmateOnly True to get only checking moves
   * @return List of legal moves
   */
  std::vector<Move> getLegalMoves(bool removeUnpromote, bool checkmateOnly) const;

  /**
   * Get list of moves in checkmate sequence for the current board.
   * @param depth Depth for checkmate sequence search
   * @return List of moves in checkmate sequence
   */
  std::vector<Move> getCheckmateMoves(int32_t depth) const;

  /**
   * Return true if nyugyoku declaration is possible.
   * @param color Side to move
   * @return True if nyugyoku declaration is possible
   */
  bool isNyugyoku(int32_t color) const;

  /**
   * Return true if in checkmate.
   * @param color Side to move
   * @return True if in checkmate
   */
  bool isCheckmate(int32_t color) const;

  /**
   * Get SFEN string.
   * @return SFEN string
   */
  std::string getSfen() const;

  /**
   * Get data to input to the model.
   * @param inputs Data to input to the model
   */
  void getInputs(int32_t* inputs) const;

  /**
   * Get data to input to the model.
   * @param inputs Data to input to the model
   * @param color Side to move
   * @param turn Number of moves
   */
  void getInputs(int32_t* inputs, int32_t color, int32_t turn) const;

  /**
   * Copy the board state.
   * @param board Source board to copy from
   */
  void copyFrom(const Board* board);

  /**
   * Get string for displaying board information.
   * @return String for displaying board information
   */
  std::string toString() const;

 private:
  /**
   * Piece information for each square on the board.
   */
  uint8_t _cells[BOARD_SIZE][BOARD_SIZE];

  /**
   * Hand piece information for black.
   */
  uint8_t _hands[2][PIECE_HAND_END - PIECE_HAND_BEGIN];

  /**
   * King positions (0: black, 1: white).
   */
  int32_t _kingPositions[2];

  /**
   * Side to move.
   */
  int32_t _color;

  /**
   * Current number of moves.
   */
  int32_t _turn;

  /**
   * Scores required for nyugyoku declaration (0: black, 1: white).
   */
  int32_t _nyugyokuScores[2];

  /**
   * Number of moves until draw.
   */
  int32_t _drawTurn;

  /**
   * Last move.
   */
  Move _lastMove;

  /**
   * Get board data to input to the model.
   * @param inputs Board data to input to the model
   * @param color Side to move
   */
  void _getBoardInputs(int32_t* inputs, int32_t color) const;

  /**
   * Get game data to input to the model.
   * @param inputs Game data to input to the model
   * @param color Side to move
   * @param turn Number of moves
   */
  void _getInfoInputs(int32_t* inputs, int32_t color, int32_t turn) const;
};

}  // namespace deepshogi

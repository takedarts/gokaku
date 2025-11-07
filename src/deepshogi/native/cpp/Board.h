#pragma once

#include "Move.h"
#include "cshogi/cshogi.h"

namespace deepshogi {

class Board {
 public:
  /**
   * Create an instance of the initial board.
   * @param nyugyokuScoreBlack Score required for nyugyoku declaration for black
   * @param nyugyokuScoreWhite Score required for nyugyoku declaration for white
   * @param drawSteps Number of moves until draw
   */
  Board(int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps);

  /**
   * Create an instance by specifying the board.
   * @param board Object holding board information
   */
  Board(const Board& board);

  /**
   * Create a board instance.
   */
  Board();

  /**
   * Destroy the instance.
   */
  virtual ~Board() = default;

  /**
   * Initialize the board with an SFEN string.
   * @param sfen SFEN string
   */
  void initializeWithSfen(const std::string& sfen);

  /**
   * Initialize the board with Huffman encoded board information.
   * @param data Huffman encoded board information
   */
  void initializeWithPackedSfen(char* data);

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
   * Get list of coordinates of pieces attacking the specified coordinates.
   * @param x X coordinate
   * @param y Y coordinate
   * @return List of coordinates of attacking pieces
   */
  std::vector<std::pair<int32_t, int32_t>> getAttackers(int32_t x, int32_t y) const;

  /**
   * Get list of legal moves for the current board.
   * Returns a list of legal moves with non-promoting moves for pawn, bishop, and rook removed.
   * @return List of legal moves
   */
  std::vector<Move> getLegalMoves() const;

  /**
   * Get move history.
   * @return Move history
   */
  std::vector<Move> getHistoryMoves() const;

  /**
   * Search for checkmate sequence and return the first move.
   * If no checkmate sequence is found, returns pass (MOVE_PASS).
   * If checkSearchNode is 0, performs exhaustive search.
   * If checkSearchNode is 1 or more, uses the df-pn algorithm for search.
   * @param checkSearchDepth Depth for checkmate sequence search
   * @param checkSearchNode Number of nodes for checkmate sequence search (0 for exhaustive search)
   * @return Move in checkmate sequence
   */
  Move searchCheckMove(int32_t checkSearchDepth, int32_t checkSearchNode);

  /**
   * Return true if nyugyoku declaration is possible.
   * @return True if nyugyoku declaration is possible
   */
  bool isNyugyoku() const;

  /**
   * Return true if in checkmate.
   * @return True if in checkmate
   */
  bool isCheckmate() const;

  /**
   * Get SFEN string.
   * @return SFEN string
   */
  std::string getSfen() const;

  /**
   * Get Huffman encoded board information.
   * @param data Buffer to store Huffman encoded board information
   */
  void getPackedSfen(char* data) const;

  /**
   * Get data to input to the model.
   * @param inputs Data to input to the model
   */
  void getInputs(float* inputs) const;

  /**
   * Get data to input to the model.
   * @param inputs Data to input to the model
   * @param color Side to move
   * @param steps Number of moves
   */
  void getInputs(float* inputs, int32_t color, int32_t steps) const;

  /**
   * Copy the board state.
   * @param board Source board to copy from
   */
  void copyFrom(const Board* board);

  /**
   * Get string for displaying board information.
   * @return String for displaying board information
   */
  std::string dump() const;

  /**
   * Output the board state.
   * @param os Output destination
   */
  void print(std::ostream& os = std::cout) const;

 private:
  /**
   * Object to hold board information.
   * Uses cshogi's __Board class.
   */
  cshogi::__Board _board;

  /**
   * Score required for nyugyoku declaration for black.
   */
  int32_t _nyugyokuScoreBlack;

  /**
   * Score required for nyugyoku declaration for white.
   */
  int32_t _nyugyokuScoreWhite;

  /**
   * Number of moves until draw.
   */
  int32_t _drawSteps;

  /**
   * Get board data to input to the model.
   * @param inputs Board data to input to the model
   * @param color Side to move
   */
  void _getBoardInputs(float* inputs, int32_t color) const;

  /**
   * Get game data to input to the model.
   * @param inputs Game data to input to the model
   * @param color Side to move
   * @param steps Number of moves
   */
  void _getInfoInputs(float* inputs, int32_t color, int32_t steps) const;
};

}  // namespace deepshogi

#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

#include "BitBoard.h"
#include "Move.h"
#include "MoveResult.h"
#include "Position.h"

namespace deepshogi {

/**
 * Hold the board position.
 */
class Board {
 private:
  /**
   * Calculate position hashes.
   * Allow hash classes to access private board members.
   */
  friend class BoardHash;
  friend class InferenceHash;

 public:
  /**
   * Construct a board object.
   * Do not place any pieces on the board.
   */
  Board();

  /**
   * Constructs a board object.
   * No pieces are placed on the board.
   * @param nyugyokuScoreBlack Points required for black's entering-king declaration
   * @param nyugyokuScoreWhite Points required for white's entering-king declaration
   * @param drawTurn Number of moves until a draw
   */
  Board(int8_t nyugyokuScoreBlack, int8_t nyugyokuScoreWhite, int16_t drawTurn);

  /**
   * Destroys the board object.
   */
  virtual ~Board() = default;

  /**
   * Initializes the board from an SFEN-format string.
   * @param sfen SFEN-format string
   */
  void initialize(const std::string& sfen);

  /**
   * Moves a piece.
   * @param move The move to apply
   * @return The result of the move
   */
  MoveResult play(const Move& move);

  /**
   * Undoes the given move from the board.
   * This function assumes the given move is the most recent move.
   * @param result The result of the move to undo
   */
  void undo(const MoveResult& result);

  /**
   * Returns the positions of pieces that attack the specified coordinate.
   * @param position The coordinate to check
   * @return List of positions of pieces attacking the specified coordinate
   */
  std::vector<Position> getAttackers(const Position& position) const;

  /**
   * Returns the list of legal moves for the current board state.
   * @param removeUnpromote If true, removes non-promotion moves for pawn, bishop, rook, and lance
   * on the 2nd rank
   * @param checkOnly If true, returns only moves that cause check
   * @return List of legal moves
   */
  std::vector<Move> getLegalMoves(bool removeUnpromote, bool checkOnly) const;

  /**
   * Generate legal moves while reusing the existing capacity.
   * @param moves std::vector<Move>& output vector, whose contents are replaced.
   * @param removeUnpromote bool flag to omit selected unpromoted moves.
   * @param checkOnly bool flag to generate only checking moves.
   * @return void
   */
  void getLegalMoves(std::vector<Move>& moves, bool removeUnpromote, bool checkOnly) const;

  /**
   * Return a checkmating move sequence for the current position.
   * @param depth Checkmate search depth.
   * @return Checkmating move sequence.
   */
  std::vector<Move> getCheckmateMoves(int32_t depth) const;

  /**
   * Return the piece score, counting major pieces as five and minor pieces as one.
   * If nyugyoku is true, count only pieces in enemy territory and in hand.
   * If nyugyoku is false, count all pieces except the king.
   * @param color Color to score.
   * @param nyugyoku True to count only pieces in enemy territory and in hand.
   * @return Total piece score.
   */
  int8_t getScore(int8_t color, bool nyugyoku) const;

  /**
   * Return true if an entering-king victory can be declared.
   * @param color Declaring color.
   * @return True if an entering-king victory can be declared.
   */
  bool isNyugyoku(int8_t color) const;

  /**
   * Returns true if the specified color is in check.
   * @param color The color of the side being checked
   * @return true if in check
   */
  bool isCheck(int8_t color) const;

  /**
   * Returns the board state as an SFEN-format string.
   * @return SFEN-format string
   */
  std::string getSfen() const;

  /**
   * Returns the input data for the model.
   * @param inputs Input data for the model
   */
  void getInputs(int32_t* inputs) const;

  /**
   * Returns the input data for the model.
   * @param inputs Input data for the model
   * @param color The current player's color
   */
  void getInputs(int32_t* inputs, int8_t color) const;

  /**
   * Copies the board state from another board.
   * @param board The source board
   */
  void copyFrom(const Board* board);

  /**
   * Returns a string for displaying the board information.
   * @return String representation of the board
   */
  std::string toString() const;

  /**
   * Returns the current player's color.
   * @return The current player's color
   */
  inline int8_t getColor() const {
    return _color;
  }

  /**
   * Returns the current move number.
   * @return Current move number
   */
  inline int16_t getTurn() const {
    return _turn;
  }

  /**
   * Returns the move number at which the game is declared a draw.
   * @return Move number for a draw
   */
  inline int16_t getDrawTurn() const {
    return _drawTurn;
  }

  /**
   * Return the piece at the specified coordinates.
   * @param x X coordinate.
   * @param y Y coordinate.
   * @return Piece type.
   */
  inline uint8_t getPiece(const Position& position) const {
    return _cells[position.getIndex()];
  }

  /**
   * Returns the number of the specified held piece.
   * @param color The player's color
   * @param piece Type of piece
   * @return Number of held pieces
   */
  inline int8_t getHandPieceNum(int8_t color, uint8_t piece) const {
    return _hands[(color == COLOR_BLACK) ? 0 : 1][piece];
  }

  /**
   * Write a string representation of the board to the output stream.
   * @param os Output stream.
   * @param board Board object.
   * @return Output stream.
   */
  friend std::ostream& operator<<(std::ostream& os, const Board& board) {
    os << board.toString();
    return os;
  }

 private:
  /**
   * Copy constructor for the board object.
   * The assignment operator `=` is only available for internal use.
   * @param board The source board object to copy
   */
  Board(const Board& board) = default;

  /**
   * Array representing the state of each square on the board.
   */
  uint8_t _cells[BOARD_SIZE * BOARD_SIZE];

  /**
   * Hash value representing the piece arrangement on the board.
   */
  uint64_t _cellHash;

  /**
   * Array representing the number of held pieces for each player.
   */
  uint8_t _hands[2][PIECE_HAND_END - PIECE_HAND_BEGIN];

  /**
   * Array representing each player's held piece counts as bits.
   * Sets bits in the following ranges equal to the piece count:
   * Pawn: 0-17, Lance: 18-21, Knight: 22-25, Silver: 26-29, Bishop: 30-31, Rook: 32-33, Gold: 34-37
   *
   * This bit representation is used for the following purposes:
   * - Checking the held piece condition when determining inferior boards
   * - Generating held piece information when creating model input data
   */
  uint64_t _handBits[2];

  /**
   * King positions (0: black, 1: white).
   */
  Position _kingPositions[2];

  /**
   * Points required for entering-king declaration (0: black, 1: white).
   */
  int8_t _nyugyokuScores[2];

  /**
   * Current player's color.
   */
  int8_t _color;

  /**
   * Current move number.
   */
  int16_t _turn;

  /**
   * Move number at which the game is declared a draw.
   */
  int16_t _drawTurn;

  /**
   * Piece occupancy bitboards: index 0 for Black, index 1 for White.
   * Each bit corresponds to a square: one if occupied, zero otherwise.
   */
  BitBoard _colorBitBoards[2];

  /**
   * Bitboards representing where each type of piece is located (0: black's pieces, 1: white's
   * pieces).
   * Each bit corresponds to a square on the board: 1 if a piece is present, 0 otherwise.
   * The array index corresponds to the type of piece.
   * However, the following pieces are assigned to another piece's bitboard:
   *  - Promoted pawn, promoted lance, promoted knight, and promoted silver are assigned to the gold
   * bitboard
   *  - Horse (promoted bishop) is assigned to both the bishop and king bitboards
   *  - Dragon (promoted rook) is assigned to both the rook and king bitboards
   */
  BitBoard _pieceBitBoards[2][PIECE_BLACK_PRO_PAWN - PIECE_BLACK_BEGIN];

  /**
   * Places a piece at the specified position.
   * This function assumes there is no piece at the specified position.
   * @param pos The coordinate to place the piece
   * @param piece Integer value representing the piece to place
   */
  void _putPiece(const Position& pos, uint8_t piece);

  /**
   * Removes a piece from the specified position.
   * @param pos The coordinate to remove the piece from
   */
  void _removePiece(const Position& pos);

  /**
   * Adds the specified piece to the held pieces.
   * @param color The player's color (COLOR_BLACK or COLOR_WHITE)
   * @param piece Integer value representing the piece to add
   * @param num Number of pieces to add
   */
  void _addHand(int8_t color, uint8_t piece, int32_t num);

  /**
   * Adds the specified piece to the held pieces.
   * @param color The player's color (COLOR_BLACK or COLOR_WHITE)
   * @param piece Integer value representing the piece to add
   */
  void _addHand(int8_t color, uint8_t piece);

  /**
   * Removes the specified piece from the held pieces.
   * This function assumes the specified piece exists in the held pieces.
   * @param color The player's color (COLOR_BLACK or COLOR_WHITE)
   * @param piece Integer value representing the piece to remove
   */
  void _removeHand(int8_t color, uint8_t piece);

  /**
   * Return the positions of pieces attacking the specified square.
   * If returnOnFirstAttacker is true, return attack existence as a bool.
   * Treat additionalOccIndex as an immobile blocker when calculating sliding attacks.
   * If removeOwnKing is true, ignore our king when calculating sliding attacks.
   * @param color Color of the side under attack.
   * @param posIndex Square to check for attacking pieces.
   * @param additionalOccIndex Additional occupied square, or -1 for none.
   * @return bool when returnOnFirstAttacker is true; otherwise std::vector<int8_t> positions.
   */
  template <bool returnOnFirstAttacker, bool removeOwnKing>
  std::conditional_t<returnOnFirstAttacker, bool, std::vector<int8_t>> _getAttackers(
      int8_t color, int8_t posIndex, int8_t additionalOccIndex = -1) const;

  /**
   * Return the legal moves for the current position.
   * If removeUnpromote is true, omit unpromoted pawn, bishop, rook, and second-rank lance moves.
   * If checkOnly is true, generate only checking moves.
   * @param legalMoves Vector to which legal moves are appended.
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalMoves(std::vector<Move>& legalMoves) const;

  /**
   * Generates legal moves for pawn movement.
   * If the template argument removeUnpromote is true, removes non-promotion moves.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add pawn legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for pawn movement
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalPawnMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for lance movement.
   * If the template argument removeUnpromote is true, removes non-promotion moves on the 2nd rank.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add lance legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for lance movement
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalLanceMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for knight movement.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add knight legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for knight movement
   */
  template <bool checkOnly>
  void _getLegalKnightMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for silver movement.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add silver legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for silver movement
   */
  template <bool checkOnly>
  void _getLegalSilverMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for gold movement.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add gold legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for gold movement
   */
  template <bool checkOnly>
  void _getLegalGoldMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for king movement.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add king legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for king movement
   */
  template <bool checkOnly>
  void _getLegalKingMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for bishop movement.
   * If the template argument removeUnpromote is true, removes non-promotion moves.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add bishop legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for bishop movement
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalBishopMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for rook movement.
   * If the template argument removeUnpromote is true, removes non-promotion moves.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add rook legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for rook movement
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalRookMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for dropping a pawn from hand.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add drop-pawn legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for dropping a pawn
   */
  template <bool checkOnly>
  void _getLegalHandPawnMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for dropping a lance from hand.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add drop-lance legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for dropping a lance
   */
  template <bool checkOnly>
  void _getLegalHandLanceMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for dropping a knight from hand.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add drop-knight legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for dropping a knight
   */
  template <bool checkOnly>
  void _getLegalHandKnightMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for dropping a silver from hand.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add drop-silver legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for dropping a silver
   */
  template <bool checkOnly>
  void _getLegalHandSilverMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for dropping a gold from hand.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add drop-gold legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for dropping a gold
   */
  template <bool checkOnly>
  void _getLegalHandGoldMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for dropping a bishop from hand.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add drop-bishop legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for dropping a bishop
   */
  template <bool checkOnly>
  void _getLegalHandBishopMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Generates legal moves for dropping a rook from hand.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add drop-rook legal moves to
   * @param destinationBitBoard Bitboard of valid destination coordinates for dropping a rook
   */
  template <bool checkOnly>
  void _getLegalHandRookMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Returns whether moving from the specified position to the specified position results in check.
   * The piece type to move is specified by the template argument piece (specified as
   * PIECE_BLACK_XXX).
   * @param srcIndex Integer value representing the source position
   * @param dstIndex Integer value representing the destination position
   * @return true if the move results in check
   */
  template <uint8_t piece>
  bool _isCheckMove(int8_t srcIndex, int8_t dstIndex) const;

  /**
   * Returns whether moving from the specified position to the specified position results in a
   * discovered check.
   * @param srcIndex Integer value representing the source position
   * @param dstIndex Integer value representing the destination position
   * @param color The color of the king to check
   * @return true if the move results in a discovered check
   */
  bool _isDiscoveredCheckMove(int8_t srcIndex, int8_t dstIndex, int8_t color) const;

  /**
   * Returns whether dropping a piece at the specified position results in check.
   * The piece type to drop is specified by the template argument piece (specified as
   * PIECE_BLACK_XXX).
   * @param dstIndex Integer value representing the destination position
   * @return true if the drop results in check
   */
  template <uint8_t piece>
  bool _isDropCheckMove(int8_t dstIndex) const;

  /**
   * Returns whether dropping a pawn at the specified position results in an illegal checkmate
   * (uchifuzume).
   * @param dstIndex Integer value representing the destination position
   * @return true if the drop results in an illegal checkmate
   */
  bool _isDropPawnCheckmateMove(int8_t dstIndex) const;

  /**
   * Returns the board data to input to the model.
   * @param inputs Board data to input to the model
   * @param color The player's color (COLOR_BLACK or COLOR_WHITE)
   */
  void _getBoardInputs(int32_t* inputs, int8_t color) const;

  /**
   * Returns the game data to input to the model.
   * @param inputs Game data to input to the model
   * @param color The player's color (COLOR_BLACK or COLOR_WHITE)
   */
  void _getInfoInputs(int32_t* inputs, int8_t color) const;
};

}  // namespace deepshogi

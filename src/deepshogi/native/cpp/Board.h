#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "BitBoard.h"
#include "Move.h"
#include "MoveResult.h"
#include "Position.h"

namespace deepshogi {

/**
 * A class that holds board state information.
 */
class Board {
 private:
  /**
   * A class for computing hash values of the board.
   * Declared as a friend class of Board to allow access to its private members.
   */
  friend class BoardHash;

 public:
  /**
   * Constructs a board object.
   * No pieces are placed on the board.
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
   * @param removeUnpromote If true, removes non-promotion moves for pawn, bishop, rook, and lance on the 2nd rank
   * @param checkOnly If true, returns only moves that cause check
   * @return List of legal moves
   */
  std::vector<Move> getLegalMoves(bool removeUnpromote, bool checkOnly) const;

  /**
   * Returns the sequence of moves leading to checkmate for the current board state.
   * @param depth Depth of the checkmate search
   * @return Sequence of moves leading to checkmate
   */
  std::vector<Move> getCheckmateMoves(int32_t depth) const;

  /**
   * Returns true if an entering-king declaration is possible.
   * @param color The color of the declaring side
   * @return true if an entering-king declaration is possible
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
   * Returns true if this board is equal to or inferior to the given board.
   * An inferior board is one where the current player and piece positions on the board are the same,
   * and the number of each type of held piece is equal or fewer.
   * Whether the piece positions on the board are the same is checked by comparing hash values.
   * (Hash collisions are considered extremely rare in practice and are ignored here.)
   * To check the held piece condition, verify that the set of positions where bits are set
   * in the held piece bit representation is a subset of the other board's set.
   * @param other The board to compare against
   * @param color The color to evaluate
   * @return true if this board is equal to or inferior to the given board
   */
  inline bool isLesserThanOrEqual(const Board& other, int8_t color) const {
    // If the current player differs, consider the boards different
    if (_color != other._color) {
      return false;
    }

    // If the board hash values differ, consider the piece positions different
    if (_cellHash != other._cellHash) {
      return false;
    }

    // If the piece positions differ, return false
    if (_colorBitBoards[0] != other._colorBitBoards[0] ||
        _colorBitBoards[1] != other._colorBitBoards[1]) {
      return false;
    }

    // Verify that the set of bit positions set in the held piece bit representation
    // is a subset of the other board's set
    int8_t color_idx = (color == COLOR_BLACK) ? 0 : 1;

    return (_handBits[color_idx] & other._handBits[color_idx]) == _handBits[color_idx];
  }

  /**
   * Returns true if this board is strictly inferior to the given board.
   * @param other The board to compare against
   * @param color The color to evaluate
   * @return true if this board is strictly inferior to the given board
   */
  inline bool isLesserThan(const Board& other, int8_t color) const {
    // If the current player differs, consider the boards different
    if (_color != other._color) {
      return false;
    }

    // If the board hash values differ, consider the piece positions different
    if (_cellHash != other._cellHash) {
      return false;
    }

    // If the piece positions differ, return false
    if (_colorBitBoards[0] != other._colorBitBoards[0] ||
        _colorBitBoards[1] != other._colorBitBoards[1]) {
      return false;
    }

    // If the held piece bit representations are the same, the held piece counts are also the same
    if (_handBits[0] == other._handBits[0] && _handBits[1] == other._handBits[1]) {
      return false;
    }

    // Verify that the set of bit positions set in the held piece bit representation
    // is a subset of the other board's set
    int8_t color_idx = (color == COLOR_BLACK) ? 0 : 1;

    return (_handBits[color_idx] & other._handBits[color_idx]) == _handBits[color_idx];
  }

  /**
   * Returns the piece at the specified coordinates.
   * @param x X coordinate
   * @param y Y coordinate
   * @return Type of piece
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
   * Returns the last move.
   * @return The last move
   */
  inline Move getLastMove() const {
    return _lastMove;
  }

  /**
   * Writes the string representation of the board to an output stream.
   * @param os Output stream
   * @param board Board object
   * @return Output stream
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
   * Object that stores the most recent move.
   */
  Move _lastMove;

  /**
   * Bitboards representing where each color's pieces are located (0: black's pieces, 1: white's pieces).
   * Each bit corresponds to a square on the board: 1 if a piece is present, 0 otherwise.
   */
  BitBoard _colorBitBoards[2];

  /**
   * Bitboards representing where each type of piece is located (0: black's pieces, 1: white's pieces).
   * Each bit corresponds to a square on the board: 1 if a piece is present, 0 otherwise.
   * The array index corresponds to the type of piece.
   * However, the following pieces are assigned to another piece's bitboard:
   *  - Promoted pawn, promoted lance, promoted knight, and promoted silver are assigned to the gold bitboard
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
   * Returns a list of positions of pieces that attack the specified coordinate.
   * If the template argument returnOnFirstAttacker is true,
   * returns only the first attacker found.
   * If additionalOccIndex is specified, calculates sliding piece attacks assuming
   * there is an immovable piece at that coordinate.
   * If the template argument removeOwnKing is true, removes the own king's position
   * when calculating sliding piece attacks.
   * @param color The color of the side being attacked
   * @param posIndex The coordinate to check for attacking pieces
   * @param additionalOccIndex Coordinate to assume has an extra piece (-1 if none)
   * @return List of positions of pieces attacking the specified coordinate
   */
  template <bool returnOnFirstAttacker, bool removeOwnKing>
  std::vector<int8_t> _getAttackers(
      int8_t color, int8_t posIndex, int8_t additionalOccIndex = -1) const;

  /**
   * Returns the list of legal moves for the current board state.
   * If the template argument removeUnpromote is true, removes non-promotion moves for pawn, bishop, rook, and lance on the 2nd rank.
   * If the template argument checkOnly is true, returns only moves that cause check.
   * @param legalMoves Array object to add legal moves to
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
   * The piece type to move is specified by the template argument piece (specified as PIECE_BLACK_XXX).
   * @param srcIndex Integer value representing the source position
   * @param dstIndex Integer value representing the destination position
   * @return true if the move results in check
   */
  template <uint8_t piece>
  bool _isCheckMove(int8_t srcIndex, int8_t dstIndex) const;

  /**
   * Returns whether moving from the specified position to the specified position results in a discovered check.
   * @param srcIndex Integer value representing the source position
   * @param dstIndex Integer value representing the destination position
   * @param color The color of the king to check
   * @return true if the move results in a discovered check
   */
  bool _isDiscoveredCheckMove(int8_t srcIndex, int8_t dstIndex, int8_t color) const;

  /**
   * Returns whether dropping a piece at the specified position results in check.
   * The piece type to drop is specified by the template argument piece (specified as PIECE_BLACK_XXX).
   * @param dstIndex Integer value representing the destination position
   * @return true if the drop results in check
   */
  template <uint8_t piece>
  bool _isDropCheckMove(int8_t dstIndex) const;

  /**
   * Returns whether dropping a pawn at the specified position results in an illegal checkmate (uchifuzume).
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

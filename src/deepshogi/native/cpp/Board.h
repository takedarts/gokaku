#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "BitBoard.h"
#include "Move.h"
#include "Position.h"
#include "Result.h"

namespace deepshogi {

/**
 * A class that holds board state information.
 */
class Board {
 public:
  /**
   * Creates a board object.
   * Pieces are not placed on the board.
   */
  Board();

  /**
   * Creates a board object.
   * Pieces are not placed on the board.
   * @param nyugyokuScoreBlack Points required for black's declaration of victory
   * @param nyugyokuScoreWhite Points required for white's declaration of victory
   * @param drawTurn Number of moves until a draw occurs
   */
  Board(int8_t nyugyokuScoreBlack, int8_t nyugyokuScoreWhite, int16_t drawTurn);

  /**
   * Destroys the board object.
   */
  virtual ~Board() = default;

  /**
   * Initializes the board from a SFEN format string.
   * @param sfen SFEN format string
   */
  void initialize(const std::string& sfen);

  /**
   * Moves a piece.
   * @param move The move to play
   * @return The result of the move
   */
  Result play(const Move& move);

  /**
   * Reverts the specified move from the board.
   * This function assumes the specified move is the last move made.
   * @param result The result of the move to revert
   */
  void undo(const Result& result);

  /**
   * Gets the positions of pieces attacking the specified coordinate.
   * @param position The coordinate to check
   * @return List of positions of pieces attacking the specified coordinate
   */
  std::vector<Position> getAttackers(const Position& position) const;

  /**
   * Gets the list of legal moves in the current board state.
   * @param removeUnpromote If true, removes non-promotion moves for pawns, bishops, rooks, and lances on the 2nd row
   * @param checkOnly If true, only returns moves that give check
   * @return List of legal moves
   */
  std::vector<Move> getLegalMoves(bool removeUnpromote, bool checkOnly) const;

  /**
   * Gets the move sequence for the checkmate line in the current board state.
   * @param depth The depth of checkmate search
   * @return Move sequence for the checkmate line
   */
  std::vector<Move> getCheckmateMoves(int32_t depth) const;

  /**
   * Returns true if a declaration of victory is possible.
   * @param color The side making the declaration
   * @return True if a declaration of victory is possible
   */
  bool isNyugyoku(int8_t color) const;

  /**
   * Returns true if the specified side is in check.
   * @param color The side that is in check
   * @return True if in check
   */
  bool isCheck(int8_t color) const;

  /**
   * Gets the SFEN format string.
   * @return SFEN format string
   */
  std::string getSfen() const;

  /**
   * Gets data to input to the model.
   * @param inputs Data to input to the model
   */
  void getInputs(int32_t* inputs) const;

  /**
   * Gets data to input to the model.
   * @param inputs Data to input to the model
   * @param color The side to move
   */
  void getInputs(int32_t* inputs, int8_t color) const;

  /**
   * Copies the board state.
   * @param board The source board to copy from
   */
  void copyFrom(const Board* board);

  /**
   * Gets a string representation of the board state.
   * @return String representation of the board state
   */
  std::string toString() const;

  /**
   * Gets the side to move.
   * @return The side to move
   */
  inline int8_t getColor() const {
    return _color;
  }

  /**
   * Gets the current move number.
   * @return The current move number
   */
  inline int16_t getTurn() const {
    return _turn;
  }

  /**
   * Gets the move number at which a draw occurs.
   * @return The move number at which a draw occurs
   */
  inline int16_t getDrawTurn() const {
    return _drawTurn;
  }

  /**
   * Gets the hash value of the board.
   * @return The hash value of the board
   */
  inline uint64_t getHash() const {
    if (_color == COLOR_BLACK) {
      return _hash;
    } else {
      return _hash ^ 0xffffffffffffffffULL;
    }
  }

  /**
   * Gets the piece at the specified position.
   * @param position The position on the board
   * @return The piece type
   */
  inline uint8_t getPiece(const Position& position) const {
    return _cells[position.getIndex()];
  }

  /**
   * Gets the number of hand pieces of the specified type.
   * @param color The side to move
   * @param piece The piece type
   * @return The number of hand pieces
   */
  inline int8_t getHandPieceNum(int8_t color, uint8_t piece) const {
    return _hands[(color == COLOR_BLACK) ? 0 : 1][piece];
  }

  /**
   * Gets the last move made.
   * @return The last move
   */
  inline Move getLastMove() const {
    return _lastMove;
  }

  /**
   * Writes the string representation of the board to the output stream.
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
   * Copies the board object.
   * Copying via the `=` operator is for internal use only.
   * @param board The source board object to copy from
   */
  Board(const Board& board) = default;

  /**
   * Array representing the state of each square on the board.
   */
  uint8_t _cells[BOARD_SIZE * BOARD_SIZE];

  /**
   * Array representing the number of hand pieces for each player.
   */
  uint8_t _hands[2][PIECE_HAND_END - PIECE_HAND_BEGIN];

  /**
   * Position of the king (0: black, 1: white).
   */
  Position _kingPositions[2];

  /**
   * Points required for declaration of victory (0: black, 1: white).
   */
  int8_t _nyugyokuScores[2];

  /**
   * The side to move.
   */
  int8_t _color;

  /**
   * The current move number.
   */
  int16_t _turn;

  /**
   * The move number at which a draw occurs.
   */
  int16_t _drawTurn;

  /**
   * Hash value.
   */
  uint64_t _hash;

  /**
   * Object storing the last move.
   */
  Move _lastMove;

  /**
   * Bitboards representing the positions of black and white pieces (0: black pieces, 1: white pieces).
   * Each bit corresponds to a square on the board, with 1 indicating a piece is present and 0 otherwise.
   */
  BitBoard _colorBitBoards[2];

  /**
   * Bitboards representing the positions of each piece type (0: black pieces, 1: white pieces).
   * Each bit corresponds to a square on the board, with 1 indicating a piece is present and 0 otherwise.
   * Array indices correspond to piece types.
   * However, the following pieces are assigned to other piece bitboards:
   *  - Promoted pawn, promoted lance, promoted knight, and promoted silver are assigned to the gold bitboard
   *  - Horse is assigned to the bishop and king bitboards
   *  - Dragon is assigned to the rook and king bitboards
   */
  BitBoard _pieceBitBoards[2][PIECE_BLACK_PRO_PAWN - PIECE_BLACK_BEGIN];

  /**
   * Places a piece at the specified position.
   * This function assumes the specified coordinate is empty.
   * @param pos The position to place the piece
   * @param piece Integer value representing the piece to place
   */
  void _putPiece(const Position& pos, uint8_t piece);

  /**
   * Removes a piece from the specified position.
   * @param pos The position from which to remove the piece
   */
  void _removePiece(const Position& pos);

  /**
   * Adds a piece to hand.
   * @param color The side (COLOR_BLACK or COLOR_WHITE)
   * @param piece Integer value representing the piece to add
   * @param num The number of pieces to add
   */
  void _addHand(int8_t color, uint8_t piece, int32_t num);

  /**
   * Adds a piece to hand.
   * @param color The side (COLOR_BLACK or COLOR_WHITE)
   * @param piece Integer value representing the piece to add
   */
  void _addHand(int8_t color, uint8_t piece);

  /**
   * Removes a piece from hand.
   * This function assumes the specified piece exists in hand.
   * @param color The side (COLOR_BLACK or COLOR_WHITE)
   * @param piece Integer value representing the piece to remove
   */
  void _removeHand(int8_t color, uint8_t piece);

  /**
   * Returns a list of positions of pieces attacking the specified coordinate.
   * If the template argument returnOnFirstAttacker is true, only the first attacking piece found is returned.
   * If additionalOccIndex is specified, a piece blocking that square is assumed when calculating slider attacks.
   * If the template argument removeOwnKing is true, the own king square is removed when calculating slider attacks.
   * @param color The side being attacked
   * @param posIndex The coordinate to check for attacking pieces
   * @param additionalOccIndex Coordinate to assume a piece is on (-1 if not adding)
   * @return List of positions of pieces attacking the specified coordinate
   */
  template <bool returnOnFirstAttacker, bool removeOwnKing>
  std::vector<int8_t> _getAttackers(
      int8_t color, int8_t posIndex, int8_t additionalOccIndex = -1) const;

  /**
   * Gets the list of legal moves in the current board state.
   * If the template argument removeUnpromote is true, removes non-promotion moves for pawns, bishops, rooks, and lances on the 2nd row.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add the list of legal moves to
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalMoves(std::vector<Move>& legalMoves) const;

  /**
   * Creates legal moves for pawn moves.
   * If the template argument removeUnpromote is true, removes non-promotion moves.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal pawn moves to
   * @param destinationBitBoard Bitboard of valid destinations for pawn moves
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalPawnMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for lance moves.
   * If the template argument removeUnpromote is true, removes non-promotion moves on the 2nd row.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal lance moves to
   * @param destinationBitBoard Bitboard of valid destinations for lance moves
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalLanceMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for knight moves.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal knight moves to
   * @param destinationBitBoard Bitboard of valid destinations for knight moves
   */
  template <bool checkOnly>
  void _getLegalKnightMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for silver moves.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal silver moves to
   * @param destinationBitBoard Bitboard of valid destinations for silver moves
   */
  template <bool checkOnly>
  void _getLegalSilverMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for gold moves.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal gold moves to
   * @param destinationBitBoard Bitboard of valid destinations for gold moves
   */
  template <bool checkOnly>
  void _getLegalGoldMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for king moves.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal king moves to
   * @param destinationBitBoard Bitboard of valid destinations for king moves
   */
  template <bool checkOnly>
  void _getLegalKingMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for bishop moves.
   * If the template argument removeUnpromote is true, removes non-promotion moves.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal bishop moves to
   * @param destinationBitBoard Bitboard of valid destinations for bishop moves
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalBishopMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for rook moves.
   * If the template argument removeUnpromote is true, removes non-promotion moves.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal rook moves to
   * @param destinationBitBoard Bitboard of valid destinations for rook moves
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalRookMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for hand pawn drops.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal hand pawn moves to
   * @param destinationBitBoard Bitboard of valid destinations for hand pawn drops
   */
  template <bool checkOnly>
  void _getLegalHandPawnMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for hand lance drops.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal hand lance moves to
   * @param destinationBitBoard Bitboard of valid destinations for hand lance drops
   */
  template <bool checkOnly>
  void _getLegalHandLanceMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for hand knight drops.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal hand knight moves to
   * @param destinationBitBoard Bitboard of valid destinations for hand knight drops
   */
  template <bool checkOnly>
  void _getLegalHandKnightMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for hand silver drops.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal hand silver moves to
   * @param destinationBitBoard Bitboard of valid destinations for hand silver drops
   */
  template <bool checkOnly>
  void _getLegalHandSilverMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for hand gold drops.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal hand gold moves to
   * @param destinationBitBoard Bitboard of valid destinations for hand gold drops
   */
  template <bool checkOnly>
  void _getLegalHandGoldMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for hand bishop drops.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal hand bishop moves to
   * @param destinationBitBoard Bitboard of valid destinations for hand bishop drops
   */
  template <bool checkOnly>
  void _getLegalHandBishopMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Creates legal moves for hand rook drops.
   * If the template argument checkOnly is true, only returns moves that give check.
   * @param legalMoves Array to add legal hand rook moves to
   * @param destinationBitBoard Bitboard of valid destinations for hand rook drops
   */
  template <bool checkOnly>
  void _getLegalHandRookMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * Returns whether a move from the specified position to another position gives check.
   * The template argument piece specifies the type of piece being moved (use PIECE_BLACK_XXX).
   * @param srcIndex Integer value representing the source position
   * @param dstIndex Integer value representing the destination position
   * @return True if the move gives check
   */
  template <uint8_t piece>
  bool _isCheckMove(int8_t srcIndex, int8_t dstIndex) const;

  /**
   * Returns whether a move from the specified position to another position creates a discovered check.
   * @param srcIndex Integer value representing the source position
   * @param dstIndex Integer value representing the destination position
   * @param color The side being checked
   * @return True if the move creates a discovered check
   */
  bool _isDiscoveredCheckMove(int8_t srcIndex, int8_t dstIndex, int8_t color) const;

  /**
   * Returns whether a drop to the specified position gives check.
   * The template argument piece specifies the type of piece being dropped (use PIECE_BLACK_XXX).
   * @param dstIndex Integer value representing the destination position
   * @return True if the drop gives check
   */
  template <uint8_t piece>
  bool _isDropCheckMove(int8_t dstIndex) const;

  /**
   * Returns whether a pawn drop at the specified position is an illegal pawn drop checkmate.
   * @param dstIndex Integer value representing the destination position
   * @return True if the pawn drop is an illegal checkmate
   */
  bool _isDropPawnCheckmateMove(int8_t dstIndex) const;

  /**
   * Gets board data to input to the model.
   * @param inputs Board data to input to the model
   * @param color The side to move (COLOR_BLACK or COLOR_WHITE)
   */
  void _getBoardInputs(int32_t* inputs, int8_t color) const;

  /**
   * Gets game data to input to the model.
   * @param inputs Game data to input to the model
   * @param color The side to move (COLOR_BLACK or COLOR_WHITE)
   */
  void _getInfoInputs(int32_t* inputs, int8_t color) const;
};

}  // namespace deepshogi

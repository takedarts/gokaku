#include "Board.h"

#include <algorithm>
#include <map>
#include <sstream>

#include "Constant.h"

namespace deepshogi {

// Mapping table from piece number to SFEN character
static const std::map<uint8_t, const char*> SFEN_PIECE_NAMES = {
    {PIECE_BLACK_PAWN, "P"},
    {PIECE_BLACK_LANCE, "L"},
    {PIECE_BLACK_KNIGHT, "N"},
    {PIECE_BLACK_SILVER, "S"},
    {PIECE_BLACK_GOLD, "G"},
    {PIECE_BLACK_BISHOP, "B"},
    {PIECE_BLACK_ROOK, "R"},
    {PIECE_BLACK_KING, "K"},
    {PIECE_BLACK_PRO_PAWN, "+P"},
    {PIECE_BLACK_PRO_LANCE, "+L"},
    {PIECE_BLACK_PRO_KNIGHT, "+N"},
    {PIECE_BLACK_PRO_SILVER, "+S"},
    {PIECE_BLACK_HORSE, "+B"},
    {PIECE_BLACK_DRAGON, "+R"},
    {PIECE_WHITE_PAWN, "p"},
    {PIECE_WHITE_LANCE, "l"},
    {PIECE_WHITE_KNIGHT, "n"},
    {PIECE_WHITE_SILVER, "s"},
    {PIECE_WHITE_GOLD, "g"},
    {PIECE_WHITE_BISHOP, "b"},
    {PIECE_WHITE_ROOK, "r"},
    {PIECE_WHITE_KING, "k"},
    {PIECE_WHITE_PRO_PAWN, "+p"},
    {PIECE_WHITE_PRO_LANCE, "+l"},
    {PIECE_WHITE_PRO_KNIGHT, "+n"},
    {PIECE_WHITE_PRO_SILVER, "+s"},
    {PIECE_WHITE_HORSE, "+b"},
    {PIECE_WHITE_DRAGON, "+r"},
};

// Mapping table from SFEN character to piece number
// For promoted pieces, converts to the unpromoted piece number (promotion is handled separately)
static const std::map<char, uint8_t> SFEN_PIECE_TYPES = {
    {'P', PIECE_BLACK_PAWN},
    {'L', PIECE_BLACK_LANCE},
    {'N', PIECE_BLACK_KNIGHT},
    {'S', PIECE_BLACK_SILVER},
    {'G', PIECE_BLACK_GOLD},
    {'B', PIECE_BLACK_BISHOP},
    {'R', PIECE_BLACK_ROOK},
    {'K', PIECE_BLACK_KING},
    {'p', PIECE_WHITE_PAWN},
    {'l', PIECE_WHITE_LANCE},
    {'n', PIECE_WHITE_KNIGHT},
    {'s', PIECE_WHITE_SILVER},
    {'g', PIECE_WHITE_GOLD},
    {'b', PIECE_WHITE_BISHOP},
    {'r', PIECE_WHITE_ROOK},
    {'k', PIECE_WHITE_KING},
};

// Array of SFEN hand piece names
static const std::map<uint8_t, const char*> SFEN_HAND_PIECE_NAMES[2] = {
    {
        {PIECE_HAND_ROOK, "R"},
        {PIECE_HAND_BISHOP, "B"},
        {PIECE_HAND_GOLD, "G"},
        {PIECE_HAND_SILVER, "S"},
        {PIECE_HAND_KNIGHT, "N"},
        {PIECE_HAND_LANCE, "L"},
        {PIECE_HAND_PAWN, "P"},
    },
    {
        {PIECE_HAND_ROOK, "r"},
        {PIECE_HAND_BISHOP, "b"},
        {PIECE_HAND_GOLD, "g"},
        {PIECE_HAND_SILVER, "s"},
        {PIECE_HAND_KNIGHT, "n"},
        {PIECE_HAND_LANCE, "l"},
        {PIECE_HAND_PAWN, "p"},
    },
};

// Mapping table from SFEN hand piece character to {color, piece number}
static const std::map<char, std::pair<uint8_t, uint8_t>> SFEN_HAND_PIECE_TYPES = {
    {'R', {COLOR_BLACK, PIECE_HAND_ROOK}},
    {'B', {COLOR_BLACK, PIECE_HAND_BISHOP}},
    {'G', {COLOR_BLACK, PIECE_HAND_GOLD}},
    {'S', {COLOR_BLACK, PIECE_HAND_SILVER}},
    {'N', {COLOR_BLACK, PIECE_HAND_KNIGHT}},
    {'L', {COLOR_BLACK, PIECE_HAND_LANCE}},
    {'P', {COLOR_BLACK, PIECE_HAND_PAWN}},
    {'r', {COLOR_WHITE, PIECE_HAND_ROOK}},
    {'b', {COLOR_WHITE, PIECE_HAND_BISHOP}},
    {'g', {COLOR_WHITE, PIECE_HAND_GOLD}},
    {'s', {COLOR_WHITE, PIECE_HAND_SILVER}},
    {'n', {COLOR_WHITE, PIECE_HAND_KNIGHT}},
    {'l', {COLOR_WHITE, PIECE_HAND_LANCE}},
    {'p', {COLOR_WHITE, PIECE_HAND_PAWN}},
};

// Array of SFEN hand piece types
// Used to determine the display order in SFEN format
static constexpr uint8_t SFEN_HAND_PIECES[] = {
    PIECE_HAND_ROOK, PIECE_HAND_BISHOP, PIECE_HAND_GOLD,
    PIECE_HAND_SILVER, PIECE_HAND_KNIGHT, PIECE_HAND_LANCE, PIECE_HAND_PAWN};

// Bitboard indices corresponding to piece types
static constexpr int8_t PAWN_INDEX = PIECE_BLACK_PAWN - PIECE_BLACK_BEGIN;
static constexpr int8_t LANCE_INDEX = PIECE_BLACK_LANCE - PIECE_BLACK_BEGIN;
static constexpr int8_t KNIGHT_INDEX = PIECE_BLACK_KNIGHT - PIECE_BLACK_BEGIN;
static constexpr int8_t SILVER_INDEX = PIECE_BLACK_SILVER - PIECE_BLACK_BEGIN;
static constexpr int8_t GOLD_INDEX = PIECE_BLACK_GOLD - PIECE_BLACK_BEGIN;
static constexpr int8_t KING_INDEX = PIECE_BLACK_KING - PIECE_BLACK_BEGIN;
static constexpr int8_t BISHOP_INDEX = PIECE_BLACK_BISHOP - PIECE_BLACK_BEGIN;
static constexpr int8_t ROOK_INDEX = PIECE_BLACK_ROOK - PIECE_BLACK_BEGIN;

// Offsets for the held piece bit representation
static constexpr int8_t HAND_BIT_OFFSETS[PIECE_HAND_END - PIECE_HAND_BEGIN] = {
    0, 18, 22, 26, 30, 32, 34};

/**
 * Sets the specified bit.
 * @param inputs Bit string
 * @param index Position of the bit to set
 */
inline void setInputBit(int32_t* inputs, int32_t index, int32_t value = 1) {
  inputs[index / 32] |= (value << (index % 32));
}

/**
 * Searches for a checkmate move from the given board state.
 * Returns an array with the checkmate move sequence in reverse order.
 * Returns an empty array if no checkmate is found.
 * Note that the Board object's contents change during the search.
 * @param board Board object
 * @param depth Search depth
 * @return List of moves leading to checkmate
 */
static std::vector<Move> searchCheckmateMoves(Board& board, int32_t depth) {
  // If depth is 0 or less, no checkmate was found; return empty array
  if (depth <= 0) {
    return {};
  }

  // If the draw turn count is exceeded, return empty array
  if (board.getTurn() >= board.getDrawTurn()) {
    return {};
  }

  // Try moves that put the opponent in check
  std::vector<Move> shortest_moves;

  for (const Move& move : board.getLegalMoves(false, true)) {
    // Advance the board and save the result
    MoveResult checkmate_result = board.play(move);

    // Check if there are no moves to escape from check
    std::vector<Move> escape_moves = board.getLegalMoves(false, false);

    // If there are no escape moves, checkmate is found; return the move
    if (escape_moves.empty()) {
      board.undo(checkmate_result);
      return {move};
    }

    // If search depth is 1 or less, do not try escape moves
    if (depth - 1 <= 0) {
      board.undo(checkmate_result);
      continue;
    }

    // Try all escape moves and find the longest checkmate sequence
    bool checkmated = true;
    std::vector<Move> longest_moves;

    for (Move& escape_move : escape_moves) {
      // Create the next board state
      MoveResult escape_result = board.play(escape_move);

      // Recursively search for checkmate moves
      std::vector<Move> moves = searchCheckmateMoves(board, depth - 2);

      // If no checkmate found, determine it is not checkmate and break the loop
      if (moves.empty()) {
        board.undo(escape_result);
        checkmated = false;
        break;
      }

      // Save the longest checkmate sequence
      if (longest_moves.empty() || moves.size() > longest_moves.size() - 1) {
        longest_moves = moves;
        longest_moves.push_back(escape_move);
      }

      // Restore the board
      board.undo(escape_result);
    }

    // If all escape moves lead to checkmate, a checkmate sequence has been found
    // Save the shortest checkmate sequence
    if (checkmated) {
      if (shortest_moves.empty() || longest_moves.size() < shortest_moves.size() - 1) {
        shortest_moves = longest_moves;
        shortest_moves.push_back(move);
      }
    }

    // Restore the board
    board.undo(checkmate_result);
  }

  // Return the shortest checkmate sequence
  return shortest_moves;
}

/**
 * Constructs a board object.
 * No pieces are placed on the board.
 */
Board::Board()
    : _cells{0},
      _cellHash(0),
      _hands{{0}},
      _handBits{0},
      _kingPositions{POSITION_INVALID, POSITION_INVALID},
      _nyugyokuScores{28, 27},
      _color(COLOR_BLACK),
      _turn(0),
      _drawTurn(0x7fff),
      _lastMove(MOVE_INVALID) {
}

/**
 * Constructs a board object.
 * No pieces are placed on the board.
 * @param nyugyokuScoreBlack Points required for black's entering-king declaration
 * @param nyugyokuScoreWhite Points required for white's entering-king declaration
 * @param drawTurn Number of moves until a draw
 */
Board::Board(int8_t nyugyokuScoreBlack, int8_t nyugyokuScoreWhite, int16_t drawTurn)
    : Board() {
  _nyugyokuScores[0] = nyugyokuScoreBlack;
  _nyugyokuScores[1] = nyugyokuScoreWhite;
  _drawTurn = drawTurn;
}

/**
 * Initializes the board from an SFEN-format string.
 * @param sfen SFEN-format string
 */
void Board::initialize(const std::string& sfen) {
  // Initialize
  std::fill(std::begin(_cells), std::end(_cells), 0);
  _cellHash = 0;

  std::fill(_hands[0], _hands[0] + (sizeof(_hands[0]) / sizeof(_hands[0][0])), 0);
  std::fill(_hands[1], _hands[1] + (sizeof(_hands[1]) / sizeof(_hands[1][0])), 0);
  _handBits[0] = 0;
  _handBits[1] = 0;

  _kingPositions[0] = POSITION_INVALID;
  _kingPositions[1] = POSITION_INVALID;

  _colorBitBoards[0].clearAll();
  _colorBitBoards[1].clearAll();

  for (int32_t p = 0; p < PIECE_BLACK_PRO_PAWN - PIECE_BLACK_BEGIN; p++) {
    _pieceBitBoards[0][p].clearAll();
    _pieceBitBoards[1][p].clearAll();
  }

  // Determine the split positions in the SFEN string
  char* c_sfen = const_cast<char*>(sfen.c_str());
  int32_t board_sfen_length = 0;
  int32_t hand_sfen_length = 0;

  for (int32_t i = 0; c_sfen[i] != ' '; i++) {
    board_sfen_length++;
  }

  for (int32_t i = board_sfen_length + 3; c_sfen[i] != ' '; i++) {
    hand_sfen_length++;
  }

  // Apply the board SFEN information
  int32_t pos_x = BOARD_SIZE - 1;
  int32_t pos_y = 0;
  bool promote = false;

  for (int32_t i = 0; i < board_sfen_length; i++) {
    char c = c_sfen[i];

    // Move to the next row at a row separator
    if (c == '/') {
      pos_x = BOARD_SIZE - 1;
      pos_y += 1;
      continue;
    }

    // Set the promotion flag when the promotion symbol is encountered
    if (c == '+') {
      promote = true;
      continue;
    }

    // Check the position on the board
    if (pos_x < 0 || pos_y >= BOARD_SIZE) {
      break;
    }

    // If a digit, advance by that many empty squares
    if ('1' <= c && c <= '9') {
      pos_x -= c - '0';
      continue;
    }

    // If a piece symbol, set the piece
    Position pos(pos_x, pos_y);
    uint8_t piece = SFEN_PIECE_TYPES.at(c);

    // Apply promotion
    if (promote) {
      piece += PIECE_PROMOTE;
    }

    _putPiece(pos, piece);

    // Move to the next position
    promote = false;
    pos_x -= 1;
  }

  // Apply the hand piece SFEN information
  int32_t hand_piece_num = 0;

  for (int32_t i = 0; i < hand_sfen_length; i++) {
    char c = c_sfen[board_sfen_length + 3 + i];

    // If a digit, set the piece count
    if ('0' <= c && c <= '9') {
      hand_piece_num = hand_piece_num * 10 + (c - '0');
      continue;
    }

    // If a piece symbol, set the held piece
    // If no piece count is specified, assume 1
    if (SFEN_HAND_PIECE_TYPES.count(c) > 0) {
      auto [color, piece] = SFEN_HAND_PIECE_TYPES.at(c);

      if (hand_piece_num == 0) {
        hand_piece_num = 1;
      }

      _addHand(color, piece, hand_piece_num);
    }

    // If not a digit, reset the piece count to 0
    hand_piece_num = 0;
  }

  // Set the current player's color
  if (c_sfen[board_sfen_length + 1] == 'b') {
    _color = COLOR_BLACK;
  } else if (c_sfen[board_sfen_length + 1] == 'w') {
    _color = COLOR_WHITE;
  }

  // Set the turn count
  _turn = static_cast<int16_t>(
      std::stoi(sfen.substr(board_sfen_length + 3 + hand_sfen_length + 1)) - 1);
}

/**
 * Advances the board state by making a move.
 * @param move Move to make
 * @return Result of the move
 */
MoveResult Board::play(const Move& move) {
  Position src = move.getSrc();
  Position dst = move.getDst();
  uint8_t captured_piece = PIECE_EMPTY;

  // Dropping a piece from hand
  if (src.getX() == BOARD_SIZE) {
    // Decrease the held piece count
    uint8_t hand_piece = src.getY();

    _removeHand(_color, hand_piece);

    // Place the piece
    uint8_t dst_piece =
        (_color == COLOR_BLACK)
            ? (hand_piece - PIECE_HAND_BEGIN + PIECE_BLACK_BEGIN)
            : (hand_piece - PIECE_HAND_BEGIN + PIECE_WHITE_BEGIN);

    _putPiece(dst, dst_piece);
  }
  // Moving a piece on the board
  else {
    // If there is a piece at the destination, capture it
    captured_piece = _cells[dst.getIndex()];

    if (captured_piece != PIECE_EMPTY) {
      int32_t hand_piece =
          (captured_piece < PIECE_WHITE_BEGIN)
              ? (captured_piece - PIECE_BLACK_BEGIN + PIECE_HAND_BEGIN)
              : (captured_piece - PIECE_WHITE_BEGIN + PIECE_HAND_BEGIN);

      // For a promoted piece, convert to the unpromoted piece type
      if (hand_piece >= PIECE_HAND_END) {
        hand_piece -= PIECE_PROMOTE;
      }

      // Remove the piece at the destination
      _removePiece(dst);

      // Add the captured piece to the held pieces
      _addHand(_color, hand_piece);
    }

    // Get the piece at the source
    uint8_t src_piece = _cells[src.getIndex()];

    // Apply promotion
    if (move.isPromote()) {
      src_piece += PIECE_PROMOTE;
    }

    // Move the piece
    _removePiece(src);
    _putPiece(dst, src_piece);
  }

  // Switch the current player
  _color = (_color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;

  // Increment the turn count
  _turn += 1;

  // Save the move
  _lastMove = move;

  // Return the move result
  return MoveResult(move, captured_piece);
}

/**
 * Reverts the board to the state before the given move was made.
 * This function assumes the given move is the immediately preceding move.
 * @param result Result of the move to undo
 */
void Board::undo(const MoveResult& result) {
  Move move = result.getMove();
  Position src = move.getSrc();
  Position dst = move.getDst();
  uint8_t captured_piece = result.getCaptured();

  // Switch the current player
  _color = (_color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;

  // Decrement the turn count
  _turn -= 1;

  // Reset the saved move
  _lastMove = MOVE_INVALID;

  // Restore pieces to their original positions
  if (src.getX() == BOARD_SIZE) {
    // If a piece was dropped from hand, remove it and increase the held piece count
    _removePiece(dst);
    _addHand(_color, src.getY());
  } else {
    // If a piece was moved on the board, move it back and restore any captured piece
    uint8_t dst_piece = _cells[dst.getIndex()];

    if (move.isPromote()) {
      dst_piece -= PIECE_PROMOTE;
    }

    _removePiece(dst);
    _putPiece(src, dst_piece);

    // If there was a captured piece, restore it
    if (captured_piece != PIECE_EMPTY) {
      uint8_t hand_piece =
          (captured_piece < PIECE_WHITE_BEGIN)
              ? (captured_piece - PIECE_BLACK_BEGIN + PIECE_HAND_BEGIN)
              : (captured_piece - PIECE_WHITE_BEGIN + PIECE_HAND_BEGIN);

      if (hand_piece >= PIECE_HAND_END) {
        hand_piece -= PIECE_PROMOTE;
      }

      _putPiece(dst, captured_piece);
      _removeHand(_color, hand_piece);
    }
  }
}

/**
 * Returns the list of pieces attacking the specified coordinate.
 * @param position Coordinate to check
 * @return List of positions of pieces attacking the specified coordinate
 */
std::vector<Position> Board::getAttackers(const Position& position) const {
  // Reserve capacity in advance, as up to 10 pieces may be attacking
  std::vector<Position> attackers;
  attackers.reserve(10);

  // Get the positions of pieces attacking the specified coordinate
  for (int8_t color : {COLOR_BLACK, COLOR_WHITE}) {
    for (int8_t attacker : _getAttackers<false, false>(color, position.getIndex())) {
      attackers.emplace_back(attacker);
    }
  }

  return attackers;
}

/**
 * Returns the list of legal moves for the current board state.
 * @param removeUnpromote If true, removes non-promotion moves for pawn, bishop, rook, and lance on the 2nd rank
 * @param checkOnly If true, returns only moves that cause check
 * @return List of legal moves
 */
std::vector<Move> Board::getLegalMoves(bool removeUnpromote, bool checkOnly) const {
  std::vector<Move> legal_moves;

  if (checkOnly) {
    legal_moves.reserve(16);
  } else {
    legal_moves.reserve(64);
  }

  if (removeUnpromote) {
    if (checkOnly) {
      _getLegalMoves<true, true>(legal_moves);
    } else {
      _getLegalMoves<true, false>(legal_moves);
    }
  } else {
    if (checkOnly) {
      _getLegalMoves<false, true>(legal_moves);
    } else {
      _getLegalMoves<false, false>(legal_moves);
    }
  }

  return legal_moves;
}

/**
 * Returns the sequence of moves leading to checkmate for the current board state.
 * @param depth Depth of the checkmate search
 * @return Checkmate move sequence
 */
std::vector<Move> Board::getCheckmateMoves(int32_t depth) const {
  // Create a board copy for computation
  // Prepare a search board since the Board object's contents change during the search
  Board clone_board(*this);

  // Execute the checkmate search
  std::vector<Move> moves = searchCheckmateMoves(clone_board, depth);

  // Return the checkmate move sequence (stored in reverse order, so reverse it)
  std::reverse(moves.begin(), moves.end());

  return moves;
}

/**
 * Returns true if an entering-king declaration is possible.
 * @param color Color of the declaring side
 * @return True if an entering-king declaration is possible
 */
bool Board::isNyugyoku(int8_t color) const {
  int8_t my_color_idx = (color == COLOR_BLACK) ? 0 : 1;

  // [Condition 1] It is the declaring side's turn
  // [Condition 6] The declaring side has time remaining

  // [Condition 5] The declaring side's king is not in check
  if (isCheck(color)) {
    return false;
  }

  // [Condition 2] The declaring side's king has entered within the opponent's third rank
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

  if (king_pos_idx < 0 ||
      !BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(king_pos_idx)) {
    return false;
  }

  // [Condition 4] The declaring side has 10 or more pieces (excluding the king) within the opponent's third rank
  // Since the king has entered enemy territory, at least 11 pieces in total are required
  BitBoard invasion_bitboard =
      _colorBitBoards[my_color_idx] & BITBOARD_ENEMY_AREAS[my_color_idx];

  if (invasion_bitboard.countBit() < 11) {
    return false;
  }

  // [Condition 3] The declaring side calculates with 5 points for major pieces and 1 point for minor pieces
  // Only the declaring side's held pieces and pieces in enemy territory (excluding the king) are counted.
  BitBoard bishop_rook_bitboard =
      (_pieceBitBoards[my_color_idx][PIECE_BLACK_BISHOP - PIECE_BLACK_BEGIN] |
       _pieceBitBoards[my_color_idx][PIECE_BLACK_ROOK - PIECE_BLACK_BEGIN]) &
      invasion_bitboard;
  int8_t nyugyoku_score =
      (invasion_bitboard.countBit() - 1) +
      (bishop_rook_bitboard.countBit() * 4) +
      _hands[my_color_idx][PIECE_HAND_ROOK - PIECE_HAND_BEGIN] * 5 +
      _hands[my_color_idx][PIECE_HAND_BISHOP - PIECE_HAND_BEGIN] * 5 +
      _hands[my_color_idx][PIECE_HAND_PAWN - PIECE_HAND_BEGIN] +
      _hands[my_color_idx][PIECE_HAND_LANCE - PIECE_HAND_BEGIN] +
      _hands[my_color_idx][PIECE_HAND_KNIGHT - PIECE_HAND_BEGIN] +
      _hands[my_color_idx][PIECE_HAND_SILVER - PIECE_HAND_BEGIN] +
      _hands[my_color_idx][PIECE_HAND_GOLD - PIECE_HAND_BEGIN];

  // Check if the required score for entering-king declaration is met
  return nyugyoku_score >= _nyugyokuScores[my_color_idx];
}

/**
 * Returns true if the specified color's king is in check.
 * @param color Color of the side being checked
 * @return True if the king is in check
 */
bool Board::isCheck(int8_t color) const {
  int8_t color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int8_t king_pos_idx = _kingPositions[color_idx].getIndex();

  // If the king is not on the board, check cannot be applied
  if (king_pos_idx < 0) {
    return false;
  }
  // If the king exists, check whether it is in check
  else {
    return !_getAttackers<true, false>(color, king_pos_idx).empty();
  }
}

/**
 * Returns the SFEN-format string.
 * @return SFEN-format string
 */
std::string Board::getSfen() const {
  std::stringstream ss;

  // Set the board information
  for (int32_t y = 0; y < BOARD_SIZE; y++) {
    int32_t empty_count = 0;

    for (int32_t x = BOARD_SIZE - 1; x >= 0; x--) {
      uint8_t piece = _cells[y + BOARD_SIZE * x];

      if (piece == PIECE_EMPTY) {
        empty_count++;
        continue;
      }

      if (empty_count > 0) {
        ss << empty_count;
        empty_count = 0;
      }

      ss << SFEN_PIECE_NAMES.at(piece);
    }

    if (empty_count > 0) {
      ss << empty_count;
    }

    if (y < BOARD_SIZE - 1) {
      ss << "/";
    }
  }

  // Set the current player's color
  if (_color == COLOR_BLACK) {
    ss << " b ";
  } else {
    ss << " w ";
  }

  // Set the held piece information
  bool has_hand = false;

  for (int8_t color_idx = 0; color_idx < 2; color_idx++) {
    for (int32_t i = 0; i < PIECE_HAND_END - PIECE_HAND_BEGIN; i++) {
      uint8_t hand_piece = SFEN_HAND_PIECES[i];
      const char* hand_name = SFEN_HAND_PIECE_NAMES[color_idx].at(hand_piece);
      uint8_t hand_count = _hands[color_idx][hand_piece - PIECE_HAND_BEGIN];

      if (hand_count > 0) {
        if (hand_count > 1) {
          ss << static_cast<int32_t>(hand_count);
        }

        ss << hand_name;
        has_hand = true;
      }
    }
  }

  if (!has_hand) {
    ss << "-";
  }

  // Set the turn count
  ss << " " << (_turn + 1);

  return ss.str();
}

/**
 * Returns the model input data.
 * @param inputs Model input data
 */
void Board::getInputs(int32_t* inputs) const {
  getInputs(inputs, _color);
}

/**
 * Returns the model input data.
 * @param inputs Model input data
 * @param color Player's color
 */
void Board::getInputs(int32_t* inputs, int8_t color) const {
  // Initialize
  std::fill_n(inputs, MODEL_INPUT_PACK_SIZE, 0);

  // Retrieve the model input data
  _getBoardInputs(inputs, color);
  _getInfoInputs(inputs, color);
}

/**
 * Copies the board state from another board.
 * @param board Source board
 */
void Board::copyFrom(const Board* board) {
  *this = *board;
}

/**
 * Returns the string representation of the board state.
 * @return String representation of the board state
 */
std::string Board::toString() const {
  // CSA-format strings for displaying piece information.
  const char piece_names[][3] = {
      "FU", "KY", "KE", "GI", "KA", "HI", "KI", "OU",
      "TO", "NY", "NK", "NG", "UM", "RY"};
  const char hand_color_names[][3] = {"P+", "P-"};

  // Stream object for building the string
  std::stringstream ss;

  // Build the color and turn number string
  ss << "Color: " << ((_color == COLOR_WHITE) ? "white" : "black");
  ss << ", Turn: " << _turn << std::endl;

  // Build the piece information string for the board
  for (int32_t y = 0; y < BOARD_SIZE; y++) {
    ss << "P" << y + 1;

    for (int32_t x = BOARD_SIZE - 1; x >= 0; x--) {
      uint8_t piece = _cells[y + x * BOARD_SIZE];

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

  // Build the held piece information string
  for (int32_t c = 0; c < 2; c++) {
    ss << hand_color_names[c];

    for (int32_t i = 0; i < PIECE_HAND_END - PIECE_HAND_BEGIN; i++) {
      for (int32_t j = 0; j < _hands[c][i]; j++) {
        ss << "00" << piece_names[i];
      }
    }

    if (c == 0) {
      ss << std::endl;
    }
  }

  return ss.str();
}

/**
 * Places a piece at the specified position.
 * This method assumes that the specified position does not already have a piece.
 * @param pos Coordinate at which to place the piece
 * @param piece Integer value representing the piece to place
 */
void Board::_putPiece(const Position& pos, uint8_t piece) {
  int8_t pos_idx = pos.getIndex();

  // If the specified position already has a piece, throw an exception
  if (_cells[pos_idx] != PIECE_EMPTY) {
    throw std::invalid_argument("Position already has a piece");
  }

  // Update the hash value
  _cellHash ^= BOARD_HASH_VALUES[pos_idx][piece];

  // Place the piece at the specified position
  _cells[pos_idx] = piece;

  // Update the bitboard according to the piece type
  int32_t color_idx = (piece < PIECE_WHITE_BEGIN) ? 0 : 1;
  int32_t piece_idx = (piece < PIECE_WHITE_BEGIN)
                          ? (piece - PIECE_BLACK_BEGIN)
                          : (piece - PIECE_WHITE_BEGIN);

  _colorBitBoards[color_idx].setBit(pos_idx);

  if (piece_idx < PIECE_BLACK_PRO_PAWN - PIECE_BLACK_BEGIN) {
    _pieceBitBoards[color_idx][piece_idx].setBit(pos_idx);
  } else if (piece_idx < PIECE_BLACK_HORSE - PIECE_BLACK_BEGIN) {
    _pieceBitBoards[color_idx][PIECE_BLACK_GOLD - PIECE_BLACK_BEGIN].setBit(pos_idx);
  } else if (piece_idx == PIECE_BLACK_HORSE - PIECE_BLACK_BEGIN) {
    _pieceBitBoards[color_idx][PIECE_BLACK_BISHOP - PIECE_BLACK_BEGIN].setBit(pos_idx);
    _pieceBitBoards[color_idx][PIECE_BLACK_KING - PIECE_BLACK_BEGIN].setBit(pos_idx);
  } else if (piece_idx == PIECE_BLACK_DRAGON - PIECE_BLACK_BEGIN) {
    _pieceBitBoards[color_idx][PIECE_BLACK_ROOK - PIECE_BLACK_BEGIN].setBit(pos_idx);
    _pieceBitBoards[color_idx][PIECE_BLACK_KING - PIECE_BLACK_BEGIN].setBit(pos_idx);
  }

  // Update the king's position
  if (piece == PIECE_BLACK_KING) {
    _kingPositions[0] = pos;
  } else if (piece == PIECE_WHITE_KING) {
    _kingPositions[1] = pos;
  }
}

/**
 * Removes the piece at the specified position.
 * @param pos Coordinate from which to remove the piece
 */
void Board::_removePiece(const Position& pos) {
  int8_t pos_idx = pos.getIndex();
  uint8_t piece = _cells[pos_idx];

  // If the specified position has no piece, throw an exception
  if (piece == PIECE_EMPTY) {
    throw std::invalid_argument("Position does not have a piece");
  }

  // Update the hash value
  _cellHash ^= BOARD_HASH_VALUES[pos_idx][piece];

  // Remove the piece from the specified position
  _cells[pos_idx] = PIECE_EMPTY;

  // Update the bitboard according to the piece type
  // Pro-pawn, pro-lance, pro-knight, and pro-silver share the gold bitboard
  // Horse uses the bishop and king bitboards
  // Dragon uses the rook and king bitboards
  int32_t color_idx = (piece < PIECE_WHITE_BEGIN) ? 0 : 1;
  int32_t piece_idx = (piece < PIECE_WHITE_BEGIN)
                          ? (piece - PIECE_BLACK_BEGIN)
                          : (piece - PIECE_WHITE_BEGIN);

  _colorBitBoards[color_idx].clearBit(pos_idx);

  if (piece_idx < PIECE_BLACK_PRO_PAWN - PIECE_BLACK_BEGIN) {
    _pieceBitBoards[color_idx][piece_idx].clearBit(pos_idx);
  } else if (piece_idx < PIECE_BLACK_HORSE - PIECE_BLACK_BEGIN) {
    _pieceBitBoards[color_idx][PIECE_BLACK_GOLD - PIECE_BLACK_BEGIN].clearBit(pos_idx);
  } else if (piece_idx == PIECE_BLACK_HORSE - PIECE_BLACK_BEGIN) {
    _pieceBitBoards[color_idx][PIECE_BLACK_BISHOP - PIECE_BLACK_BEGIN].clearBit(pos_idx);
    _pieceBitBoards[color_idx][PIECE_BLACK_KING - PIECE_BLACK_BEGIN].clearBit(pos_idx);
  } else if (piece_idx == PIECE_BLACK_DRAGON - PIECE_BLACK_BEGIN) {
    _pieceBitBoards[color_idx][PIECE_BLACK_ROOK - PIECE_BLACK_BEGIN].clearBit(pos_idx);
    _pieceBitBoards[color_idx][PIECE_BLACK_KING - PIECE_BLACK_BEGIN].clearBit(pos_idx);
  }

  // Update the king's position
  if (piece == PIECE_BLACK_KING) {
    _kingPositions[0] = POSITION_INVALID;
  } else if (piece == PIECE_WHITE_KING) {
    _kingPositions[1] = POSITION_INVALID;
  }
}

/**
 * Adds a piece to the held pieces.
 * @param color Player's color (COLOR_BLACK or COLOR_WHITE)
 * @param piece Integer value representing the piece to add
 * @param num Number of pieces to add
 */
void Board::_addHand(int8_t color, uint8_t piece, int32_t num) {
  int32_t color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int32_t piece_idx = piece - PIECE_HAND_BEGIN;

  // Update the held piece bit representation
  int8_t offset = HAND_BIT_OFFSETS[piece_idx] + _hands[color_idx][piece_idx];

  for (int8_t i = 0; i < num; i++) {
    _handBits[color_idx] |= (1ULL << (offset + i));
  }

  // Add the piece to the held pieces
  _hands[color_idx][piece_idx] += num;
}

/**
 * Adds a piece to the held pieces.
 * @param color Player's color (COLOR_BLACK or COLOR_WHITE)
 * @param piece Integer value representing the piece to add
 */
void Board::_addHand(int8_t color, uint8_t piece) {
  _addHand(color, piece, 1);
}

/**
 * Removes a piece from the held pieces.
 * This function assumes that the specified piece is present in the held pieces.
 * @param color Player's color (COLOR_BLACK or COLOR_WHITE)
 * @param piece Integer value representing the piece to remove
 */
void Board::_removeHand(int8_t color, uint8_t piece) {
  int32_t color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int32_t piece_idx = piece - PIECE_HAND_BEGIN;

  // If the specified piece is not in the held pieces, throw an exception
  if (_hands[color_idx][piece_idx] <= 0) {
    throw std::invalid_argument("Hand does not have the specified piece");
  }

  // Update the held piece bit representation
  int8_t offset = HAND_BIT_OFFSETS[piece_idx] + _hands[color_idx][piece_idx];

  _handBits[color_idx] &= ~(1ULL << (offset - 1));

  // Remove the piece from the held pieces
  _hands[color_idx][piece_idx] -= 1;
}

/**
 * Returns a list of positions of pieces attacking the specified coordinate.
 * If the template argument returnOnFirstAttacker is true, returns only the first attacker found.
 * If additionalOccIndex is specified, that position is treated as an additional occupied square when computing sliding piece attacks.
 * If the template argument removeOwnKing is true, the own king's position is removed from the occupancy bitboard when computing sliding piece attacks.
 * @param color Color of the side being attacked
 * @param posIndex Coordinate to check for attacking pieces
 * @param additionalOccIndex Coordinate to treat as additionally occupied (-1 if none)
 * @return List of positions of pieces attacking the specified coordinate
 */
template <bool returnOnFirstAttacker, bool removeOwnKing>
std::vector<int8_t> Board::_getAttackers(
    int8_t color, int8_t posIndex, int8_t additionalOccIndex) const {
  // Calculate the color indices for the attacked side and the attacking side
  int8_t my_color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;

  // Object to store attacker positions
  // Reserve capacity for up to 10 positions in advance
  std::vector<int8_t> attacker_indices;

  if constexpr (returnOnFirstAttacker) {
    attacker_indices.reserve(1);
  } else {
    attacker_indices.reserve(10);
  }

  // Check pawn attacks
  BitBoard pawn_bitboard =
      BITBOARD_PAWN_ATTACKS[my_color_idx][posIndex] &
      _pieceBitBoards[op_color_idx][PAWN_INDEX];

  while (pawn_bitboard) {
    attacker_indices.push_back(pawn_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Check knight attacks
  BitBoard knight_bitboard =
      BITBOARD_KNIGHT_ATTACKS[my_color_idx][posIndex] &
      _pieceBitBoards[op_color_idx][KNIGHT_INDEX];

  while (knight_bitboard) {
    attacker_indices.push_back(knight_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Check silver attacks
  BitBoard silver_bitboard =
      BITBOARD_SILVER_ATTACKS[my_color_idx][posIndex] &
      _pieceBitBoards[op_color_idx][SILVER_INDEX];

  while (silver_bitboard) {
    attacker_indices.push_back(silver_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Check gold attacks
  BitBoard gold_bitboard =
      BITBOARD_GOLD_ATTACKS[my_color_idx][posIndex] &
      _pieceBitBoards[op_color_idx][GOLD_INDEX];

  while (gold_bitboard) {
    attacker_indices.push_back(gold_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Check king attacks
  // Create a mask to exclude positions already found via king attacks,
  // since king and bishop/rook attacks may overlap
  BitBoard king_bitboard =
      BITBOARD_KING_ATTACKS[posIndex] & _pieceBitBoards[op_color_idx][KING_INDEX];
  BitBoard horse_dragon_bitboard = king_bitboard;

  while (king_bitboard) {
    attacker_indices.push_back(king_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Build the occupancy bitboard for positions relevant to lance, bishop, and rook attacks
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
  BitBoard lance_attack_bitboard;
  BitBoard bishop_attack_bitboard;
  BitBoard rook_attack_bitboard;

  // If removeOwnKing is true, remove the own king's position from the occupancy bitboard
  if constexpr (removeOwnKing) {
    int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

    if (king_pos_idx >= 0) {
      occ_bitboard.clearBit(king_pos_idx);
    }
  }

  // If additionalOccIndex is not -1, treat that position as additionally occupied
  if (additionalOccIndex >= 0) {
    occ_bitboard.setBit(additionalOccIndex);
  }

  // Check attacks in the upward direction
  int8_t up_index =
      (BITBOARD_LONG_ATTACKS[0][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (up_index >= 0 && !horse_dragon_bitboard.hasBit(up_index)) {
    if (color == COLOR_BLACK) {
      lance_attack_bitboard.setBit(up_index);
    }

    rook_attack_bitboard.setBit(up_index);
  }

  // Check attacks in the upper-right direction
  int8_t up_right_index =
      (BITBOARD_LONG_ATTACKS[1][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (up_right_index >= 0 && !horse_dragon_bitboard.hasBit(up_right_index)) {
    bishop_attack_bitboard.setBit(up_right_index);
  }

  // Check attacks in the rightward direction
  int8_t right_index =
      (BITBOARD_LONG_ATTACKS[2][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (right_index >= 0 && !horse_dragon_bitboard.hasBit(right_index)) {
    rook_attack_bitboard.setBit(right_index);
  }

  // Check attacks in the lower-right direction
  int8_t down_right_index =
      (BITBOARD_LONG_ATTACKS[3][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (down_right_index >= 0 && !horse_dragon_bitboard.hasBit(down_right_index)) {
    bishop_attack_bitboard.setBit(down_right_index);
  }

  // Check attacks in the downward direction
  int8_t down_index =
      (BITBOARD_LONG_ATTACKS[4][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (down_index >= 0 && !horse_dragon_bitboard.hasBit(down_index)) {
    if (color == COLOR_WHITE) {
      lance_attack_bitboard.setBit(down_index);
    }

    rook_attack_bitboard.setBit(down_index);
  }

  // Check attacks in the lower-left direction
  int8_t down_left_index =
      (BITBOARD_LONG_ATTACKS[5][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (down_left_index >= 0 && !horse_dragon_bitboard.hasBit(down_left_index)) {
    bishop_attack_bitboard.setBit(down_left_index);
  }

  // Check attacks in the leftward direction
  int8_t left_index =
      (BITBOARD_LONG_ATTACKS[6][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (left_index >= 0 && !horse_dragon_bitboard.hasBit(left_index)) {
    rook_attack_bitboard.setBit(left_index);
  }

  // Check attacks in the upper-left direction
  int8_t up_left_index =
      (BITBOARD_LONG_ATTACKS[7][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (up_left_index >= 0 && !horse_dragon_bitboard.hasBit(up_left_index)) {
    bishop_attack_bitboard.setBit(up_left_index);
  }

  // Register the coordinate if there is a lance attack
  BitBoard lance_bitboard =
      lance_attack_bitboard & _pieceBitBoards[op_color_idx][LANCE_INDEX];

  if (lance_bitboard) {
    attacker_indices.push_back(lance_bitboard.getRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Register the coordinate if there is a bishop attack
  BitBoard bishop_bitboard =
      bishop_attack_bitboard & _pieceBitBoards[op_color_idx][BISHOP_INDEX];

  while (bishop_bitboard) {
    attacker_indices.push_back(bishop_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Register the coordinate if there is a rook attack
  BitBoard rook_bitboard =
      rook_attack_bitboard & _pieceBitBoards[op_color_idx][ROOK_INDEX];

  while (rook_bitboard) {
    attacker_indices.push_back(rook_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  return attacker_indices;
}

/**
 * Returns the list of legal moves for the current board state.
 * If the template argument removeUnpromote is true, removes non-promotion moves for pawn, bishop, rook, and lance on the 2nd rank.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the list of legal moves to
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalMoves(std::vector<Move>& legalMoves) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;

  // Check the check status
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();
  std::vector<int8_t> checking_piece_indices;

  if (king_pos_idx >= 0) {
    checking_piece_indices = _getAttackers<false, false>(_color, king_pos_idx);
  }

  // If not in check (0 attackers), all legal moves are valid
  // If in check by 1 attacker, capturing that piece, moving the king, or interposing are legal
  // If in double check (2+ attackers), only king moves are legal
  BitBoard destination_bitboard;  // initialized to 0

  if (checking_piece_indices.empty()) {
    destination_bitboard = ~_colorBitBoards[my_color_idx];
  } else if (checking_piece_indices.size() == 1) {
    int8_t op_pos_idx = checking_piece_indices[0];
    int8_t direction = DIRECTION_INDICES[king_pos_idx][op_pos_idx];

    if (direction != 8) {
      destination_bitboard =
          BITBOARD_LONG_ATTACKS[direction][king_pos_idx] &
          BITBOARD_LONG_ATTACKS[(direction + 4) % 8][op_pos_idx];
    }

    destination_bitboard.setBit(op_pos_idx);
  }

  // Generate moves for pieces on the board
  // Since one-square horse and dragon moves can overlap with king and horse/dragon moves,
  // first generate king, horse, and dragon moves and remove duplicates
  _getLegalKingMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalBishopMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);
  _getLegalRookMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);

  // Remove duplicate moves
  std::sort(legalMoves.begin(), legalMoves.end());
  legalMoves.erase(std::unique(legalMoves.begin(), legalMoves.end()), legalMoves.end());

  // Generate legal moves for pawn, lance, knight, silver, and gold
  _getLegalPawnMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);
  _getLegalLanceMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);
  _getLegalKnightMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalSilverMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalGoldMoves<checkOnly>(legalMoves, destination_bitboard);

  // Generate legal drop moves for pawn, lance, knight, silver, gold, bishop, and rook
  // Since these are hand-piece drops, exclude moves that would capture opponent pieces
  destination_bitboard &= ~_colorBitBoards[op_color_idx];
  _getLegalHandPawnMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalHandLanceMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalHandKnightMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalHandSilverMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalHandGoldMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalHandBishopMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalHandRookMoves<checkOnly>(legalMoves, destination_bitboard);
}

/**
 * Generates legal moves for pawn moves.
 * If the template argument removeUnpromote is true, removes non-promotion moves.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal pawn moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for pawn moves
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalPawnMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Build the pawn bitboard
  BitBoard pawn_bitboard = _pieceBitBoards[my_color_idx][PAWN_INDEX];

  // Generate legal moves for each pawn
  while (pawn_bitboard) {
    int8_t src_idx = pawn_bitboard.popRightmostBitIndex();
    BitBoard move_bitboard =
        BITBOARD_PAWN_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // If no destination exists, check the next pawn
    if (!move_bitboard) {
      continue;
    }

    // Get the destination coordinate
    int8_t dst_idx = move_bitboard.getRightmostBitIndex();

    // Check if the move is valid (verify it does not result in a discovered check on own king)
    if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
      continue;
    }

    // Check if promotion is possible
    bool promote = BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

    // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
    if constexpr (checkOnly) {
      bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                             : _isCheckMove<PIECE_BLACK_PAWN>(src_idx, dst_idx);
      if (check) {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }
    } else {
      legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
    }

    // Register the promotion move; if removeUnpromote is false, also register the non-promotion move
    // However, if moving to the 1st rank, promotion is mandatory, so the non-promotion move is not registered
    if constexpr (!removeUnpromote) {
      if (promote && BITBOARD_PAWN_DROPABLES[my_color_idx].hasBit(dst_idx)) {
        // If restricting to check-only moves, register only if it gives check; otherwise register unconditionally
        if constexpr (checkOnly) {
          if (_isCheckMove<PIECE_BLACK_PAWN>(src_idx, dst_idx)) {
            legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
          }
        } else {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
        }
      }
    }
  }
}

/**
 * Generates legal moves for lance moves.
 * If the template argument removeUnpromote is true, removes non-promotion moves for the 2nd rank.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal lance moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for lance moves
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalLanceMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Build the lance bitboard
  BitBoard lance_bitboard = _pieceBitBoards[my_color_idx][LANCE_INDEX];

  // Generate legal moves for each lance
  BitBoard occupied_bitboard = _colorBitBoards[0] | _colorBitBoards[1];

  while (lance_bitboard) {
    int8_t src_idx = lance_bitboard.popRightmostBitIndex();

    // Build the lance destination bitboard
    BitBoard move_bitboard;

    if (_color == COLOR_BLACK) {
      int8_t target_idx =
          (BITBOARD_LONG_ATTACKS[0][src_idx] & occupied_bitboard).getLeftmostBitIndex();

      if (target_idx == -1) {
        target_idx = BITBOARD_LONG_ATTACKS[0][src_idx].getRightmostBitIndex();
      }

      move_bitboard = BITBOARD_LONG_ATTACKS[0][src_idx] & BITBOARD_LONG_ATTACKS[4][target_idx];
      move_bitboard.setBit(target_idx);
    } else {
      int8_t target_idx =
          (BITBOARD_LONG_ATTACKS[4][src_idx] & occupied_bitboard).getRightmostBitIndex();

      if (target_idx == -1) {
        target_idx = BITBOARD_LONG_ATTACKS[4][src_idx].getLeftmostBitIndex();
      }

      move_bitboard = BITBOARD_LONG_ATTACKS[4][src_idx] & BITBOARD_LONG_ATTACKS[0][target_idx];
      move_bitboard.setBit(target_idx);
    }

    move_bitboard &= destinationBitBoard;

    // Generate legal moves for each destination
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // Check if the move is valid (verify it does not result in a discovered check on own king)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // Check if promotion is possible
      bool promote = BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

      // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
      if constexpr (checkOnly) {
        bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                               : _isCheckMove<PIECE_BLACK_LANCE>(src_idx, dst_idx);
        if (check) {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
        }
      } else {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }

      // If removeUnpromote is false, also register non-promotion moves for the 2nd and 3rd ranks
      // If removeUnpromote is true, register only the non-promotion move for the 3rd rank
      if constexpr (removeUnpromote) {
        // The 3rd-rank check reuses the knight-drop bitboard
        if (promote && BITBOARD_KNIGHT_DROPABLES[my_color_idx].hasBit(dst_idx)) {
          // If restricting to check-only moves, register only if it gives check; otherwise register unconditionally
          if constexpr (checkOnly) {
            if (_isCheckMove<PIECE_BLACK_LANCE>(src_idx, dst_idx)) {
              legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
            }
          } else {
            legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
          }
        }
      } else {
        // The 2nd-rank check reuses the pawn-drop bitboard
        if (promote && BITBOARD_PAWN_DROPABLES[my_color_idx].hasBit(dst_idx)) {
          // If restricting to check-only moves, register only if it gives check; otherwise register unconditionally
          if constexpr (checkOnly) {
            if (_isCheckMove<PIECE_BLACK_LANCE>(src_idx, dst_idx)) {
              legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
            }
          } else {
            legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
          }
        }
      }
    }
  }
}

/**
 * Generates legal moves for knight moves.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal knight moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for knight moves
 */
template <bool checkOnly>
void Board::_getLegalKnightMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Build the knight bitboard
  BitBoard knight_bitboard = _pieceBitBoards[my_color_idx][KNIGHT_INDEX];

  // Generate legal moves for each knight
  while (knight_bitboard) {
    int8_t src_idx = knight_bitboard.popRightmostBitIndex();
    BitBoard move_bitboard =
        BITBOARD_KNIGHT_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // Generate legal moves for each destination (knight)
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // Check if the move is valid (verify it does not result in a discovered check on own king)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // Check if promotion is possible
      bool promote = BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

      // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
      if constexpr (checkOnly) {
        bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                               : _isCheckMove<PIECE_BLACK_KNIGHT>(src_idx, dst_idx);
        if (check) {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
        }
      } else {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }

      // If a promotion move was registered, also register the non-promotion move
      if (promote && BITBOARD_KNIGHT_DROPABLES[my_color_idx].hasBit(dst_idx)) {
        // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
        if constexpr (checkOnly) {
          if (_isCheckMove<PIECE_BLACK_KNIGHT>(src_idx, dst_idx)) {
            legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
          }
        } else {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
        }
      }
    }
  }
}

/**
 * Generates legal moves for silver moves.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal silver moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for silver moves
 */
template <bool checkOnly>
void Board::_getLegalSilverMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Build the silver bitboard
  BitBoard silver_bitboard = _pieceBitBoards[my_color_idx][SILVER_INDEX];

  // Generate legal moves for each silver
  while (silver_bitboard) {
    int8_t src_idx = silver_bitboard.popRightmostBitIndex();

    // Build the silver destination bitboard
    BitBoard move_bitboard =
        BITBOARD_SILVER_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // Generate legal moves for each destination (silver)
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // Check if the move is valid (verify it does not result in a discovered check on own king)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // Check if promotion is possible
      bool promote =
          BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(src_idx) ||
          BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

      // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
      if constexpr (checkOnly) {
        bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                               : _isCheckMove<PIECE_BLACK_SILVER>(src_idx, dst_idx);
        if (check) {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
        }
      } else {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }

      // If a promotion move was registered, also register the non-promotion move
      if (promote) {
        // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
        if constexpr (checkOnly) {
          if (_isCheckMove<PIECE_BLACK_SILVER>(src_idx, dst_idx)) {
            legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
          }
        } else {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
        }
      }
    }
  }
}

/**
 * Generates legal moves for gold moves.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal gold moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for gold moves
 */
template <bool checkOnly>
void Board::_getLegalGoldMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Build the gold bitboard
  BitBoard gold_bitboard = _pieceBitBoards[my_color_idx][GOLD_INDEX];

  // Generate legal moves for each gold
  while (gold_bitboard) {
    int8_t src_idx = gold_bitboard.popRightmostBitIndex();

    // Build the gold destination bitboard
    BitBoard move_bitboard =
        BITBOARD_GOLD_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // Generate legal moves for each destination (gold)
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // Check if the move is valid (verify it does not result in a discovered check on own king)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
      if constexpr (checkOnly) {
        if (_isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)) {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
        }
      } else {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
      }
    }
  }
}

/**
 * Generates legal moves for king moves.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal king moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for king moves
 */
template <bool checkOnly>
void Board::_getLegalKingMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

  // Generate legal moves for the king
  // Skip king move generation if the king position is not set
  // King move destinations are not restricted by destinationBitBoard
  // The king can only move to squares with no own pieces and no opponent attacks
  // Since the king cannot give check, in check-only mode only register discovered checks
  if (king_pos_idx >= 0) {
    BitBoard king_move_bitboard =
        BITBOARD_KING_ATTACKS[king_pos_idx] & ~_colorBitBoards[my_color_idx];

    // Generate legal moves for each destination (king)
    while (king_move_bitboard) {
      int8_t dst_idx = king_move_bitboard.popRightmostBitIndex();

      // Do not register suicide moves
      if (!_getAttackers<true, true>(_color, dst_idx).empty()) {
        continue;
      }

      // If restricting to check-only moves, register only moves that cause discovered check; otherwise register unconditionally
      if constexpr (checkOnly) {
        if (_isDiscoveredCheckMove(king_pos_idx, dst_idx, OPPOSITE_COLOR(_color))) {
          legalMoves.emplace_back(Position(king_pos_idx), Position(dst_idx), false);
        }
      } else {
        legalMoves.emplace_back(Position(king_pos_idx), Position(dst_idx), false);
      }
    }
  }

  // Build the horse and dragon bitboard
  BitBoard bishop_bitboard = _pieceBitBoards[my_color_idx][BISHOP_INDEX];
  BitBoard other_bitboard = _pieceBitBoards[my_color_idx][KING_INDEX];

  if (king_pos_idx >= 0) {
    other_bitboard.clearBit(king_pos_idx);
  }

  // Generate legal moves for each piece (horse/dragon)
  while (other_bitboard) {
    int8_t src_idx = other_bitboard.popRightmostBitIndex();
    BitBoard move_bitboard = BITBOARD_KING_ATTACKS[src_idx] & destinationBitBoard;

    // Generate legal moves for each destination (horse/dragon)
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // Check if the move is valid (verify it does not result in a discovered check on own king)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
      if constexpr (checkOnly) {
        bool check = (bishop_bitboard.hasBit(src_idx))
                         ? _isCheckMove<PIECE_BLACK_HORSE>(src_idx, dst_idx)
                         : _isCheckMove<PIECE_BLACK_DRAGON>(src_idx, dst_idx);
        if (check) {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
        }
      } else {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
      }
    }
  }
}

/**
 * Generates legal moves for bishop moves.
 * If the template argument removeUnpromote is true, removes non-promotion moves.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal bishop moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for bishop moves
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalBishopMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Generate legal moves for bishops
  BitBoard bishop_bitboard = _pieceBitBoards[my_color_idx][BISHOP_INDEX];

  // Generate legal moves for each position (bishop)
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];

  while (bishop_bitboard) {
    int8_t src_idx = bishop_bitboard.popRightmostBitIndex();

    // Check if it has promoted to horse (position is set in the king bitboard)
    bool already_promoted =
        _pieceBitBoards[my_color_idx][KING_INDEX].hasBit(src_idx);

    // Check moves in 4 diagonal directions
    for (int8_t direction : {1, 3, 5, 7}) {
      BitBoard attack_bitboard = BITBOARD_LONG_ATTACKS[direction][src_idx] & occ_bitboard;
      int8_t attack_idx = (direction < 4)
                              ? attack_bitboard.getLeftmostBitIndex()
                              : attack_bitboard.getRightmostBitIndex();
      BitBoard move_bitboard = BITBOARD_LONG_ATTACKS[direction][src_idx];

      if (attack_idx >= 0) {
        move_bitboard &= BITBOARD_LONG_ATTACKS[(direction + 4) % 8][attack_idx];
        move_bitboard.setBit(attack_idx);
      }

      move_bitboard &= destinationBitBoard;

      // Generate legal moves for each destination (bishop)
      while (move_bitboard) {
        int8_t dst_idx = move_bitboard.popRightmostBitIndex();

        // Check if the move is valid (verify it does not result in a discovered check on own king)
        if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
          continue;
        }

        // Check if promotion is possible
        bool promote = !already_promoted &&
                       (BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(src_idx) ||
                        BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx));

        // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
        if constexpr (checkOnly) {
          bool check = (promote || already_promoted)
                           ? _isCheckMove<PIECE_BLACK_HORSE>(src_idx, dst_idx)
                           : _isCheckMove<PIECE_BLACK_BISHOP>(src_idx, dst_idx);
          if (check) {
            legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
          }
        } else {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
        }

        // If removing non-promotion moves, additionally check the non-promotion move
        if constexpr (!removeUnpromote) {
          // Add the non-promotion move
          if (promote) {
            // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
            if constexpr (checkOnly) {
              if (_isCheckMove<PIECE_BLACK_BISHOP>(src_idx, dst_idx)) {
                legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
              }
            } else {
              legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
            }
          }
        }
      }
    }
  }
}

/**
 * Generates legal moves for rook moves.
 * If the template argument removeUnpromote is true, removes non-promotion moves.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal rook moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for rook moves
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalRookMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Generate legal moves for rooks
  BitBoard rook_bitboard = _pieceBitBoards[my_color_idx][ROOK_INDEX];

  // Generate legal moves for each position (rook)
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];

  while (rook_bitboard) {
    int8_t src_idx = rook_bitboard.popRightmostBitIndex();

    // Check if it has promoted to dragon (position is set in the king bitboard)
    bool already_promoted =
        _pieceBitBoards[my_color_idx][KING_INDEX].hasBit(src_idx);

    // Check moves in 4 orthogonal directions
    for (int8_t direction : {0, 2, 4, 6}) {
      BitBoard attack_bitboard = BITBOARD_LONG_ATTACKS[direction][src_idx] & occ_bitboard;
      int8_t attack_idx = (direction < 4)
                              ? attack_bitboard.getLeftmostBitIndex()
                              : attack_bitboard.getRightmostBitIndex();
      BitBoard move_bitboard = BITBOARD_LONG_ATTACKS[direction][src_idx];

      if (attack_idx >= 0) {
        move_bitboard &= BITBOARD_LONG_ATTACKS[(direction + 4) % 8][attack_idx];
        move_bitboard.setBit(attack_idx);
      }

      move_bitboard &= destinationBitBoard;

      // Generate legal moves for each destination (rook)
      while (move_bitboard) {
        int8_t dst_idx = move_bitboard.popRightmostBitIndex();

        // Check if the move is valid (verify it does not result in a discovered check on own king)
        if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
          continue;
        }

        // Check if promotion is possible
        bool promote = !already_promoted &&
                       (BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(src_idx) ||
                        BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx));

        // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
        if constexpr (checkOnly) {
          bool check = (promote || already_promoted)
                           ? _isCheckMove<PIECE_BLACK_DRAGON>(src_idx, dst_idx)
                           : _isCheckMove<PIECE_BLACK_ROOK>(src_idx, dst_idx);
          if (check) {
            legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
          }
        } else {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
        }

        // If removing non-promotion moves, additionally check the non-promotion move
        if constexpr (!removeUnpromote) {
          // Add the non-promotion move
          if (promote) {
            // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
            if constexpr (checkOnly) {
              if (_isCheckMove<PIECE_BLACK_ROOK>(src_idx, dst_idx)) {
                legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
              }
            } else {
              legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
            }
          }
        }
      }
    }
  }
}

/**
 * Generates legal drop moves for pawns.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal pawn drop moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for pawn drops
 */
template <bool checkOnly>
void Board::_getLegalHandPawnMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;

  // If no pawn is in the hand, there are no legal moves
  if (_hands[my_color_idx][PAWN_INDEX] == 0) {
    return;
  }

  // Build a bitboard of files that already have a pawn
  BitBoard pawn_exist_bitboard;
  BitBoard pawn_piece_bitboard = _pieceBitBoards[my_color_idx][PAWN_INDEX];

  while (pawn_piece_bitboard) {
    constexpr uint64_t mask = (1ULL << BOARD_SIZE) - 1;
    int8_t idx = pawn_piece_bitboard.popRightmostBitIndex();
    int8_t shift = idx / BOARD_SIZE * BOARD_SIZE;

    pawn_exist_bitboard |= BitBoard(mask, 0) << shift;
  }

  // Build the bitboard of squares where a pawn can be dropped
  BitBoard hand_pawn_bitboard =
      BITBOARD_PAWN_DROPABLES[my_color_idx] & ~pawn_exist_bitboard & destinationBitBoard;

  // Pre-calculate the square that would give check (and potentially trigger drop-pawn checkmate)
  int8_t op_king_idx = _kingPositions[op_color_idx].getIndex();
  int8_t pawn_mate_idx =
      (op_king_idx >= 0)
          ? BITBOARD_PAWN_ATTACKS[op_color_idx][op_king_idx].getRightmostBitIndex()
          : -1;

  // Generate legal moves for each destination (pawn drop)
  while (hand_pawn_bitboard) {
    int8_t dst_idx = hand_pawn_bitboard.popRightmostBitIndex();

    // Skip if it would result in drop-pawn checkmate
    if (dst_idx == pawn_mate_idx && _isDropPawnCheckmateMove(dst_idx)) {
      continue;
    }

    // If restricting to check-only moves, register only moves that give check
    if constexpr (checkOnly) {
      if (dst_idx == pawn_mate_idx) {
        legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_PAWN), Position(dst_idx), false);
      }
    } else {
      legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_PAWN), Position(dst_idx), false);
    }
  }
}

/**
 * Generates legal drop moves for lances.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal lance drop moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for lance drops
 */
template <bool checkOnly>
void Board::_getLegalHandLanceMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If no lance is in the hand, there are no legal moves
  if (_hands[my_color_idx][LANCE_INDEX] == 0) {
    return;
  }

  // Build the bitboard of squares where a lance can be dropped
  // The droppable squares for a lance are the same as for a pawn (ignoring nifu),
  // so we reuse the pawn bitboard
  BitBoard hand_lance_bitboard = BITBOARD_PAWN_DROPABLES[my_color_idx] & destinationBitBoard;

  // Generate legal moves for each destination (lance drop)
  while (hand_lance_bitboard) {
    int8_t dst_idx = hand_lance_bitboard.popRightmostBitIndex();

    // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
    if constexpr (checkOnly) {
      if (_isDropCheckMove<PIECE_BLACK_LANCE>(dst_idx)) {
        legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_LANCE), Position(dst_idx), false);
      }
    } else {
      legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_LANCE), Position(dst_idx), false);
    }
  }
}

/**
 * Generates legal drop moves for knights.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal knight drop moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for knight drops
 */
template <bool checkOnly>
void Board::_getLegalHandKnightMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If no knight is in the hand, there are no legal moves
  if (_hands[my_color_idx][KNIGHT_INDEX] == 0) {
    return;
  }

  // Build the bitboard of squares where a knight can be dropped
  BitBoard hand_knight_bitboard = BITBOARD_KNIGHT_DROPABLES[my_color_idx] & destinationBitBoard;

  // Generate legal moves for each destination (knight drop)
  while (hand_knight_bitboard) {
    int8_t dst_idx = hand_knight_bitboard.popRightmostBitIndex();

    // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
    if constexpr (checkOnly) {
      if (_isDropCheckMove<PIECE_BLACK_KNIGHT>(dst_idx)) {
        legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_KNIGHT), Position(dst_idx), false);
      }
    } else {
      legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_KNIGHT), Position(dst_idx), false);
    }
  }
}

/**
 * Generates legal drop moves for silvers.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal silver drop moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for silver drops
 */
template <bool checkOnly>
void Board::_getLegalHandSilverMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If no silver is in the hand, there are no legal moves
  if (_hands[my_color_idx][SILVER_INDEX] == 0) {
    return;
  }

  // Build the bitboard of squares where a silver can be dropped
  BitBoard hand_silver_bitboard = destinationBitBoard;

  // Generate legal moves for each destination (silver drop)
  while (hand_silver_bitboard) {
    int8_t dst_idx = hand_silver_bitboard.popRightmostBitIndex();

    // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
    if constexpr (checkOnly) {
      if (_isDropCheckMove<PIECE_BLACK_SILVER>(dst_idx)) {
        legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_SILVER), Position(dst_idx), false);
      }
    } else {
      legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_SILVER), Position(dst_idx), false);
    }
  }
}

/**
 * Generates legal drop moves for golds.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal gold drop moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for gold drops
 */
template <bool checkOnly>
void Board::_getLegalHandGoldMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If no gold is in the hand, there are no legal moves
  if (_hands[my_color_idx][GOLD_INDEX] == 0) {
    return;
  }

  // Build the bitboard of squares where a gold can be dropped
  BitBoard hand_gold_bitboard = destinationBitBoard;

  // Generate legal moves for each destination (gold drop)
  while (hand_gold_bitboard) {
    int8_t dst_idx = hand_gold_bitboard.popRightmostBitIndex();

    // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
    if constexpr (checkOnly) {
      if (_isDropCheckMove<PIECE_BLACK_GOLD>(dst_idx)) {
        legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_GOLD), Position(dst_idx), false);
      }
    } else {
      legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_GOLD), Position(dst_idx), false);
    }
  }
}

/**
 * Generates legal drop moves for bishops.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal bishop drop moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for bishop drops
 */
template <bool checkOnly>
void Board::_getLegalHandBishopMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If no bishop is in the hand, there are no legal moves
  if (_hands[my_color_idx][BISHOP_INDEX] == 0) {
    return;
  }

  // Build the bitboard of squares where a bishop can be dropped
  BitBoard hand_bishop_bitboard = destinationBitBoard;

  // Generate legal moves for each destination (bishop drop)
  while (hand_bishop_bitboard) {
    int8_t dst_idx = hand_bishop_bitboard.popRightmostBitIndex();

    // If restricting to check-only moves, register only moves that give check
    if constexpr (checkOnly) {
      if (_isDropCheckMove<PIECE_BLACK_BISHOP>(dst_idx)) {
        legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_BISHOP), Position(dst_idx), false);
      }
    } else {
      legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_BISHOP), Position(dst_idx), false);
    }
  }
}

/**
 * Generates legal drop moves for rooks.
 * If the template argument checkOnly is true, returns only moves that cause check.
 * @param legalMoves Array object to add the legal rook drop moves to
 * @param destinationBitBoard Bitboard representing valid destination squares for rook drops
 */
template <bool checkOnly>
void Board::_getLegalHandRookMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If no rook is in the hand, there are no legal moves
  if (_hands[my_color_idx][ROOK_INDEX] == 0) {
    return;
  }

  // Build the bitboard of squares where a rook can be dropped
  BitBoard hand_rook_bitboard = destinationBitBoard;

  // Generate legal moves for each destination (rook drop)
  while (hand_rook_bitboard) {
    int8_t dst_idx = hand_rook_bitboard.popRightmostBitIndex();

    // If restricting to check-only moves, register only moves that give check; otherwise register unconditionally
    if constexpr (checkOnly) {
      if (_isDropCheckMove<PIECE_BLACK_ROOK>(dst_idx)) {
        legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_ROOK), Position(dst_idx), false);
      }
    } else {
      legalMoves.emplace_back(Position(BOARD_SIZE, PIECE_HAND_ROOK), Position(dst_idx), false);
    }
  }
}

/**
 * Returns whether moving from the specified source to destination results in check.
 * Specify the piece type using the template argument piece (use PIECE_BLACK_XXX).
 * @param srcIndex Integer value representing the source position
 * @param dstIndex Integer value representing the destination position
 * @return true if the move from the specified source to destination results in check
 */
template <uint8_t piece>
bool Board::_isCheckMove(int8_t srcIndex, int8_t dstIndex) const {
  return _isDropCheckMove<piece>(dstIndex) ||
         _isDiscoveredCheckMove(srcIndex, dstIndex, OPPOSITE_COLOR(_color));
}

/**
 * Returns whether moving from the specified source to destination causes a discovered check.
 * @param srcIndex Integer value representing the source position
 * @param dstIndex Integer value representing the destination position
 * @param color Player's turn for the king being checked against
 * @return true if the move causes a discovered check
 */
bool Board::_isDiscoveredCheckMove(int8_t srcIndex, int8_t dstIndex, int8_t color) const {
  int8_t my_color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

  // If the king is not on the board, discovered check is not possible
  if (king_pos_idx < 0) {
    return false;
  }

  // Check the positional relationship with the king
  int8_t direction = DIRECTION_INDICES[srcIndex][king_pos_idx];

  // If the king is not on an extension line in any of the 8 directions, discovered check is not possible
  if (direction == 8) {
    return false;
  }

  // If the move direction is the same as or opposite to the direction toward the king, discovered check is not possible
  int8_t move_direction = DIRECTION_INDICES[srcIndex][dstIndex];

  if (move_direction == direction || move_direction == (direction + 4) % 8) {
    return false;
  }

  // If there is a piece between the moving piece and the king, discovered check is not possible
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
  BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][srcIndex] & occ_bitboard;
  int8_t between_idx = (direction < 4)
                           ? between_bitboard.getLeftmostBitIndex()
                           : between_bitboard.getRightmostBitIndex();

  if (between_idx != king_pos_idx) {
    return false;
  }

  // Check the piece on the opposite side
  BitBoard opposite_bitboard =
      BITBOARD_LONG_ATTACKS[(direction + 4) % 8][srcIndex] & occ_bitboard;
  int8_t opposite_idx = (direction < 4)
                            ? opposite_bitboard.getRightmostBitIndex()
                            : opposite_bitboard.getLeftmostBitIndex();

  // If there is no piece, discovered check is not possible
  if (opposite_idx < 0) {
    return false;
  }
  // If the direction is diagonal (odd) and the piece there has bishop attacks, it is a discovered check
  else if ((direction % 2 == 1) &&
           _pieceBitBoards[op_color_idx][BISHOP_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // If the direction is orthogonal (even) and the piece there has rook attacks, it is a discovered check
  else if ((direction % 2 == 0) &&
           _pieceBitBoards[op_color_idx][ROOK_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // If black moves downward and the piece there has lance attacks, it is a discovered check
  else if (color == COLOR_BLACK && direction == 4 &&
           _pieceBitBoards[op_color_idx][LANCE_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // If white moves upward and the piece there has lance attacks, it is a discovered check
  else if (color == COLOR_WHITE && direction == 0 &&
           _pieceBitBoards[op_color_idx][LANCE_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // Otherwise, discovered check is not possible
  else {
    return false;
  }
}

/**
 * Returns whether dropping a piece at the specified position results in check.
 * Specify the piece type using the template argument piece (use PIECE_BLACK_XXX).
 * @param dstIndex Integer value representing the destination position
 * @return true if dropping a piece at the specified position results in check
 */
template <uint8_t piece>
bool Board::_isDropCheckMove(int8_t dstIndex) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t op_king_idx = _kingPositions[op_color_idx].getIndex();

  // If the king is not on the board, check is not possible
  if (op_king_idx < 0) {
    return false;
  }

  // For pawn (uchifuzume is checked separately, so it is not considered here)
  if constexpr (piece == PIECE_BLACK_PAWN) {
    return BITBOARD_PAWN_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // For lance
  if constexpr (piece == PIECE_BLACK_LANCE) {
    // If black is not attacking upward or white is not attacking downward, check is not possible
    int8_t direction = DIRECTION_INDICES[dstIndex][op_king_idx];

    if ((_color == COLOR_BLACK && direction != 0) ||
        (_color == COLOR_WHITE && direction != 4)) {
      return false;
    }

    // If there is no piece between the drop square and the opponent's king, it is a check
    BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
    BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][dstIndex] & occ_bitboard;
    int8_t between_idx = (direction < 4)
                             ? between_bitboard.getLeftmostBitIndex()
                             : between_bitboard.getRightmostBitIndex();

    return between_idx == op_king_idx;
  }

  // For knight
  if constexpr (piece == PIECE_BLACK_KNIGHT) {
    return BITBOARD_KNIGHT_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // For silver
  if constexpr (piece == PIECE_BLACK_SILVER) {
    return BITBOARD_SILVER_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // For gold
  if constexpr (piece == PIECE_BLACK_GOLD) {
    return BITBOARD_GOLD_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // For horse and dragon (sliding piece attacks are checked later)
  if constexpr (piece == PIECE_BLACK_HORSE || piece == PIECE_BLACK_DRAGON) {
    if (BITBOARD_KING_ATTACKS[dstIndex].hasBit(op_king_idx)) {
      return true;
    }
  }

  // For bishop (horse's one-square move is already checked; check diagonal directions to conclude)
  if constexpr (piece == PIECE_BLACK_BISHOP || piece == PIECE_BLACK_HORSE) {
    // If the direction to the opponent's king is not diagonal, check is not possible
    int8_t direction = DIRECTION_INDICES[dstIndex][op_king_idx];

    if (direction % 2 == 0) {
      return false;
    }

    // If there is no piece between the drop square and the opponent's king, it is a check (bishop)
    BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
    BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][dstIndex] & occ_bitboard;
    int8_t between_idx = (direction < 4)
                             ? between_bitboard.getLeftmostBitIndex()
                             : between_bitboard.getRightmostBitIndex();

    return between_idx == op_king_idx;
  }

  // For rook (dragon's one-square move is already checked; check orthogonal directions to conclude)
  if constexpr (piece == PIECE_BLACK_ROOK || piece == PIECE_BLACK_DRAGON) {
    // If the direction to the opponent's king is not orthogonal, check is not possible
    int8_t direction = DIRECTION_INDICES[dstIndex][op_king_idx];

    if (direction == 8 || direction % 2 == 1) {
      return false;
    }

    // If there is no piece between the drop square and the opponent's king, it is a check (rook)
    BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
    BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][dstIndex] & occ_bitboard;
    int8_t between_idx = (direction < 4)
                             ? between_bitboard.getLeftmostBitIndex()
                             : between_bitboard.getRightmostBitIndex();

    return between_idx == op_king_idx;
  }

  return false;
}

/**
 * Returns whether dropping a pawn at the specified position results in uchifuzume (drop-pawn checkmate).
 * @param dstIndex Integer value representing the destination position
 * @return true if dropping a pawn at the specified position results in uchifuzume
 */
bool Board::_isDropPawnCheckmateMove(int8_t dstIndex) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t op_king_idx = _kingPositions[op_color_idx].getIndex();

  // If the king is not on the board, uchifuzume is not possible
  if (op_king_idx < 0) {
    return false;
  }

  // If the specified coordinate is not directly in front of the king, uchifuzume is not possible
  int8_t pawn_mate_idx =
      BITBOARD_PAWN_ATTACKS[op_color_idx][op_king_idx].getRightmostBitIndex();

  if (dstIndex != pawn_mate_idx) {
    return false;
  }

  // If none of the king's escape squares is attacked, uchifuzume is not possible
  // Note: account for the possibility that the dropped pawn may block a sliding piece attack
  BitBoard king_move_bitboard =
      BITBOARD_KING_ATTACKS[op_king_idx] & ~_colorBitBoards[op_color_idx];

  while (king_move_bitboard) {
    int8_t dst_idx = king_move_bitboard.popRightmostBitIndex();

    if (_getAttackers<true, true>(OPPOSITE_COLOR(_color), dst_idx, pawn_mate_idx).empty()) {
      return false;
    }
  }

  // If any piece other than the king can move to the drop square, uchifuzume is not possible
  for (int8_t attacker_pos : _getAttackers<false, false>(_color, pawn_mate_idx)) {
    if (attacker_pos != op_king_idx &&
        !_isDiscoveredCheckMove(attacker_pos, pawn_mate_idx, OPPOSITE_COLOR(_color))) {
      return false;
    }
  }

  // If none of the above conditions are satisfied, it is uchifuzume
  return true;
}

/**
 * Retrieves the board data to input to the model.
 * @param inputs Board data array to input to the model
 * @param color Player's color (COLOR_BLACK or COLOR_WHITE)
 */
void Board::_getBoardInputs(int32_t* inputs, int8_t color) const {
  constexpr int32_t black_offset = 1;
  constexpr int32_t white_offset = black_offset + 14 + 14 + 6;
  constexpr int32_t other_offset = white_offset + 14 + 14 + 6;
  constexpr int32_t board_square = BOARD_SIZE * BOARD_SIZE;

  for (int32_t src = 0; src < board_square; src++) {
    // Calculate the index of the location to set the value
    // If it is white's turn, rotate the board 180 degrees
    int32_t dst = (color == COLOR_BLACK) ? src : (board_square - 1 - src);

    // Set the information for empty squares
    if (_cells[src] == PIECE_EMPTY) {
      setInputBit(inputs, 0 * board_square + dst);
    }

    // Set the values for piece placement
    uint8_t piece = _cells[src];

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

    // Set the values for piece attack coverage
    int32_t black_att_count = 0;
    int32_t white_att_count = 0;

    for (Position pos : getAttackers(Position(src))) {
      uint8_t att_piece = _cells[pos.getIndex()];

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

    // Set the count of attacking pieces
    black_att_count = std::min(black_att_count, 5);
    white_att_count = std::min(white_att_count, 5);

    setInputBit(inputs, (black_offset + 14 + 14 + black_att_count) * board_square + dst);
    setInputBit(inputs, (white_offset + 14 + 14 + white_att_count) * board_square + dst);

    // Set the coordinate of the last moved piece
    if (_lastMove != MOVE_INVALID && _lastMove.getDst().getIndex() == src) {
      setInputBit(inputs, other_offset * board_square + dst);
    }

    // Set the row and column indices
    constexpr int32_t row_offset = other_offset + 1;
    constexpr int32_t col_offset = row_offset + BOARD_SIZE;
    int32_t x = src / BOARD_SIZE;
    int32_t y = src % BOARD_SIZE;

    if (color == COLOR_BLACK) {
      setInputBit(inputs, (row_offset + y) * board_square + dst);
      setInputBit(inputs, (col_offset + std::min(x, BOARD_SIZE - 1 - x)) * board_square + dst);
    } else {
      setInputBit(inputs, (row_offset + BOARD_SIZE - 1 - y) * board_square + dst);
      setInputBit(inputs, (col_offset + std::min(x, BOARD_SIZE - 1 - x)) * board_square + dst);
    }
  }
}

/**
 * Retrieves the game data to input to the model.
 * @param inputs Game data array to input to the model
 * @param color Player's color (COLOR_BLACK or COLOR_WHITE)
 */
void Board::_getInfoInputs(int32_t* inputs, int8_t color) const {
  constexpr int32_t info_offset = MODEL_FEATURES * BOARD_SIZE * BOARD_SIZE;
  constexpr int32_t hand_offsets[] = {0, 18, 22, 26, 30, 32, 34};
  constexpr int32_t hand_length = 38;

  // Set the held piece information
  // Use the bit representation of held pieces directly
  for (int side = 0; side < 2; side++) {
    int32_t offset = info_offset + (side * hand_length);
    int32_t input_index = offset / 32;
    int32_t bit_index = offset % 32;

    int8_t color_idx = (color == COLOR_BLACK) ? side : (1 - side);
    uint64_t hand_bits = _handBits[color_idx];
    int8_t bit_shift = 0;

    while (bit_shift < hand_length) {
      inputs[input_index] |= (int32_t)((hand_bits >> bit_shift) << bit_index) & 0xffffffff;
      bit_shift += 32 - bit_index;
      input_index += 1;
      bit_index = 0;
    }
  }

  // Set the check status information
  if (isCheck(_color)) {
    setInputBit(inputs, info_offset + hand_length * 2);
  }

  // Set the score required for entering-king declaration
  if (color == COLOR_BLACK) {
    inputs[MODEL_INPUT_PACK_SIZE - 3] = (int)((_nyugyokuScores[0] - 27.5) / 5.0 * 0xfffff);
    inputs[MODEL_INPUT_PACK_SIZE - 2] = (int)((_nyugyokuScores[1] - 27.5) / 5.0 * 0xfffff);
  } else {
    inputs[MODEL_INPUT_PACK_SIZE - 3] = (int)((_nyugyokuScores[1] - 27.5) / 5.0 * 0xfffff);
    inputs[MODEL_INPUT_PACK_SIZE - 2] = (int)((_nyugyokuScores[0] - 27.5) / 5.0 * 0xfffff);
  }

  // Set the remaining number of moves until a draw
  float remaining_turn = 1.0f - (_drawTurn - _turn) / 50.0f;

  remaining_turn = std::min(std::max(remaining_turn, 0.0f), 1.0f);
  inputs[MODEL_INPUT_PACK_SIZE - 1] = (int)(remaining_turn * 0xfffff);
}

}  // namespace deepshogi

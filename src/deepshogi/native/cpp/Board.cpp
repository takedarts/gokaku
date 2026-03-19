#include "Board.h"

#include <algorithm>
#include <map>
#include <sstream>

#include "Constant.h"

namespace deepshogi {

// Mapping table between piece numbers and characters in SFEN format
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

// Mapping table between SFEN format piece characters and their numbers
// For promoted pieces, convert to the piece number before promotion (promotion processing follows after)
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

// Array of SFEN format hand piece names
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

// Mapping table between SFEN format hand piece characters and their {color, piece number}
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

// Array of hand piece types in SFEN format
// Used to determine the display order in SFEN format
static const uint8_t SFEN_HAND_PIECES[] = {
    PIECE_HAND_ROOK, PIECE_HAND_BISHOP, PIECE_HAND_GOLD,
    PIECE_HAND_SILVER, PIECE_HAND_KNIGHT, PIECE_HAND_LANCE, PIECE_HAND_PAWN};

// Bitboard index corresponding to piece type
static constexpr int8_t PAWN_INDEX = PIECE_BLACK_PAWN - PIECE_BLACK_BEGIN;
static constexpr int8_t LANCE_INDEX = PIECE_BLACK_LANCE - PIECE_BLACK_BEGIN;
static constexpr int8_t KNIGHT_INDEX = PIECE_BLACK_KNIGHT - PIECE_BLACK_BEGIN;
static constexpr int8_t SILVER_INDEX = PIECE_BLACK_SILVER - PIECE_BLACK_BEGIN;
static constexpr int8_t GOLD_INDEX = PIECE_BLACK_GOLD - PIECE_BLACK_BEGIN;
static constexpr int8_t KING_INDEX = PIECE_BLACK_KING - PIECE_BLACK_BEGIN;
static constexpr int8_t BISHOP_INDEX = PIECE_BLACK_BISHOP - PIECE_BLACK_BEGIN;
static constexpr int8_t ROOK_INDEX = PIECE_BLACK_ROOK - PIECE_BLACK_BEGIN;

/**
 * Sets the specified bit.
 * @param inputs Bit sequence
 * @param index Position of the bit to set
 */
inline void setInputBit(int32_t* inputs, int32_t index, int32_t value = 1) {
  inputs[index / 32] |= (value << (index % 32));
}

/**
 * Searches whether there is a checkmate move from the specified board.
 * Returns an array with the checkmate move sequence stored in reverse order.
 * Returns an empty array if no checkmate move is found.
 * Note that the contents of the Board object change during the search.
 * @param board Board object
 * @param depth Search depth
 * @return List of checkmate moves
 */
static std::vector<Move> searchCheckmateMoves(Board& board, int32_t depth) {
  // If depth is 0 or less, no checkmate move was found, so return an empty array
  if (depth <= 0) {
    return {};
  }

  // If the draw turn count is exceeded, return an empty array
  if (board.getTurn() >= board.getDrawTurn()) {
    return {};
  }

  // Try moves that give check
  std::vector<Move> shortest_moves;

  for (const Move& move : board.getLegalMoves(false, true)) {
    // Advance the board and save the result
    Result checkmate_result = board.play(move);

    // Check if there are any moves to escape the check
    std::vector<Move> escape_moves = board.getLegalMoves(false, false);

    // If there are no escape moves, checkmate is found, so return the checkmate move
    if (escape_moves.empty()) {
      board.undo(checkmate_result);
      return {move};
    }

    // If search depth is 1 or less, do not try escape moves
    if (depth - 1 <= 0) {
      board.undo(checkmate_result);
      continue;
    }

    // Try all escape moves and find the deepest checkmate sequence
    bool checkmated = true;
    std::vector<Move> longest_moves;

    for (Move& escape_move : escape_moves) {
      // Create the next board state
      Result escape_result = board.play(escape_move);

      // Recursively search for checkmate moves
      std::vector<Move> moves = searchCheckmateMoves(board, depth - 2);

      // If no checkmate move is found, determine it as not checkmate and exit loop
      if (moves.empty()) {
        board.undo(escape_result);
        checkmated = false;
        break;
      }

      // Save the deepest checkmate sequence
      if (longest_moves.empty() || moves.size() > longest_moves.size() - 1) {
        longest_moves = moves;
        longest_moves.push_back(escape_move);
      }

      // Restore the board to its previous state
      board.undo(escape_result);
    }

    // If all escape moves lead to checkmate, a checkmate sequence has been found
    // Save the shallowest checkmate sequence
    if (checkmated) {
      if (shortest_moves.empty() || longest_moves.size() < shortest_moves.size() - 1) {
        shortest_moves = longest_moves;
        shortest_moves.push_back(move);
      }
    }

    // Restore the board to its previous state
    board.undo(checkmate_result);
  }

  // Return the shallowest checkmate sequence
  return shortest_moves;
}

/**
 * Creates a board object.
 * Does not place pieces on the board.
 */
Board::Board()
    : _cells{0},
      _hands{{0}},
      _kingPositions{POSITION_INVALID, POSITION_INVALID},
      _nyugyokuScores{28, 27},
      _color(COLOR_BLACK),
      _turn(0),
      _drawTurn(0x7fff),
      _hash(0),
      _lastMove(MOVE_INVALID) {
}

/**
 * Creates a board object.
 * Does not place pieces on the board.
 * @param nyugyokuScoreBlack The score required for black to declare nyugyoku
 * @param nyugyokuScoreWhite The score required for white to declare nyugyoku
 * @param drawTurn The number of moves until a draw
 */
Board::Board(int8_t nyugyokuScoreBlack, int8_t nyugyokuScoreWhite, int16_t drawTurn)
    : Board() {
  _nyugyokuScores[0] = nyugyokuScoreBlack;
  _nyugyokuScores[1] = nyugyokuScoreWhite;
  _drawTurn = drawTurn;
}

/**
 * Initializes the board with a string in SFEN format.
 * @param sfen A string in SFEN format
 */
void Board::initialize(const std::string& sfen) {
  // Initialize the board
  std::fill(std::begin(_cells), std::end(_cells), 0);
  std::fill(_hands[0], _hands[0] + (sizeof(_hands[0]) / sizeof(_hands[0][0])), 0);
  std::fill(_hands[1], _hands[1] + (sizeof(_hands[1]) / sizeof(_hands[1][0])), 0);

  _kingPositions[0] = POSITION_INVALID;
  _kingPositions[1] = POSITION_INVALID;
  _hash = 0;

  _colorBitBoards[0].clearAll();
  _colorBitBoards[1].clearAll();

  for (int32_t p = 0; p < PIECE_BLACK_PRO_PAWN - PIECE_BLACK_BEGIN; p++) {
    _pieceBitBoards[0][p].clearAll();
    _pieceBitBoards[1][p].clearAll();
  }

  // Determine the split positions of the SFEN string
  char* c_sfen = const_cast<char*>(sfen.c_str());
  int32_t board_sfen_length = 0;
  int32_t hand_sfen_length = 0;

  for (int32_t i = 0; c_sfen[i] != ' '; i++) {
    board_sfen_length++;
  }

  for (int32_t i = board_sfen_length + 3; c_sfen[i] != ' '; i++) {
    hand_sfen_length++;
  }

  // Reflect the SFEN information on the board
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

    // If it's a number, move to the next position by the number of empty squares
    if ('1' <= c && c <= '9') {
      pos_x -= c - '0';
      continue;
    }

    // If it's a piece symbol, set the piece
    Position pos(pos_x, pos_y);
    uint8_t piece = SFEN_PIECE_TYPES.at(c);

    // Handle promotion
    if (promote) {
      piece += PIECE_PROMOTE;
    }

    _putPiece(pos, piece);

    // Move to the next position
    promote = false;
    pos_x -= 1;
  }

  // Reflect the SFEN information of the hands
  int32_t hand_piece_num = 1;

  for (int32_t i = 0; i < hand_sfen_length; i++) {
    char c = c_sfen[board_sfen_length + 3 + i];

    // If it's a number, set the number of pieces
    if ('1' <= c && c <= '9') {
      hand_piece_num = c - '0';
      continue;
    }

    // If it's a piece symbol, set the hand pieces
    if (SFEN_HAND_PIECE_TYPES.count(c) > 0) {
      auto [color, piece] = SFEN_HAND_PIECE_TYPES.at(c);

      _addHand(color, piece, hand_piece_num);
    }

    // If it's not a number, reset the number of pieces to 1
    hand_piece_num = 1;
  }

  // Set the turn
  if (c_sfen[board_sfen_length + 1] == 'b') {
    _color = COLOR_BLACK;
  } else if (c_sfen[board_sfen_length + 1] == 'w') {
    _color = COLOR_WHITE;
  }

  // Set the turn number
  _turn = static_cast<int16_t>(
      std::stoi(sfen.substr(board_sfen_length + 3 + hand_sfen_length + 1)) - 1);
}

/**
 * Moves a piece.
 * @param move The move to play
 * @return The result of the move
 */
Result Board::play(const Move& move) {
  Position src = move.getSrc();
  Position dst = move.getDst();
  uint8_t captured_piece = PIECE_EMPTY;

  // If the move is from a hand piece
  if (src.getX() == BOARD_SIZE) {
    // Decrease the hand piece
    uint8_t hand_piece = src.getY();

    _removeHand(_color, hand_piece);

    // Place the piece on the board
    uint8_t dst_piece =
        (_color == COLOR_BLACK)
            ? (hand_piece - PIECE_HAND_BEGIN + PIECE_BLACK_BEGIN)
            : (hand_piece - PIECE_HAND_BEGIN + PIECE_WHITE_BEGIN);

    _putPiece(dst, dst_piece);
  }
  // If the move is from a piece on the board
  else {
    // If there is a piece at the destination, capture it
    captured_piece = _cells[dst.getIndex()];

    if (captured_piece != PIECE_EMPTY) {
      int32_t hand_piece =
          (captured_piece < PIECE_WHITE_BEGIN)
              ? (captured_piece - PIECE_BLACK_BEGIN + PIECE_HAND_BEGIN)
              : (captured_piece - PIECE_WHITE_BEGIN + PIECE_HAND_BEGIN);

      // If the captured piece is promoted, convert it to the unpromoted piece type
      if (hand_piece >= PIECE_HAND_END) {
        hand_piece -= PIECE_PROMOTE;
      }

      // Remove the piece from the destination
      _removePiece(dst);

      // Add the captured piece to the hand
      _addHand(_color, hand_piece);
    }

    // Get the piece from the source
    uint8_t src_piece = _cells[src.getIndex()];

    // Handle promotion
    if (move.isPromote()) {
      src_piece += PIECE_PROMOTE;
    }

    // Move the piece
    _removePiece(src);
    _putPiece(dst, src_piece);
  }

  // Change the turn
  _color = (_color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;

  // Increase the turn number
  _turn += 1;

  // Save the move
  _lastMove = move;

  // Return the result of the move
  return Result(move, captured_piece);
}

/**
 * Undoes the specified move on the board.
 * This function assumes that the specified move is the most recent move.
 * @param result The result of the move to undo
 */
void Board::undo(const Result& result) {
  Move move = result.getMove();
  Position src = move.getSrc();
  Position dst = move.getDst();
  uint8_t captured_piece = result.getCaptured();

  // Change the turn
  _color = (_color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;

  // Decrease the turn number
  _turn -= 1;

  // Reset the last move
  _lastMove = MOVE_INVALID;

  // Restore the piece to its original position
  if (src.getX() == BOARD_SIZE) {
    // If the move was from a hand piece, remove the piece and increase the hand piece
    _removePiece(dst);
    _addHand(_color, src.getY());
  } else {
    // If the move was from a piece on the board, move the piece and restore any captured piece
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
 * Gets the positions of pieces that attack the specified position.
 * @param position The position to check
 * @return A list of positions of pieces that attack the specified position
 */
std::vector<Position> Board::getAttackers(const Position& position) const {
  // Reserve size in advance since at most 10 pieces can attack a position
  std::vector<Position> attackers;
  attackers.reserve(10);

  // Get the positions of pieces that attack the specified position
  for (int8_t color : {COLOR_BLACK, COLOR_WHITE}) {
    for (int8_t attacker : _getAttackers<false, false>(color, position.getIndex())) {
      attackers.emplace_back(attacker);
    }
  }

  return attackers;
}

/**
 * Gets the list of legal moves for the current board.
 * @param removeUnpromote If true, removes unpromoted moves for pawns, bishops, rooks, and lances on the second row
 * @param checkOnly If true, only gets moves that result in check
 * @return A list of legal moves
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
 * Gets the sequence of moves that lead to checkmate for the current board.
 * @param depth The depth of the checkmate search
 * @return A sequence of moves that lead to checkmate
 */
std::vector<Move> Board::getCheckmateMoves(int32_t depth) const {
  // Create a clone of the board for calculation
  // During the search, the contents of the Board object change,
  // so a separate board is prepared for the search
  Board clone_board(*this);

  // Execute the checkmate search
  std::vector<Move> moves = searchCheckmateMoves(clone_board, depth);

  // Return the sequence of moves that lead to checkmate
  // (stored in reverse order, so reverse the order)
  std::reverse(moves.begin(), moves.end());

  return moves;
}

/**
 * Returns true if the board is in a state where nyugyoku declaration is possible.
 * @param color The color of the player making the declaration
 * @return True if nyugyoku declaration is possible
 */
bool Board::isNyugyoku(int8_t color) const {
  int8_t my_color_idx = (color == COLOR_BLACK) ? 0 : 1;

  // [Condition 1] It is the declaring player's turn
  // [Condition 6] The declaring player has remaining time

  // [Condition 5] The declaring player's king is not in check
  if (isCheck(color)) {
    return false;
  }

  // [Condition 2] The declaring player's king has entered the opponent's territory (inner 3 rows)
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

  if (king_pos_idx < 0 ||
      !BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(king_pos_idx)) {
    return false;
  }

  // [Condition 4] The declaring player has 10 or more pieces
  // in the opponent's territory (inner 3 rows), excluding the king
  // Since the king is in the enemy territory, a total of 11 or more pieces must exist
  BitBoard invasion_bitboard =
      _colorBitBoards[my_color_idx] & BITBOARD_ENEMY_AREAS[my_color_idx];

  if (invasion_bitboard.countBit() < 11) {
    return false;
  }

  // [Condition 3] The declaring player's score is calculated as:
  // major pieces (rook/bishop) = 5 points, minor pieces = 1 point
  // Points are counted only from the declaring player's captured pieces
  // and pieces in the enemy territory (excluding the king)
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

  // Check if the required score for nyugyoku declaration is met
  return nyugyoku_score >= _nyugyokuScores[my_color_idx];
}

/**
 * Returns true if the board is in a state where the king is in check.
 * @param color The color of the player whose king is in check
 * @return True if the king is in check
 */
bool Board::isCheck(int8_t color) const {
  int8_t color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int8_t king_pos_idx = _kingPositions[color_idx].getIndex();

  // If the king is not on the board, it cannot be in check
  if (king_pos_idx < 0) {
    return false;
  }
  // If the king is on the board, check if it is in check
  else {
    return !_getAttackers<true, false>(color, king_pos_idx).empty();
  }
}

/**
 * Returns the SFEN string representation of the board.
 * @return The SFEN string representation of the board
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

  // Set the turn
  if (_color == COLOR_BLACK) {
    ss << " b ";
  } else {
    ss << " w ";
  }

  // Set the hand piece information
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

  // Set the turn number
  ss << " " << (_turn + 1);

  return ss.str();
}

/**
 * Retrieves the data to be input into the model.
 * @param inputs The data to be input into the model
 */
void Board::getInputs(int32_t* inputs) const {
  getInputs(inputs, _color);
}

/**
 * Retrieves the data to be input into the model.
 * @param inputs The data to be input into the model
 * @param color The color of the player whose turn it is
 */
void Board::getInputs(int32_t* inputs, int8_t color) const {
  // Initialize the input array
  std::fill_n(inputs, MODEL_INPUT_PACK_SIZE, 0);

  // Retrieve the data to be input into the model
  _getBoardInputs(inputs, color);
  _getInfoInputs(inputs, color);
}

/**
 * Copies the state of the board.
 * @param board The board to copy from
 */
void Board::copyFrom(const Board* board) {
  *this = *board;
}

/**
 * Returns a string representation of the board.
 * @return A string representation of the board
 */
std::string Board::toString() const {
  // Array of piece information for creating a string in CSA format
  const char piece_names[][3] = {
      "FU", "KY", "KE", "GI", "KA", "HI", "KI", "OU",
      "TO", "NY", "NK", "NG", "UM", "RY"};
  const char hand_color_names[][3] = {"P+", "P-"};

  // Create a string stream object to build the string
  std::stringstream ss;

  // Create the turn and turn number
  ss << "Color: " << ((_color == COLOR_WHITE) ? "white" : "black");
  ss << ", Turn: " << _turn << std::endl;

  // Create the board piece information
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

  // Create the hand piece information
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
 * This method assumes that there is no piece at the specified coordinates.
 * @param pos The position to place the piece
 * @param piece The integer value representing the piece to place
 */
void Board::_putPiece(const Position& pos, uint8_t piece) {
  int8_t pos_idx = pos.getIndex();

  // Update the hash value
  _hash ^= BOARD_HASH_VALUES[pos_idx][piece];

  // Place the piece at the specified position
  _cells[pos_idx] = piece;

  // Update the bitboards based on the type of piece
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
 * Removes a piece from the specified position.
 * @param pos The position to remove the piece from
 */
void Board::_removePiece(const Position& pos) {
  int8_t pos_idx = pos.getIndex();
  uint8_t piece = _cells[pos_idx];

  // Update the hash value
  _hash ^= BOARD_HASH_VALUES[pos_idx][piece];

  // Remove the piece from the specified position
  _cells[pos_idx] = PIECE_EMPTY;

  // Update the bitboards based on the type of piece
  // Promoted pawn, promoted lance, promoted knight, and promoted silver use the same bitboard as gold
  // Horse uses the same bitboard as bishop and king
  // Dragon uses the same bitboard as rook and king
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
 * Adds a piece to the hand.
 * @param color The color of the player (COLOR_BLACK or COLOR_WHITE)
 * @param piece The integer value representing the piece to add
 * @param num The number of pieces to add
 */
void Board::_addHand(int8_t color, uint8_t piece, int32_t num) {
  int32_t color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int32_t piece_idx = piece - PIECE_HAND_BEGIN;

  // Update the hash value
  _hash ^= HAND_HASH_VALUES[color_idx][piece_idx][_hands[color_idx][piece_idx]];

  // Add the specified piece to the hand
  _hands[color_idx][piece_idx] += num;

  // Update the hash value
  _hash ^= HAND_HASH_VALUES[color_idx][piece_idx][_hands[color_idx][piece_idx]];
}

/**
 * Adds a piece to the hand.
 * @param color The color of the player (COLOR_BLACK or COLOR_WHITE)
 * @param piece The integer value representing the piece to add
 */
void Board::_addHand(int8_t color, uint8_t piece) {
  _addHand(color, piece, 1);
}

/**
 * Removes a piece from the hand.
 * This function assumes that the specified piece exists in the hand.
 * @param color The color of the player (COLOR_BLACK or COLOR_WHITE)
 * @param piece The integer value representing the piece to remove
 */
void Board::_removeHand(int8_t color, uint8_t piece) {
  int32_t color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int32_t piece_idx = piece - PIECE_HAND_BEGIN;

  // Update the hash value
  _hash ^= HAND_HASH_VALUES[color_idx][piece_idx][_hands[color_idx][piece_idx]];

  // Remove the specified piece from the hand
  _hands[color_idx][piece_idx] -= 1;

  // Update the hash value
  _hash ^= HAND_HASH_VALUES[color_idx][piece_idx][_hands[color_idx][piece_idx]];
}

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
std::vector<int8_t> Board::_getAttackers(
    int8_t color, int8_t posIndex, int8_t additionalOccIndex) const {
  // Calculate the array indices for the color being attacked and the color attacking
  int8_t my_color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;

  // Object to store the coordinates of attacking pieces
  // Reserve space for up to 10 coordinates in advance

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
  // Since king attacks can overlap with bishop and rook attacks,
  // create a mask to exclude the coordinates found here
  BitBoard king_bitboard =
      BITBOARD_KING_ATTACKS[posIndex] & _pieceBitBoards[op_color_idx][KING_INDEX];
  BitBoard horse_dragon_bitboard = king_bitboard;

  while (king_bitboard) {
    attacker_indices.push_back(king_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Create bitboards representing the squares attacked by lance, bishop, and rook
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
  BitBoard lance_attack_bitboard;
  BitBoard bishop_attack_bitboard;
  BitBoard rook_attack_bitboard;

  // If removeOwnKing is true, remove the position of the player's own king from the occupancy bitboard
  if constexpr (removeOwnKing) {
    int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

    if (king_pos_idx >= 0) {
      occ_bitboard.clearBit(king_pos_idx);
    }
  }

  // If additionalOccIndex is not -1, assume there is a piece at that position
  if (additionalOccIndex >= 0) {
    occ_bitboard.setBit(additionalOccIndex);
  }

  // Check upward attacks
  int8_t up_index =
      (BITBOARD_LONG_ATTACKS[0][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (up_index >= 0 && !horse_dragon_bitboard.hasBit(up_index)) {
    if (color == COLOR_BLACK) {
      lance_attack_bitboard.setBit(up_index);
    }

    rook_attack_bitboard.setBit(up_index);
  }

  // Check up-right attacks
  int8_t up_right_index =
      (BITBOARD_LONG_ATTACKS[1][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (up_right_index >= 0 && !horse_dragon_bitboard.hasBit(up_right_index)) {
    bishop_attack_bitboard.setBit(up_right_index);
  }

  // Check right attacks
  int8_t right_index =
      (BITBOARD_LONG_ATTACKS[2][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (right_index >= 0 && !horse_dragon_bitboard.hasBit(right_index)) {
    rook_attack_bitboard.setBit(right_index);
  }

  // Check down-right attacks
  int8_t down_right_index =
      (BITBOARD_LONG_ATTACKS[3][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (down_right_index >= 0 && !horse_dragon_bitboard.hasBit(down_right_index)) {
    bishop_attack_bitboard.setBit(down_right_index);
  }

  // Check downward attacks
  int8_t down_index =
      (BITBOARD_LONG_ATTACKS[4][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (down_index >= 0 && !horse_dragon_bitboard.hasBit(down_index)) {
    if (color == COLOR_WHITE) {
      lance_attack_bitboard.setBit(down_index);
    }

    rook_attack_bitboard.setBit(down_index);
  }

  // Check down-left attacks
  int8_t down_left_index =
      (BITBOARD_LONG_ATTACKS[5][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (down_left_index >= 0 && !horse_dragon_bitboard.hasBit(down_left_index)) {
    bishop_attack_bitboard.setBit(down_left_index);
  }

  // Check left attacks
  int8_t left_index =
      (BITBOARD_LONG_ATTACKS[6][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (left_index >= 0 && !horse_dragon_bitboard.hasBit(left_index)) {
    rook_attack_bitboard.setBit(left_index);
  }

  // Check up-left attacks
  int8_t up_left_index =
      (BITBOARD_LONG_ATTACKS[7][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (up_left_index >= 0 && !horse_dragon_bitboard.hasBit(up_left_index)) {
    bishop_attack_bitboard.setBit(up_left_index);
  }

  // Check lance attacks
  BitBoard lance_bitboard =
      lance_attack_bitboard & _pieceBitBoards[op_color_idx][LANCE_INDEX];

  if (lance_bitboard) {
    attacker_indices.push_back(lance_bitboard.getRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Check bishop attacks
  BitBoard bishop_bitboard =
      bishop_attack_bitboard & _pieceBitBoards[op_color_idx][BISHOP_INDEX];

  while (bishop_bitboard) {
    attacker_indices.push_back(bishop_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // Check rook attacks
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
 * Get a list of legal moves for the current board.
 * If the template parameter removeUnpromote is true,
 * moves that do not promote pawns, bishops, rooks, or lances on the second rank are removed.
 * If the template parameter checkOnly is true, only moves that result in check are retrieved.
 * @param legalMoves A vector to which the list of legal moves will be added.
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalMoves(std::vector<Move>& legalMoves) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;

  // Check the situation of check
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();
  std::vector<int8_t> checking_piece_indices;

  if (king_pos_idx >= 0) {
    checking_piece_indices = _getAttackers<false, false>(_color, king_pos_idx);
  }

  // If there are no pieces giving check, all legal moves are valid
  // If there is one piece giving check, the legal moves are capturing that piece,
  //  moving the king, or placing a piece between the king and the attacking piece
  // If there are two or more pieces giving check, only moving the king is a legal move
  BitBoard destination_bitboard;  // Initially initialized to 0

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

  // Create moves for moving pieces on the board
  // Since the one-square moves of horse and dragon can overlap with the king's moves and horse/dragon moves,
  // generate legal moves for the king, horse, and dragon first and remove duplicate moves
  _getLegalKingMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalBishopMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);
  _getLegalRookMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);

  // Remove duplicate moves
  std::sort(legalMoves.begin(), legalMoves.end());
  legalMoves.erase(std::unique(legalMoves.begin(), legalMoves.end()), legalMoves.end());

  // Generate legal moves for pawns, lances, knights, silvers, and golds
  _getLegalPawnMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);
  _getLegalLanceMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);
  _getLegalKnightMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalSilverMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalGoldMoves<checkOnly>(legalMoves, destination_bitboard);

  // Generate legal moves for dropping pawns, lances, knights, silvers, golds, bishops, and rooks
  // Since these are moves for dropping pieces from hand, moves that capture opponent's pieces are excluded
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
 * Create legal moves for pawns.
 * If the template parameter removeUnpromote is true, moves that do not promote pawns are removed.
 * If the template parameter checkOnly is true, only moves that give check are obtained.
 * @param legalMoves Array object to which the list of legal moves for pawns is added
 * @param destinationBitBoard Bitboard representing valid destination squares for pawn moves
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalPawnMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Create bitboard for pawns
  BitBoard pawn_bitboard = _pieceBitBoards[my_color_idx][PAWN_INDEX];

  // Generate legal moves for each pawn
  while (pawn_bitboard) {
    int8_t src_idx = pawn_bitboard.popRightmostBitIndex();
    BitBoard move_bitboard =
        BITBOARD_PAWN_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // If there are no valid destinations, move on to the next pawn
    if (!move_bitboard) {
      continue;
    }

    // Get the coordinates of the destination
    int8_t dst_idx = move_bitboard.getRightmostBitIndex();

    // Check if the move is possible (i.e., not a discovered check)
    if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
      continue;
    }

    // Check if the move can promote
    bool promote = BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

    // If only checking for check, register the move only if it gives check
    // Otherwise, register the move unconditionally
    if constexpr (checkOnly) {
      bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                             : _isCheckMove<PIECE_BLACK_PAWN>(src_idx, dst_idx);
      if (check) {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }
    } else {
      legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
    }

    // Register promoted moves, and if removeUnpromote is false, also register unpromoted moves
    // However, if the pawn moves to the first rank, it can only promote, so do not register unpromoted moves
    if constexpr (!removeUnpromote) {
      if (promote && BITBOARD_PAWN_DROPABLES[my_color_idx].hasBit(dst_idx)) {
        // If only checking for check, register the move only if it gives check
        // Otherwise, register the move unconditionally
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
 * Create legal moves for lances.
 * If the template parameter removeUnpromote is true, moves that do not promote lances are removed.
 * If the template parameter checkOnly is true, only moves that give check are obtained.
 * @param legalMoves Array object to which the list of legal moves for lances is added
 * @param destinationBitBoard Bitboard representing valid destination squares for lance moves
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalLanceMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Create bitboard for lances
  BitBoard lance_bitboard = _pieceBitBoards[my_color_idx][LANCE_INDEX];

  // Generate legal moves for each lance
  BitBoard occupied_bitboard = _colorBitBoards[0] | _colorBitBoards[1];

  while (lance_bitboard) {
    int8_t src_idx = lance_bitboard.popRightmostBitIndex();

    // Create bitboard for lance destinations
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

      // Check if the move is possible (i.e., not a discovered check)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // Check if the move can promote
      bool promote = BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

      // If only checking for check, register the move only if it gives check
      // Otherwise, register the move unconditionally
      if constexpr (checkOnly) {
        bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                               : _isCheckMove<PIECE_BLACK_LANCE>(src_idx, dst_idx);
        if (check) {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
        }
      } else {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }

      // If removeUnpromote is false, also register unpromoted moves for the 2nd and 3rd ranks
      // If removeUnpromote is true, only register unpromoted moves for the 3rd rank
      if constexpr (removeUnpromote) {
        // Use the same bitboard as for checking if a knight can be dropped for the 3rd rank
        if (promote && BITBOARD_KNIGHT_DROPABLES[my_color_idx].hasBit(dst_idx)) {
          // If only checking for check, register the move only if it gives check
          // Otherwise, register the move unconditionally
          if constexpr (checkOnly) {
            if (_isCheckMove<PIECE_BLACK_LANCE>(src_idx, dst_idx)) {
              legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
            }
          } else {
            legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
          }
        }
      } else {
        // Use the same bitboard as for checking if a pawn can be dropped for the 2nd rank
        if (promote && BITBOARD_PAWN_DROPABLES[my_color_idx].hasBit(dst_idx)) {
          // If only checking for check, register the move only if it gives check
          // Otherwise, register the move unconditionally
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
 * Generate legal moves for knights.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal knight moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for knight moves
 */
template <bool checkOnly>
void Board::_getLegalKnightMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Create a bitboard for knights
  BitBoard knight_bitboard = _pieceBitBoards[my_color_idx][KNIGHT_INDEX];

  // Generate legal moves for each knight
  while (knight_bitboard) {
    int8_t src_idx = knight_bitboard.popRightmostBitIndex();
    BitBoard move_bitboard =
        BITBOARD_KNIGHT_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // Generate legal moves for each destination
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // Check if the move is possible (i.e., not a discovered check)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // Check if the move can promote
      bool promote = BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

      // If only checking for check, register the move only if it gives check
      // Otherwise, register the move unconditionally
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
        // If only checking for check, register the move only if it gives check
        // Otherwise, register the move unconditionally
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
 * Generate legal moves for silvers.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal silver moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for silver moves
 */
template <bool checkOnly>
void Board::_getLegalSilverMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Create a bitboard for silvers
  BitBoard silver_bitboard = _pieceBitBoards[my_color_idx][SILVER_INDEX];

  // Generate legal moves for each silver
  while (silver_bitboard) {
    int8_t src_idx = silver_bitboard.popRightmostBitIndex();

    // Create a bitboard for silver moves
    BitBoard move_bitboard =
        BITBOARD_SILVER_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // Generate legal moves for each destination
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // Check if the move is possible (i.e., not a discovered check)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // Check if the move can promote
      bool promote =
          BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(src_idx) ||
          BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

      // If only checking for check, register the move only if it gives check
      // Otherwise, register the move unconditionally
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
        // If only checking for check, register the move only if it gives check
        // Otherwise, register the move unconditionally
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
 * Generate legal moves for golds.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal gold moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for gold moves
 */
template <bool checkOnly>
void Board::_getLegalGoldMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Create a bitboard for golds
  BitBoard gold_bitboard = _pieceBitBoards[my_color_idx][GOLD_INDEX];

  // Generate legal moves for each gold
  while (gold_bitboard) {
    int8_t src_idx = gold_bitboard.popRightmostBitIndex();

    // Create a bitboard for gold moves
    BitBoard move_bitboard =
        BITBOARD_GOLD_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // Generate legal moves for each destination
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // Check if the move is possible (i.e., not a discovered check)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // If only checking for check, register the move only if it gives check
      // Otherwise, register the move unconditionally
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
 * Generate legal moves for kings.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal king moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for king moves
 */
template <bool checkOnly>
void Board::_getLegalKingMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

  // Generate legal moves for the king
  // If the king's position is not set, do not generate legal moves for the king
  // The king's destination squares are not affected by the destinationBitBoard
  // The king's destination squares are limited to squares
  // that do not contain the king's own pieces and are not attacked by the opponent's pieces
  // Since the king does not give check, if only checking for check, do not include the king's moves
  if constexpr (!checkOnly) {
    if (king_pos_idx >= 0) {
      BitBoard king_move_bitboard =
          BITBOARD_KING_ATTACKS[king_pos_idx] & ~_colorBitBoards[my_color_idx];

      // Generate legal moves for each destination
      while (king_move_bitboard) {
        int8_t dst_idx = king_move_bitboard.popRightmostBitIndex();

        // Check if the move is possible (i.e., not a suicide move)
        if (_getAttackers<true, true>(_color, dst_idx).empty()) {
          legalMoves.emplace_back(Position(king_pos_idx), Position(dst_idx), false);
        }
      }
    }
  }

  // Create bitboards for promoted bishops (horses) and promoted rooks (dragons)
  BitBoard bishop_bitboard = _pieceBitBoards[my_color_idx][BISHOP_INDEX];
  BitBoard other_bitboard = _pieceBitBoards[my_color_idx][KING_INDEX];

  if (king_pos_idx >= 0) {
    other_bitboard.clearBit(king_pos_idx);
  }

  // Generate legal moves for each piece
  while (other_bitboard) {
    int8_t src_idx = other_bitboard.popRightmostBitIndex();
    BitBoard move_bitboard = BITBOARD_KING_ATTACKS[src_idx] & destinationBitBoard;

    // Generate legal moves for each destination
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // Check if the move is possible (i.e., not a discovered check)
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // If only checking for check, register the move only if it gives check
      // Otherwise, register the move unconditionally
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
 * Generate legal moves for bishops.
 * If the template parameter removeUnpromote is true, unpromoted moves are removed.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal bishop moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for bishop moves
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalBishopMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Generate legal moves for bishops
  BitBoard bishop_bitboard = _pieceBitBoards[my_color_idx][BISHOP_INDEX];

  // Generate legal moves for each bishop
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];

  while (bishop_bitboard) {
    int8_t src_idx = bishop_bitboard.popRightmostBitIndex();

    // Check if the bishop is already promoted (i.e., a horse)
    bool already_promoted =
        _pieceBitBoards[my_color_idx][KING_INDEX].hasBit(src_idx);

    // Check moves in 4 directions
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

      // Generate legal moves for each destination
      while (move_bitboard) {
        int8_t dst_idx = move_bitboard.popRightmostBitIndex();

        // Check if the move is possible (i.e., not a discovered check)
        if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
          continue;
        }

        // Check if the move can promote
        bool promote = !already_promoted &&
                       (BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(src_idx) ||
                        BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx));

        // If only checking for check, register the move only if it gives check
        // Otherwise, register the move unconditionally
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

        // If removeUnpromote is false, further check unpromoted moves
        if constexpr (!removeUnpromote) {
          // Add unpromoted moves
          if (promote) {
            // If only checking for check, register the move only if it gives check
            // Otherwise, register the move unconditionally
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
 * Generate legal moves for rooks.
 * If the template parameter removeUnpromote is true, unpromoted moves are removed.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal rook moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for rook moves
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalRookMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // Generate legal moves for rooks
  BitBoard rook_bitboard = _pieceBitBoards[my_color_idx][ROOK_INDEX];

  // Generate legal moves for each rook
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];

  while (rook_bitboard) {
    int8_t src_idx = rook_bitboard.popRightmostBitIndex();

    // Check if the rook is already promoted (i.e., a dragon)
    bool already_promoted =
        _pieceBitBoards[my_color_idx][KING_INDEX].hasBit(src_idx);

    // Check moves in 4 directions
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

      // Generate legal moves for each destination
      while (move_bitboard) {
        int8_t dst_idx = move_bitboard.popRightmostBitIndex();

        // Check if the move is possible (i.e., not a discovered check)
        if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
          continue;
        }

        // Check if the move can promote
        bool promote = !already_promoted &&
                       (BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(src_idx) ||
                        BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx));

        // If only checking for check, register the move only if it gives check
        // Otherwise, register the move unconditionally
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

        // If removeUnpromote is false, further check unpromoted moves
        if constexpr (!removeUnpromote) {
          // Add unpromoted moves
          if (promote) {
            // If only checking for check, register the move only if it gives check
            // Otherwise, register the move unconditionally
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
 * Generate legal moves for hand pawns.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal hand pawn moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for hand pawn moves
 */
template <bool checkOnly>
void Board::_getLegalHandPawnMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;

  // If there are no pawns in hand, there are no legal moves
  if (_hands[my_color_idx][PAWN_INDEX] == 0) {
    return;
  }

  // Create a bitboard for columns that already have pawns
  BitBoard pawn_exist_bitboard;
  BitBoard pawn_piece_bitboard = _pieceBitBoards[my_color_idx][PAWN_INDEX];

  while (pawn_piece_bitboard) {
    constexpr uint64_t mask = (1ULL << BOARD_SIZE) - 1;
    int8_t idx = pawn_piece_bitboard.popRightmostBitIndex();
    int8_t shift = idx / BOARD_SIZE * BOARD_SIZE;

    pawn_exist_bitboard |= BitBoard(mask, 0) << shift;
  }

  // Create a bitboard for squares where pawns can be dropped
  BitBoard hand_pawn_bitboard =
      BITBOARD_PAWN_DROPABLES[my_color_idx] & ~pawn_exist_bitboard & destinationBitBoard;

  // Calculate the squares that could result in check (i.e., potential pawn drop checkmate)
  int8_t op_king_idx = _kingPositions[op_color_idx].getIndex();
  int8_t pawn_mate_idx =
      (op_king_idx >= 0)
          ? BITBOARD_PAWN_ATTACKS[op_color_idx][op_king_idx].getRightmostBitIndex()
          : -1;

  // Generate legal moves for each destination
  while (hand_pawn_bitboard) {
    int8_t dst_idx = hand_pawn_bitboard.popRightmostBitIndex();

    // If the move is a pawn drop illegal checkmate, skip to the next destination
    if (dst_idx == pawn_mate_idx && _isDropPawnCheckmateMove(dst_idx)) {
      continue;
    }

    // If only checking for check, register the move only if it gives check
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
 * Generate legal moves for hand lances.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal hand lance moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for hand lance moves
 */
template <bool checkOnly>
void Board::_getLegalHandLanceMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If there are no lances in hand, there are no legal moves
  if (_hands[my_color_idx][LANCE_INDEX] == 0) {
    return;
  }

  // Create a bitboard for squares where lances can be dropped
  // The squares where lances can be dropped are the same as the squares where pawns can be dropped,
  // ignoring the two-pawn rule, so we reuse the pawn bitboard
  BitBoard hand_lance_bitboard = BITBOARD_PAWN_DROPABLES[my_color_idx] & destinationBitBoard;

  // Generate legal moves for each destination
  while (hand_lance_bitboard) {
    int8_t dst_idx = hand_lance_bitboard.popRightmostBitIndex();

    // If only checking for check, register the move only if it gives check
    // Otherwise, register the move unconditionally
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
 * Generate legal moves for hand knights.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal hand knight moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for hand knight moves
 */
template <bool checkOnly>
void Board::_getLegalHandKnightMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If there are no knights in hand, there are no legal moves
  if (_hands[my_color_idx][KNIGHT_INDEX] == 0) {
    return;
  }

  // Create a bitboard for squares where knights can be dropped
  BitBoard hand_knight_bitboard = BITBOARD_KNIGHT_DROPABLES[my_color_idx] & destinationBitBoard;

  // Generate legal moves for each destination
  while (hand_knight_bitboard) {
    int8_t dst_idx = hand_knight_bitboard.popRightmostBitIndex();

    // If only checking for check, register the move only if it gives check
    // Otherwise, register the move unconditionally
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
 * Generate legal moves for hand silvers.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal hand silver moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for hand silver moves
 */
template <bool checkOnly>
void Board::_getLegalHandSilverMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If there are no silvers in hand, there are no legal moves
  if (_hands[my_color_idx][SILVER_INDEX] == 0) {
    return;
  }

  // Create a bitboard for squares where silvers can be dropped
  BitBoard hand_silver_bitboard = destinationBitBoard;

  // Generate legal moves for each destination
  while (hand_silver_bitboard) {
    int8_t dst_idx = hand_silver_bitboard.popRightmostBitIndex();

    // If only checking for check, register the move only if it gives check
    // Otherwise, register the move unconditionally
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
 * Generate legal moves for hand golds.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal hand gold moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for hand gold moves
 */
template <bool checkOnly>
void Board::_getLegalHandGoldMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If there are no golds in hand, there are no legal moves
  if (_hands[my_color_idx][GOLD_INDEX] == 0) {
    return;
  }

  // Create a bitboard for squares where golds can be dropped
  BitBoard hand_gold_bitboard = destinationBitBoard;

  // Generate legal moves for each destination
  while (hand_gold_bitboard) {
    int8_t dst_idx = hand_gold_bitboard.popRightmostBitIndex();

    // If only checking for check, register the move only if it gives check
    // Otherwise, register the move unconditionally
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
 * Generate legal moves for hand bishops.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal hand bishop moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for hand bishop moves
 */
template <bool checkOnly>
void Board::_getLegalHandBishopMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If there are no bishops in hand, there are no legal moves
  if (_hands[my_color_idx][BISHOP_INDEX] == 0) {
    return;
  }

  // Create a bitboard for squares where bishops can be dropped
  BitBoard hand_bishop_bitboard = destinationBitBoard;

  // Generate legal moves for each destination
  while (hand_bishop_bitboard) {
    int8_t dst_idx = hand_bishop_bitboard.popRightmostBitIndex();

    // If only checking for check, register the move only if it gives check
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
 * Generate legal moves for hand rooks.
 * If the template parameter checkOnly is true, only moves that give check are generated.
 * @param legalMoves Array object to which the list of legal hand rook moves is added
 * @param destinationBitBoard Bitboard representing valid destination squares for hand rook moves
 */
template <bool checkOnly>
void Board::_getLegalHandRookMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // If there are no rooks in hand, there are no legal moves
  if (_hands[my_color_idx][ROOK_INDEX] == 0) {
    return;
  }

  // Create a bitboard for squares where rooks can be dropped
  BitBoard hand_rook_bitboard = destinationBitBoard;

  // Generate legal moves for each destination
  while (hand_rook_bitboard) {
    int8_t dst_idx = hand_rook_bitboard.popRightmostBitIndex();

    // If only checking for check, register the move only if it gives check
    // Otherwise, register the move unconditionally
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
 * Determine if a move from the specified source index to the specified destination index gives check.
 * The template parameter piece specifies the type of piece being moved (use PIECE_BLACK_XXX).
 * @param srcIndex Integer representing the source position
 * @param dstIndex Integer representing the destination position
 * @return True if the move from the specified source index to the specified destination index gives check
 */
template <uint8_t piece>
bool Board::_isCheckMove(int8_t srcIndex, int8_t dstIndex) const {
  return _isDropCheckMove<piece>(dstIndex) ||
         _isDiscoveredCheckMove(srcIndex, dstIndex, OPPOSITE_COLOR(_color));
}

/**
 * Determine if a move from the specified source index to the specified destination index gives discovered check.
 * @param srcIndex Integer representing the source position
 * @param dstIndex Integer representing the destination position
 * @param color The color of the king to check for
 * @return True if the move from the specified source index to the specified destination index gives discovered check
 */
bool Board::_isDiscoveredCheckMove(int8_t srcIndex, int8_t dstIndex, int8_t color) const {
  int8_t my_color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

  // If the king is not on the board, it cannot be a discovered check
  if (king_pos_idx < 0) {
    return false;
  }

  // Check the relationship with the king
  int8_t direction = DIRECTION_INDICES[srcIndex][king_pos_idx];

  // If the king is not on the extended line in any of the 8 directions, it cannot be a discovered check
  if (direction == 8) {
    return false;
  }

  // If the move direction is the same as or opposite to the king's direction, it cannot be a discovered check
  int8_t move_direction = DIRECTION_INDICES[srcIndex][dstIndex];

  if (move_direction == direction || move_direction == (direction + 4) % 8) {
    return false;
  }

  // If there are pieces between the king and the source, it cannot be a discovered check
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
  BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][srcIndex] & occ_bitboard;
  int8_t between_idx = (direction < 4)
                           ? between_bitboard.getLeftmostBitIndex()
                           : between_bitboard.getRightmostBitIndex();

  if (between_idx != king_pos_idx) {
    return false;
  }

  // Check the pieces on the opposite side
  BitBoard opposite_bitboard =
      BITBOARD_LONG_ATTACKS[(direction + 4) % 8][srcIndex] & occ_bitboard;
  int8_t opposite_idx = (direction < 4)
                            ? opposite_bitboard.getRightmostBitIndex()
                            : opposite_bitboard.getLeftmostBitIndex();

  // If there are no pieces, it cannot be a discovered check
  if (opposite_idx < 0) {
    return false;
  }
  // If the move direction is odd (diagonal) and the piece has bishop's attack, it is a discovered check
  else if ((direction % 2 == 1) &&
           _pieceBitBoards[op_color_idx][BISHOP_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // If the move direction is even (vertical/horizontal) and the piece has rook's attack, it is a discovered check
  else if ((direction % 2 == 0) &&
           _pieceBitBoards[op_color_idx][ROOK_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // If the move direction is down for black and the piece has lance's attack, it is a discovered check
  else if (color == COLOR_BLACK && direction == 4 &&
           _pieceBitBoards[op_color_idx][LANCE_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // If the move direction is up for white and the piece has lance's attack, it is a discovered check
  else if (color == COLOR_WHITE && direction == 0 &&
           _pieceBitBoards[op_color_idx][LANCE_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // In all other cases, it is not a discovered check
  else {
    return false;
  }
}

/**
 * Determine if a drop move to the specified destination index gives check.
 * The template parameter `piece` specifies the type of piece being dropped (specified with PIECE_BLACK_XXX).
 * @param dstIndex Integer representing the destination position
 * @return True if the drop move to the specified destination index gives check
 */
template <uint8_t piece>
bool Board::_isDropCheckMove(int8_t dstIndex) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t op_king_idx = _kingPositions[op_color_idx].getIndex();

  // If the king is not on the board, it cannot be a check
  if (op_king_idx < 0) {
    return false;
  }

  // If the piece is a pawn (drop pawn checkmate is checked separately, so it is not considered here)
  if constexpr (piece == PIECE_BLACK_PAWN) {
    return BITBOARD_PAWN_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // If the piece is a lance
  if constexpr (piece == PIECE_BLACK_LANCE) {
    // If the move direction is not up for black or not down for white, it cannot be a check
    int8_t direction = DIRECTION_INDICES[dstIndex][op_king_idx];

    if ((_color == COLOR_BLACK && direction != 0) ||
        (_color == COLOR_WHITE && direction != 4)) {
      return false;
    }

    // If there are no pieces between the lance and the opponent's king, it is a check
    BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
    BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][dstIndex] & occ_bitboard;
    int8_t between_idx = (direction < 4)
                             ? between_bitboard.getLeftmostBitIndex()
                             : between_bitboard.getRightmostBitIndex();

    return between_idx == op_king_idx;
  }

  // If the piece is a knight
  if constexpr (piece == PIECE_BLACK_KNIGHT) {
    return BITBOARD_KNIGHT_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // If the piece is a silver
  if constexpr (piece == PIECE_BLACK_SILVER) {
    return BITBOARD_SILVER_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // If the piece is a gold
  if constexpr (piece == PIECE_BLACK_GOLD) {
    return BITBOARD_GOLD_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // If the piece is a horse or dragon (the attacks of the flying pieces are checked later)
  if constexpr (piece == PIECE_BLACK_HORSE || piece == PIECE_BLACK_DRAGON) {
    if (BITBOARD_KING_ATTACKS[dstIndex].hasBit(op_king_idx)) {
      return true;
    }
  }

  // If the piece is a bishop (the horse's one-square move has already been checked,
  // so we look at the diagonal direction)
  if constexpr (piece == PIECE_BLACK_BISHOP || piece == PIECE_BLACK_HORSE) {
    // If the direction to the opponent's king is not diagonal, it cannot be a check
    int8_t direction = DIRECTION_INDICES[dstIndex][op_king_idx];

    if (direction % 2 == 0) {
      return false;
    }

    // If there are no pieces between the bishop/horse and the opponent's king, it is a check
    BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
    BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][dstIndex] & occ_bitboard;
    int8_t between_idx = (direction < 4)
                             ? between_bitboard.getLeftmostBitIndex()
                             : between_bitboard.getRightmostBitIndex();

    return between_idx == op_king_idx;
  }

  // If the piece is a rook (the dragon's one-square move has already been checked,
  //  so we look at the vertical and horizontal directions)
  if constexpr (piece == PIECE_BLACK_ROOK || piece == PIECE_BLACK_DRAGON) {
    // If the direction to the opponent's king is not vertical or horizontal, it cannot be a check
    int8_t direction = DIRECTION_INDICES[dstIndex][op_king_idx];

    if (direction == 8 || direction % 2 == 1) {
      return false;
    }

    // If there are no pieces between the rook/dragon and the opponent's king, it is a check
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
 * Returns whether a pawn drop to the specified position results in a checkmate.
 * @param dstIndex The index representing the destination position
 * @return true if a pawn drop to the specified position results in a checkmate
 */
bool Board::_isDropPawnCheckmateMove(int8_t dstIndex) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t op_king_idx = _kingPositions[op_color_idx].getIndex();

  // If the opponent's king is not on the board, it cannot be a pawn drop checkmate
  if (op_king_idx < 0) {
    return false;
  }

  // If the specified position is not directly in front of the king, it cannot be a pawn drop checkmate
  int8_t pawn_mate_idx =
      BITBOARD_PAWN_ATTACKS[op_color_idx][op_king_idx].getRightmostBitIndex();

  if (dstIndex != pawn_mate_idx) {
    return false;
  }

  // If there are no pieces attacking any of the squares the king can move to, it cannot be a pawn drop checkmate
  // Consider the possibility that the pawn may block the attacks of flying pieces
  BitBoard king_move_bitboard =
      BITBOARD_KING_ATTACKS[op_king_idx] & ~_colorBitBoards[op_color_idx];

  while (king_move_bitboard) {
    int8_t dst_idx = king_move_bitboard.popRightmostBitIndex();

    if (_getAttackers<true, true>(OPPOSITE_COLOR(_color), dst_idx, pawn_mate_idx).empty()) {
      return false;
    }
  }

  // If any piece other than the king can move to the pawn's position, it cannot be a pawn drop checkmate
  for (int8_t attacker_pos : _getAttackers<false, false>(_color, pawn_mate_idx)) {
    if (attacker_pos != op_king_idx &&
        !_isDiscoveredCheckMove(attacker_pos, pawn_mate_idx, OPPOSITE_COLOR(_color))) {
      return false;
    }
  }

  // If none of the above conditions are met, it is a pawn drop checkmate
  return true;
}

/**
 * Returns the board data to be input to the model.
 * @param inputs The board data to be input to the model
 * @param color The color of the player to move (COLOR_BLACK or COLOR_WHITE)
 */
void Board::_getBoardInputs(int32_t* inputs, int8_t color) const {
  const static int32_t black_offset = 1;
  const static int32_t white_offset = black_offset + 14 + 14 + 6;
  const static int32_t other_offset = white_offset + 14 + 14 + 6;
  const static int32_t board_square = BOARD_SIZE * BOARD_SIZE;

  for (int32_t src = 0; src < board_square; src++) {
    // Calculate the index where the value will be set
    // If it is the opponent's turn, rotate the board 180 degrees
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

    // Set the values for piece attacks
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

    // Set the number of attacks on the piece
    black_att_count = std::min(black_att_count, 5);
    white_att_count = std::min(white_att_count, 5);

    setInputBit(inputs, (black_offset + 14 + 14 + black_att_count) * board_square + dst);
    setInputBit(inputs, (white_offset + 14 + 14 + white_att_count) * board_square + dst);

    // Set the coordinates of the last moved piece
    if (_lastMove != MOVE_INVALID && _lastMove.getDst().getIndex() == src) {
      setInputBit(inputs, other_offset * board_square + dst);
    }

    // Set the row and column numbers
    const int32_t row_offset = other_offset + 1;
    const int32_t col_offset = row_offset + BOARD_SIZE;
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
 * Returns the game data to be input to the model.
 * @param inputs The game data to be input to the model
 * @param color The color of the player to move (COLOR_BLACK or COLOR_WHITE)
 */
void Board::_getInfoInputs(int32_t* inputs, int8_t color) const {
  const static int32_t info_offset = MODEL_FEATURES * BOARD_SIZE * BOARD_SIZE;
  const static int32_t hand_offsets[] = {0, 18, 22, 26, 30, 32, 34};
  const static int32_t color_offset = 38;

  // Set the information for hand pieces
  int32_t black_color = (color == COLOR_BLACK) ? COLOR_BLACK : COLOR_WHITE;
  int32_t white_color = (color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;

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

  // Set the information for check
  if (isCheck(_color)) {
    setInputBit(inputs, info_offset + color_offset * 2);
  }

  // Set the points required for nyugyoku declaration
  if (color == COLOR_BLACK) {
    inputs[MODEL_INPUT_PACK_SIZE - 3] = (int)((_nyugyokuScores[0] - 27.5) / 5.0 * 0xfffff);
    inputs[MODEL_INPUT_PACK_SIZE - 2] = (int)((_nyugyokuScores[1] - 27.5) / 5.0 * 0xfffff);
  } else {
    inputs[MODEL_INPUT_PACK_SIZE - 3] = (int)((_nyugyokuScores[1] - 27.5) / 5.0 * 0xfffff);
    inputs[MODEL_INPUT_PACK_SIZE - 2] = (int)((_nyugyokuScores[0] - 27.5) / 5.0 * 0xfffff);
  }

  // Set the remaining turns until a draw
  float remaining_turn = 1.0f - (_drawTurn - _turn) / 50.0f;

  remaining_turn = std::min(std::max(remaining_turn, 0.0f), 1.0f);
  inputs[MODEL_INPUT_PACK_SIZE - 1] = (int)(remaining_turn * 0xfffff);
}

}  // namespace deepshogi

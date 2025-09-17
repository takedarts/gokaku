#include "Board.h"

#include <iostream>

#include "Config.h"

namespace deepshogi {

/**
 * Create an instance of the initial board.
 * @param nyugyokuScoreBlack Score required for nyugyoku declaration for black
 * @param nyugyokuScoreWhite Score required for nyugyoku declaration for white
 * @param drawSteps Number of moves until draw
 */
Board::Board(int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps)
    : _board(),
      _nyugyokuScoreBlack(nyugyokuScoreBlack),
      _nyugyokuScoreWhite(nyugyokuScoreWhite),
      _drawSteps(drawSteps) {
}

/**
 * Create an instance by specifying the board.
 * @param board Object holding board information
 */
Board::Board(const Board& board)
    : _board(board._board),
      _nyugyokuScoreBlack(board._nyugyokuScoreBlack),
      _nyugyokuScoreWhite(board._nyugyokuScoreWhite),
      _drawSteps(board._drawSteps) {
}

/**
 * Create a board instance.
 */
Board::Board()
    : _board(),
      _nyugyokuScoreBlack(28),
      _nyugyokuScoreWhite(27),
      _drawSteps(0x7fffffff) {
}

/**
 * Initialize the board with an SFEN string.
 * @param sfen SFEN string
 */
void Board::initializeWithSfen(const std::string& sfen) {
  _board.set(sfen);
}

/**
 * Initialize the board with Huffman encoded board information.
 * @param data Huffman encoded board information
 */
void Board::initializeWithPackedSfen(char* data) {
  _board.set_psfen(data);
}

/**
 * Move a piece.
 * @param move Move
 * @return True if legal move
 */
bool Board::play(const Move& move) {
  // Create move information
  int move_src = move.getSrcX() * BOARD_SIZE + move.getSrcY();
  int move_dst = move.getDstX() * BOARD_SIZE + move.getDstY();
  int move_value = _board.move(move_src, move_dst, move.isPromote());

  // Check if the move is possible
  if (!_board.moveIsLegal(move_value)) {
    return false;
  }

  // Move the piece
  _board.push(move_value);

  return true;
}

/**
 * Get side to move.
 * @return Side to move
 */
int32_t Board::getColor() const {
  if (_board.turn() == cshogi::White) {
    return COLOR_WHITE;
  } else {
    return COLOR_BLACK;
  }
}

/**
 * Get current number of moves.
 * @return Current number of moves
 */
int32_t Board::getTurn() const {
  return _board.pos.gamePly() - 1;
}

/**
 * Get the piece at the specified coordinates.
 * @param x X coordinate
 * @param y Y coordinate
 * @return Type of piece
 */
int32_t Board::getPiece(int32_t x, int32_t y) const {
  return _board.piece(x * BOARD_SIZE + y);
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

    if (getColor() == COLOR_WHITE) {
      return PIECE_WHITE_BEGIN + hand_piece;
    } else {
      return PIECE_BLACK_BEGIN + hand_piece;
    }
  }

  // Get the type of piece at the source
  int32_t piece = getPiece(move.getSrcX(), move.getSrcY());

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
  cshogi::Color cshogi_color = (color == COLOR_WHITE) ? cshogi::White : cshogi::Black;
  cshogi::Hand cshogi_hand = _board.pos.hand(cshogi_color);

  return cshogi_hand.numOf(cshogi::HandPiece(piece));
}

/**
 * Get a list of coordinates of pieces attacking the specified square.
 * @param x X coordinate
 * @param y Y coordinate
 * @return List of coordinates of attacking pieces
 */
std::vector<std::pair<int32_t, int32_t>> Board::getAttackers(int32_t x, int32_t y) const {
  std::vector<std::pair<int32_t, int32_t>> attackers;

  cshogi::Square sq = cshogi::makeSquare(cshogi::File(x), cshogi::Rank(y));
  cshogi::Bitboard bb = _board.pos.attackersTo(sq, _board.pos.occupiedBB());

  while (bb) {
    cshogi::Square sq = bb.firstOneFromSQ11();
    attackers.push_back(std::make_pair(cshogi::makeFile(sq), cshogi::makeRank(sq)));
  }

  return attackers;
}

/**
 * Get list of legal moves for the current board.
 * Returns a list of legal moves with non-promoting moves for pawn, bishop, and rook removed.
 * @return List of legal moves
 */
std::vector<Move> Board::getLegalMoves() const {
  std::vector<Move> moves;
  cshogi::__LegalMoveList ml(_board);

  while (!ml.end()) {
    // Get legal move
    Move move(ml.move());
    ml.next();

    // If move is from hand, add to list of legal moves
    if (move.getSrcX() == BOARD_SIZE) {
      moves.push_back(move);
      continue;
    }

    // If promoted, add to list of legal moves
    if (move.isPromote()) {
      moves.push_back(move);
      continue;
    }

    // Get the type of piece at the source
    int32_t piece = getPiece(move.getSrcX(), move.getSrcY());

    // For black pawn, bishop, and rook, add moves that do not enter opponent's camp to list of legal moves
    if (piece == PIECE_BLACK_PAWN ||
        piece == PIECE_BLACK_BISHOP ||
        piece == PIECE_BLACK_ROOK) {
      if (move.getSrcY() >= 3 && move.getDstY() >= 3) {
        moves.push_back(move);
        continue;
      }
    }
    // For white pawn, bishop, and rook, add moves that do not enter opponent's camp to list of legal moves
    else if (piece == PIECE_WHITE_PAWN ||
             piece == PIECE_WHITE_BISHOP ||
             piece == PIECE_WHITE_ROOK) {
      if (move.getSrcY() <= 5 && move.getDstY() <= 5) {
        moves.push_back(move);
        continue;
      }
    }
    // Otherwise, add to list of legal moves
    else {
      moves.push_back(move);
    }
  }

  return moves;
}

/**
 * Get move history.
 * @return Move history
 */
std::vector<Move> Board::getHistoryMoves() const {
  std::vector<Move> moves;

  for (int move : _board.get_history()) {
    moves.push_back(Move(move));
  }

  return moves;
}

/**
 * Search for checkmate sequence and return the first move.
 * If no checkmate sequence is found, returns pass (MOVE_PASS).
 * If checkSearchNode is 0, performs exhaustive search.
 * If checkSearchNode is 1 or more, uses the df-pn algorithm for search.
 * @param checkSearchDepth Depth for checkmate sequence search
 * @param checkSearchNode Number of nodes for checkmate sequence search (0 for exhaustive search)
 * @return Move in checkmate sequence
 */
Move Board::searchCheckMove(int32_t checkSearchDepth, int32_t checkSearchNode) {
  int move = 0;

  // If depth is 5 or less, perform single-move search
  if (checkSearchNode < 1) {
    move = _board.mateMove(checkSearchDepth);
  }
  // Otherwise, perform DfPn search
  else {
    cshogi::__DfPn dfpn(checkSearchDepth, checkSearchNode, 0x7ffffffe);

    if (dfpn.search(_board)) {
      move = dfpn.get_move(_board);
    }
  }

  // If checkmate sequence is found, return the move
  // If not found, return MOVE_PASS
  if (move == 0) {
    return MOVE_PASS;
  } else {
    return Move(move);
  }
}

/**
 * Return true if nyugyoku declaration is possible.
 * @return True if nyugyoku declaration is possible
 */
bool Board::isNyugyoku() const {
  // 1. It is the declaring side's turn.
  // 6. The declaring side has remaining time.

  // 5. The king of the declaring side is not in check.
  if (_board.pos.inCheck()) {
    return false;
  }

  // Get side to move
  const cshogi::Color color = _board.pos.turn();

  // Create mask for opponent's camp
  const cshogi::Bitboard opponents_field =
      color == cshogi::Black
          ? cshogi::inFrontMask<cshogi::Black, cshogi::Rank4>()
          : cshogi::inFrontMask<cshogi::White, cshogi::Rank6>();

  // 2. The king of the declaring side is within the opponent's third rank.
  if (!_board.pos.bbOf(cshogi::King, color).andIsAny(opponents_field))
    return false;

  // 4. There are at least 10 pieces (excluding the king) of the declaring side within the opponent's third rank.
  const int pieces_count = (_board.pos.bbOf(color) & opponents_field).popCount() - 1;

  if (pieces_count < 10) {
    return false;
  }

  // 3. The declaring side scores 5 points for major pieces and 1 point for minor pieces.
  // Only the declaring side's pieces (excluding the king) within the opponent's third rank and its hand pieces are counted for scoring.
  const cshogi::Bitboard big_pieces =
      _board.pos.bbOf(cshogi::Rook, cshogi::Dragon, cshogi::Bishop, cshogi::Horse) &
      opponents_field & _board.pos.bbOf(color);
  const cshogi::Hand hand = _board.pos.hand(color);

  const int big_pieces_count =
      big_pieces.popCount() + hand.numOf<cshogi::HRook>() + hand.numOf<cshogi::HBishop>();
  const int small_pieces_count =
      pieces_count - big_pieces_count +
      hand.numOf<cshogi::HPawn>() + hand.numOf<cshogi::HLance>() + hand.numOf<cshogi::HKnight>() +
      hand.numOf<cshogi::HSilver>() + hand.numOf<cshogi::HGold>();
  int score = small_pieces_count + big_pieces_count * 5;

  // Get required score for nyugyoku declaration
  int required_score = 0;

  if (color == cshogi::Black) {
    required_score = _nyugyokuScoreBlack;
  } else {
    required_score = _nyugyokuScoreWhite;
  }

  // Check if the required score for nyugyoku declaration is met
  return score >= required_score;
}

/**
 * Return true if in checkmate.
 * @return True if in checkmate
 */
bool Board::isCheckmate() const {
  return _board.pos.inCheck();
}

/**
 * Get SFEN string.
 * @return SFEN string
 */
std::string Board::getSfen() const {
  return _board.toSFEN();
}

/**
 * Get Huffman encoded board information.
 * @param data Buffer to store Huffman encoded board information
 */
void Board::getPackedSfen(char* data) const {
  _board.toPackedSfen(data);
}

/**
 * Get data to input to the model.
 * @param inputs Data to input to the model
 */
void Board::getInputs(float* inputs) const {
  getInputs(inputs, getColor(), _drawSteps - getTurn());
}

/**
 * Get data to input to the model.
 * @param inputs Data to input to the model
 * @param color Side to move
 * @param steps Number of moves
 */
void Board::getInputs(float* inputs, int32_t color, int32_t steps) const {
  // Initialize with 0
  std::fill_n(inputs, MODEL_INPUT_SIZE, 0);

  // Get input data for the model
  float* board_inputs = inputs;
  float* info_inputs = inputs + MODEL_FEATURES * BOARD_SIZE * BOARD_SIZE;

  _getBoardInputs(board_inputs, color);
  _getInfoInputs(info_inputs, color, steps);
}

/**
 * Copy the board state.
 * @param board Source board to copy from
 */
void Board::copyFrom(const Board* board) {
  _board = board->_board;
  _nyugyokuScoreBlack = board->_nyugyokuScoreBlack;
  _nyugyokuScoreWhite = board->_nyugyokuScoreWhite;
  _drawSteps = board->_drawSteps;
}

/**
 * Get a string for displaying board information.
 * @return String for displaying board information
 */
std::string Board::dump() const {
  return _board.dump();
}

/**
 * Output the state of the board.
 * @param os Output destination
 */
void Board::print(std::ostream& os) const {
  os << dump() << std::endl;
}

/**
 * Get board data to input to the model.
 * @param inputs Board data to input to the model
 * @param color Side to move
 */
void Board::_getBoardInputs(float* inputs, int32_t color) const {
  typedef float board_feats_t[MODEL_FEATURES][BOARD_SIZE * BOARD_SIZE];
  board_feats_t* const feats = reinterpret_cast<board_feats_t* const>(inputs);

  // Set the color
  cshogi::Color black_color = (color == COLOR_WHITE) ? cshogi::White : cshogi::Black;
  cshogi::Color white_color = (color == COLOR_WHITE) ? cshogi::Black : cshogi::White;

  // Get the piece placement
  const cshogi::Bitboard occupied_bb = _board.pos.occupiedBB();

  // Check piece attacks and make them accessible by bit operations (merged by piece type)
  cshogi::Bitboard attacks[cshogi::ColorNum][cshogi::PieceTypeNum] = {
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}},
  };

  for (cshogi::Square sq = cshogi::SQ11; sq < cshogi::SquareNum; sq++) {
    const cshogi::Piece p = _board.pos.piece(sq);

    if (p != cshogi::Empty) {
      const cshogi::Color pc = pieceToColor(p);
      const cshogi::PieceType pt = pieceToPieceType(p);
      const cshogi::Bitboard bb = _board.pos.attacksFrom(pt, pc, sq, occupied_bb);
      attacks[pc][pt] |= bb;
    }
  }

  // Get the list of piece placement coordinates
  cshogi::Bitboard empty_bb = _board.pos.emptyBB();
  cshogi::Bitboard black_bb[cshogi::PieceTypeNum];
  cshogi::Bitboard white_bb[cshogi::PieceTypeNum];

  for (cshogi::PieceType pt = cshogi::Pawn; pt < cshogi::PieceTypeNum; ++pt) {
    black_bb[pt] = _board.pos.bbOf(pt, black_color);
    white_bb[pt] = _board.pos.bbOf(pt, white_color);
  }

  // Set the board information
  const size_t black_offset = 1;
  const size_t white_offset = black_offset + 2 * cshogi::PIECETYPE_NUM + 6;
  const size_t other_offset = white_offset + 2 * cshogi::PIECETYPE_NUM + 6;
  const int last_move = _board.get_last_move();

  for (cshogi::Square sq_dst = cshogi::SQ11; sq_dst < cshogi::SquareNum; ++sq_dst) {
    // For the second player, rotate the board 180 degrees
    cshogi::Square sq_src = sq_dst;

    if (black_color == cshogi::White) {
      sq_src = cshogi::SQ99 - sq_dst;
    }

    // Set the value for empty squares
    if (empty_bb.isSet(sq_src)) {
      (*feats)[0][sq_dst] = 1;
    }

    // Set piece placement and attacks
    for (cshogi::PieceType pt = cshogi::Pawn; pt < cshogi::PieceTypeNum; ++pt) {
      const size_t pt_idx = pt - cshogi::Pawn;

      // Set the value for piece placement
      if (black_bb[pt].isSet(sq_src)) {
        (*feats)[black_offset + pt_idx][sq_dst] = 1;
      }

      if (white_bb[pt].isSet(sq_src)) {
        (*feats)[white_offset + pt_idx][sq_dst] = 1;
      }

      // Set the value for piece attacks
      if (attacks[black_color][pt].isSet(sq_src)) {
        (*feats)[black_offset + cshogi::PIECETYPE_NUM + pt_idx][sq_dst] = 1;
      }

      if (attacks[white_color][pt].isSet(sq_src)) {
        (*feats)[white_offset + cshogi::PIECETYPE_NUM + pt_idx][sq_dst] = 1;
      }
    }

    // Set the number of piece attacks
    const int black_att_count = std::min(
        _board.pos.attackersTo(black_color, sq_src, occupied_bb).popCount(), 5);
    const int white_att_count = std::min(
        _board.pos.attackersTo(white_color, sq_src, occupied_bb).popCount(), 5);

    (*feats)[black_offset + cshogi::PIECETYPE_NUM * 2 + black_att_count][sq_dst] = 1;
    (*feats)[white_offset + cshogi::PIECETYPE_NUM * 2 + white_att_count][sq_dst] = 1;

    // Set the attack of the last moved piece
    if (last_move != 0 && (last_move & 0x7f) == sq_src) {
      (*feats)[other_offset][sq_dst] = 1;
    }

    // Set the row and column numbers
    const size_t row_offset = other_offset + 1;
    const size_t row_index = sq_dst % BOARD_SIZE;
    const size_t col_offset = row_offset + BOARD_SIZE;
    const size_t col_index = sq_dst / BOARD_SIZE;

    (*feats)[row_offset + row_index][sq_dst] = 1;
    (*feats)[col_offset + std::min(col_index, BOARD_SIZE - 1 - col_index)][sq_dst] = 1;
  }
}

/**
 * Get game data to input to the model.
 * @param inputs Game data to input to the model
 * @param color Side to move
 * @param steps Number of moves
 */
void Board::_getInfoInputs(float* inputs, int32_t color, int32_t steps) const {
  const static size_t hand_offsets[] = {0, 18, 22, 26, 30, 32, 34};
  const static size_t color_offset = 38;

  // Set information about pieces in hand
  cshogi::Color black_color = (color == COLOR_WHITE) ? cshogi::White : cshogi::Black;
  cshogi::Color white_color = (color == COLOR_WHITE) ? cshogi::Black : cshogi::White;
  const cshogi::Hand black_hand = _board.pos.hand(black_color);
  const cshogi::Hand white_hand = _board.pos.hand(white_color);

  for (cshogi::HandPiece hp = cshogi::HPawn; hp < cshogi::HandPieceNum; ++hp) {
    u32 black_num = black_hand.numOf(hp);
    u32 white_num = white_hand.numOf(hp);

    for (int32_t i = 0; i < black_num; i++) {
      inputs[hand_offsets[hp] + i] = 1;
    }

    for (int32_t i = 0; i < white_num; i++) {
      inputs[color_offset + hand_offsets[hp] + i] = 1;
    }
  }

  // Set information about check
  if (_board.pos.inCheck()) {
    inputs[color_offset * 2 + 0] = 1;
  }

  // Set the points required for entering king declaration
  if (color == COLOR_BLACK) {
    inputs[color_offset * 2 + 1] = (_nyugyokuScoreBlack - 27.5) / 5.0;
    inputs[color_offset * 2 + 2] = (_nyugyokuScoreWhite - 27.5) / 5.0;
  } else {
    inputs[color_offset * 2 + 1] = (_nyugyokuScoreWhite - 27.5) / 5.0;
    inputs[color_offset * 2 + 2] = (_nyugyokuScoreBlack - 27.5) / 5.0;
  }

  // Set the remaining number of moves until a draw
  float remaining_steps = 1.0 - (_drawSteps - steps) / 50.0;

  remaining_steps = std::min(std::max(remaining_steps, 0.0f), 1.0f);
  inputs[color_offset * 2 + 3] = remaining_steps;
}

}  // namespace deepshogi

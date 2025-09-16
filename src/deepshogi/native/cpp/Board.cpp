#include "Board.h"

#include <iostream>

#include "Config.h"

namespace deepshogi {

/**
 * 初期盤面のインスタンスを生成する。
 * @param nyugyokuScoreBlack 先手番の入玉宣言に必要な点数
 * @param nyugyokuScoreWhite 後手番の入玉宣言に必要な点数
 * @param drawSteps 引き分けとなるまでの手数
 */
Board::Board(int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps)
    : _board(),
      _nyugyokuScoreBlack(nyugyokuScoreBlack),
      _nyugyokuScoreWhite(nyugyokuScoreWhite),
      _drawSteps(drawSteps) {
}

/**
 * 盤面を指定してインスタンスを生成する。
 * @param board 盤面情報を保持するオブジェクト
 */
Board::Board(const Board& board)
    : _board(board._board),
      _nyugyokuScoreBlack(board._nyugyokuScoreBlack),
      _nyugyokuScoreWhite(board._nyugyokuScoreWhite),
      _drawSteps(board._drawSteps) {
}

/**
 * 盤面のインスタンスを生成する。
 */
Board::Board()
    : _board(),
      _nyugyokuScoreBlack(28),
      _nyugyokuScoreWhite(27),
      _drawSteps(0x7fffffff) {
}

/**
 * SFEN形式の文字列で盤面を初期化する。
 * @param sfen SFEN形式の文字列
 */
void Board::initializeWithSfen(const std::string& sfen) {
  _board.set(sfen);
}

/**
 * ハフマン符号化された盤面情報で盤面を初期化する。
 * @param data ハフマン符号化された盤面情報
 */
void Board::initializeWithPackedSfen(char* data) {
  _board.set_psfen(data);
}

/**
 * 駒を動かす。
 * @param move 着手
 * @return 合法手ならtrue
 */
bool Board::play(const Move& move) {
  // 移動情報を作成する
  int move_src = move.getSrcX() * BOARD_SIZE + move.getSrcY();
  int move_dst = move.getDstX() * BOARD_SIZE + move.getDstY();
  int move_value = _board.move(move_src, move_dst, move.isPromote());

  // 移動できることを確認する
  if (!_board.moveIsLegal(move_value)) {
    return false;
  }

  // 駒を動かす
  _board.push(move_value);

  return true;
}

/**
 * 手番を取得する。
 * @return 手番
 */
int32_t Board::getColor() const {
  if (_board.turn() == cshogi::White) {
    return COLOR_WHITE;
  } else {
    return COLOR_BLACK;
  }
}

/**
 * 現在の手数を取得する。
 * @return 現在の手数
 */
int32_t Board::getTurn() const {
  return _board.pos.gamePly() - 1;
}

/**
 * 指定された座標の駒を取得する。
 * @param x x座標
 * @param y y座標
 * @return 駒の種類
 */
int32_t Board::getPiece(int32_t x, int32_t y) const {
  return _board.piece(x * BOARD_SIZE + y);
}

/**
 * 移動した後の駒の種類を取得する。
 * @param move 着手
 * @return 駒の種類
 */
int32_t Board::getMovedPiece(const Move& move) const {
  // 　持ち駒から打つ場合、移動元のY座標に打つ駒の種類が入っている
  if (move.getSrcX() == BOARD_SIZE) {
    int32_t hand_piece = move.getSrcY();

    if (getColor() == COLOR_WHITE) {
      return PIECE_WHITE_BEGIN + hand_piece;
    } else {
      return PIECE_BLACK_BEGIN + hand_piece;
    }
  }

  // 移動元の駒の種類を取得する
  int32_t piece = getPiece(move.getSrcX(), move.getSrcY());

  // 成りの場合は成りの駒に変換する
  if (move.isPromote()) {
    piece += PIECE_PROMOTE;
  }

  return piece;
}

/**
 * 指定された種類の持ち駒の数を取得する。
 * @param color 手番
 * @param piece 駒の種類
 * @return 持ち駒の数
 */
int32_t Board::getHandPieceNum(int32_t color, int32_t piece) const {
  cshogi::Color cshogi_color = (color == COLOR_WHITE) ? cshogi::White : cshogi::Black;
  cshogi::Hand cshogi_hand = _board.pos.hand(cshogi_color);

  return cshogi_hand.numOf(cshogi::HandPiece(piece));
}

/**
 * 指定された座標に利いている駒の座標の一覧を取得する。
 * @param x x座標
 * @param y y座標
 * @return 利いている駒の座標の一覧
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
 * 現在の盤面の合法手の一覧を取得する。
 * 歩、角、飛車の不成の手を削除した合法手の一覧を返す。
 * @return 合法手の一覧
 */
std::vector<Move> Board::getLegalMoves() const {
  std::vector<Move> moves;
  cshogi::__LegalMoveList ml(_board);

  while (!ml.end()) {
    // 合法手を取得する
    Move move(ml.move());
    ml.next();

    // 持ち駒からの着手の場合は合法手の一覧に追加する
    if (move.getSrcX() == BOARD_SIZE) {
      moves.push_back(move);
      continue;
    }

    // 成りの場合は合法手の一覧に追加する
    if (move.isPromote()) {
      moves.push_back(move);
      continue;
    }

    // 移動元の駒の種類を取得する
    int32_t piece = getPiece(move.getSrcX(), move.getSrcY());

    // 黒番の歩、角、飛車の場合は相手陣に入らない手を合法手の一覧に追加する
    if (piece == PIECE_BLACK_PAWN ||
        piece == PIECE_BLACK_BISHOP ||
        piece == PIECE_BLACK_ROOK) {
      if (move.getSrcY() >= 3 && move.getDstY() >= 3) {
        moves.push_back(move);
        continue;
      }
    }
    // 白番の歩、角、飛車の場合は相手陣に入らない手を合法手の一覧に追加する
    else if (piece == PIECE_WHITE_PAWN ||
             piece == PIECE_WHITE_BISHOP ||
             piece == PIECE_WHITE_ROOK) {
      if (move.getSrcY() <= 5 && move.getDstY() <= 5) {
        moves.push_back(move);
        continue;
      }
    }
    // それ以外の場合は合法手の一覧に追加する
    else {
      moves.push_back(move);
    }
  }

  return moves;
}

/**
 * 着手履歴を取得する。
 * @return 着手履歴
 */
std::vector<Move> Board::getHistoryMoves() const {
  std::vector<Move> moves;

  for (int move : _board.get_history()) {
    moves.push_back(Move(move));
  }

  return moves;
}

/**
 * 詰み筋を探索して、最初の着手を返す。
 * 詰み筋が見つからない場合はパス（MOVE_PASS）を返す。
 * checkSearchNodeが0の場合は全探索を行う。
 * checkSearchNodeが1以上の場合はdf-pnアルゴリズムを使用して探索を行う。
 * @param checkSearchDepth 詰み手筋の探索深さ
 * @param checkSearchNode 詰み手筋の探索ノード数（0なら全探索）
 * @return 詰み筋の着手
 */
Move Board::searchCheckMove(int32_t checkSearchDepth, int32_t checkSearchNode) {
  int move = 0;

  // 深さが5以下の場合は単手数探索を実行する
  if (checkSearchNode < 1) {
    move = _board.mateMove(checkSearchDepth);
  }
  // それ以外はDfPn探索を実行する
  else {
    cshogi::__DfPn dfpn(checkSearchDepth, checkSearchNode, 0x7ffffffe);

    if (dfpn.search(_board)) {
      move = dfpn.get_move(_board);
    }
  }

  // 詰み手筋が見つかった場合は着手を返す
  // 見つからなかった場合はMOVE_PASSを返す
  if (move == 0) {
    return MOVE_PASS;
  } else {
    return Move(move);
  }
}

/**
 * 入玉宣言可能な状態であればtrueを返す。
 * @return 入玉宣言可能な状態であればtrue
 */
bool Board::isNyugyoku() const {
  // 一 宣言側の手番である。
  // 六 宣言側の持ち時間が残っている。

  // 五 宣言側の玉に王手がかかっていない。
  if (_board.pos.inCheck()) {
    return false;
  }

  // 手番を取得する
  const cshogi::Color color = _board.pos.turn();

  // 敵陣のマスクを作成する
  const cshogi::Bitboard opponents_field =
      color == cshogi::Black
          ? cshogi::inFrontMask<cshogi::Black, cshogi::Rank4>()
          : cshogi::inFrontMask<cshogi::White, cshogi::Rank6>();

  // 二 宣言側の玉が敵陣三段目以内に入っている。
  if (!_board.pos.bbOf(cshogi::King, color).andIsAny(opponents_field))
    return false;

  // 四 宣言側の敵陣三段目以内の駒は、玉を除いて10枚以上存在する。
  const int pieces_count = (_board.pos.bbOf(color) & opponents_field).popCount() - 1;

  if (pieces_count < 10) {
    return false;
  }

  // 三 宣言側が、大駒5点小駒1点で計算する
  // 点数の対象となるのは、宣言側の持駒と敵陣三段目以内に存在する玉を除く宣言側の駒のみである。
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

  // 入玉宣言に必要な点数を取得する
  int required_score = 0;

  if (color == cshogi::Black) {
    required_score = _nyugyokuScoreBlack;
  } else {
    required_score = _nyugyokuScoreWhite;
  }

  // 入玉宣言に必要な点数を満たしているか確認する
  return score >= required_score;
}

/**
 * 王手がかかっていればtrueを返す。
 * @return 王手がかかっていればtrue
 */
bool Board::isCheckmate() const {
  return _board.pos.inCheck();
}

/**
 * SFEN形式の文字列を取得する。
 * @return SFEN形式の文字列
 */
std::string Board::getSfen() const {
  return _board.toSFEN();
}

/**
 * ハフマン符号化された盤面情報を取得する。
 * @param data ハフマン符号化された盤面情報を格納するバッファ
 */
void Board::getPackedSfen(char* data) const {
  _board.toPackedSfen(data);
}

/**
 * モデルに入力するデータを取得する。
 * @param inputs モデルに入力するデータ
 */
void Board::getInputs(float* inputs) const {
  getInputs(inputs, getColor(), _drawSteps - getTurn());
}

/**
 * モデルに入力するデータを取得する。
 * @param inputs モデルに入力するデータ
 * @param color 手番
 * @param steps 手数
 */
void Board::getInputs(float* inputs, int32_t color, int32_t steps) const {
  // 0で初期化する
  std::fill_n(inputs, MODEL_INPUT_SIZE, 0);

  // モデルへの入力データを取得する
  float* board_inputs = inputs;
  float* info_inputs = inputs + MODEL_FEATURES * BOARD_SIZE * BOARD_SIZE;

  _getBoardInputs(board_inputs, color);
  _getInfoInputs(info_inputs, color, steps);
}

/**
 * 盤面の状態をコピーする。
 * @param board コピー元の盤面
 */
void Board::copyFrom(const Board* board) {
  _board = board->_board;
  _nyugyokuScoreBlack = board->_nyugyokuScoreBlack;
  _nyugyokuScoreWhite = board->_nyugyokuScoreWhite;
  _drawSteps = board->_drawSteps;
}

/**
 * 盤面情報を表示するための文字列を取得する。
 * @return 盤面情報を表示するための文字列
 */
std::string Board::dump() const {
  return _board.dump();
}

/**
 * 盤面の状態を出力する。
 * @param os 出力先
 */
void Board::print(std::ostream& os) const {
  os << dump() << std::endl;
}

/**
 * モデルに入力する盤面データを取得する。
 * @param inputs モデルに入力する盤面データ
 * @param color 手番
 */
void Board::_getBoardInputs(float* inputs, int32_t color) const {
  typedef float board_feats_t[MODEL_FEATURES][BOARD_SIZE * BOARD_SIZE];
  board_feats_t* const feats = reinterpret_cast<board_feats_t* const>(inputs);

  // 色を設定する
  cshogi::Color black_color = (color == COLOR_WHITE) ? cshogi::White : cshogi::Black;
  cshogi::Color white_color = (color == COLOR_WHITE) ? cshogi::Black : cshogi::White;

  // 駒の配置を取得する
  const cshogi::Bitboard occupied_bb = _board.pos.occupiedBB();

  // 駒の利き確認してビット演算で取り出せるようにする(駒種でマージ)
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

  // 駒の配置座標の一覧を取得する
  cshogi::Bitboard empty_bb = _board.pos.emptyBB();
  cshogi::Bitboard black_bb[cshogi::PieceTypeNum];
  cshogi::Bitboard white_bb[cshogi::PieceTypeNum];

  for (cshogi::PieceType pt = cshogi::Pawn; pt < cshogi::PieceTypeNum; ++pt) {
    black_bb[pt] = _board.pos.bbOf(pt, black_color);
    white_bb[pt] = _board.pos.bbOf(pt, white_color);
  }

  // 盤面の情報を設定する
  const size_t black_offset = 1;
  const size_t white_offset = black_offset + 2 * cshogi::PIECETYPE_NUM + 6;
  const size_t other_offset = white_offset + 2 * cshogi::PIECETYPE_NUM + 6;
  const int last_move = _board.get_last_move();

  for (cshogi::Square sq_dst = cshogi::SQ11; sq_dst < cshogi::SquareNum; ++sq_dst) {
    // 後手番の場合、盤面を180度回転する
    cshogi::Square sq_src = sq_dst;

    if (black_color == cshogi::White) {
      sq_src = cshogi::SQ99 - sq_dst;
    }

    // 空座標の値を設定する
    if (empty_bb.isSet(sq_src)) {
      (*feats)[0][sq_dst] = 1;
    }

    // 駒の配置と利きを設定する
    for (cshogi::PieceType pt = cshogi::Pawn; pt < cshogi::PieceTypeNum; ++pt) {
      const size_t pt_idx = pt - cshogi::Pawn;

      // 駒の配置の値を設定する
      if (black_bb[pt].isSet(sq_src)) {
        (*feats)[black_offset + pt_idx][sq_dst] = 1;
      }

      if (white_bb[pt].isSet(sq_src)) {
        (*feats)[white_offset + pt_idx][sq_dst] = 1;
      }

      // 駒の利きの値を設定する
      if (attacks[black_color][pt].isSet(sq_src)) {
        (*feats)[black_offset + cshogi::PIECETYPE_NUM + pt_idx][sq_dst] = 1;
      }

      if (attacks[white_color][pt].isSet(sq_src)) {
        (*feats)[white_offset + cshogi::PIECETYPE_NUM + pt_idx][sq_dst] = 1;
      }
    }

    // 駒の利きの数を設定する
    const int black_att_count = std::min(
        _board.pos.attackersTo(black_color, sq_src, occupied_bb).popCount(), 5);
    const int white_att_count = std::min(
        _board.pos.attackersTo(white_color, sq_src, occupied_bb).popCount(), 5);

    (*feats)[black_offset + cshogi::PIECETYPE_NUM * 2 + black_att_count][sq_dst] = 1;
    (*feats)[white_offset + cshogi::PIECETYPE_NUM * 2 + white_att_count][sq_dst] = 1;

    // 最後に移動した駒の利きを設定する
    if (last_move != 0 && (last_move & 0x7f) == sq_src) {
      (*feats)[other_offset][sq_dst] = 1;
    }

    // 行番号と列番号を設定する
    const size_t row_offset = other_offset + 1;
    const size_t row_index = sq_dst % BOARD_SIZE;
    const size_t col_offset = row_offset + BOARD_SIZE;
    const size_t col_index = sq_dst / BOARD_SIZE;

    (*feats)[row_offset + row_index][sq_dst] = 1;
    (*feats)[col_offset + std::min(col_index, BOARD_SIZE - 1 - col_index)][sq_dst] = 1;
  }
}

/**
 * モデルに入力するゲームデータを取得する。
 * @param inputs モデルに入力するゲームデータ
 * @param color 手番
 * @param steps 手数
 */
void Board::_getInfoInputs(float* inputs, int32_t color, int32_t steps) const {
  const static size_t hand_offsets[] = {0, 18, 22, 26, 30, 32, 34};
  const static size_t color_offset = 38;

  // 持ち駒の情報を設定する
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

  // 王手の情報を設定する
  if (_board.pos.inCheck()) {
    inputs[color_offset * 2 + 0] = 1;
  }

  // 入玉宣言に必要な点数を設定する
  if (color == COLOR_BLACK) {
    inputs[color_offset * 2 + 1] = (_nyugyokuScoreBlack - 27.5) / 5.0;
    inputs[color_offset * 2 + 2] = (_nyugyokuScoreWhite - 27.5) / 5.0;
  } else {
    inputs[color_offset * 2 + 1] = (_nyugyokuScoreWhite - 27.5) / 5.0;
    inputs[color_offset * 2 + 2] = (_nyugyokuScoreBlack - 27.5) / 5.0;
  }

  // 引き分けまでの残り手数を設定する
  float remaining_steps = 1.0 - (_drawSteps - steps) / 50.0;

  remaining_steps = std::min(std::max(remaining_steps, 0.0f), 1.0f);
  inputs[color_offset * 2 + 3] = remaining_steps;
}

}  // namespace deepshogi

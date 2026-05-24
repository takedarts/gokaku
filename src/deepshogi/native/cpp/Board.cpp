#include "Board.h"

#include <algorithm>
#include <map>
#include <sstream>

#include "Constant.h"

namespace deepshogi {

// SFEN形式の駒の番号と文字の対応表
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

// SFEN形式の駒の文字と番号の対応表
// 成り駒の場合は成る前の駒の番号に変換変換する（その後成りの処理を行う）
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

// SFEN形式の持ち駒の名前の配列
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

// SFEN形式の持ち駒の文字と{手番, 駒番号}の対応表
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

// SFEN形式の持ち駒の駒種の配列
// SFEN形式の表示順を判別するために使用する
static constexpr uint8_t SFEN_HAND_PIECES[] = {
    PIECE_HAND_ROOK, PIECE_HAND_BISHOP, PIECE_HAND_GOLD,
    PIECE_HAND_SILVER, PIECE_HAND_KNIGHT, PIECE_HAND_LANCE, PIECE_HAND_PAWN};

// 駒の種類に対応するビットボードのインデックス
static constexpr int8_t PAWN_INDEX = PIECE_BLACK_PAWN - PIECE_BLACK_BEGIN;
static constexpr int8_t LANCE_INDEX = PIECE_BLACK_LANCE - PIECE_BLACK_BEGIN;
static constexpr int8_t KNIGHT_INDEX = PIECE_BLACK_KNIGHT - PIECE_BLACK_BEGIN;
static constexpr int8_t SILVER_INDEX = PIECE_BLACK_SILVER - PIECE_BLACK_BEGIN;
static constexpr int8_t GOLD_INDEX = PIECE_BLACK_GOLD - PIECE_BLACK_BEGIN;
static constexpr int8_t KING_INDEX = PIECE_BLACK_KING - PIECE_BLACK_BEGIN;
static constexpr int8_t BISHOP_INDEX = PIECE_BLACK_BISHOP - PIECE_BLACK_BEGIN;
static constexpr int8_t ROOK_INDEX = PIECE_BLACK_ROOK - PIECE_BLACK_BEGIN;

// 持ち駒のビット表現のオフセット
static constexpr int8_t HAND_BIT_OFFSETS[PIECE_HAND_END - PIECE_HAND_BEGIN] = {
    0, 18, 22, 26, 30, 32, 34};

/**
 * 指定されたビットを立てる。
 * @param inputs ビット列
 * @param index 立てるビットの位置
 */
inline void setInputBit(int32_t* inputs, int32_t index, int32_t value = 1) {
  inputs[index / 32] |= (value << (index % 32));
}

/**
 * 指定された盤面から詰みの手があるかどうかを探索する。
 * 詰みまでの手順が逆順で格納された配列を返す。
 * 詰みの手が見つからなかった場合は空の配列を返す。
 * Boardオブジェクトの内容は探索中に変化することに注意すること。
 * @param board 盤面オブジェクト
 * @param depth 探索深さ
 * @return 詰み筋の手のリスト
 */
static std::vector<Move> searchCheckmateMoves(Board& board, int32_t depth) {
  // 深さが0以下なら詰みの手が見つからなかったので空の配列を返す
  if (depth <= 0) {
    return {};
  }

  // 引き分けのターン数を超えている場合は空の配列を返す
  if (board.getTurn() >= board.getDrawTurn()) {
    return {};
  }

  // 王手をかける手を試す
  std::vector<Move> shortest_moves;

  for (const Move& move : board.getLegalMoves(false, true)) {
    // 盤面を進めて、その結果を保存しておく
    MoveResult checkmate_result = board.play(move);

    // 王手から逃げる手がないかどうかを調べる
    std::vector<Move> escape_moves = board.getLegalMoves(false, false);

    // 王手から逃げる手がないなら詰みの手が見つかったので詰みの手を返す
    if (escape_moves.empty()) {
      board.undo(checkmate_result);
      return {move};
    }

    // 探索深さが1以下の場合は王手から逃げる手を試さない
    if (depth - 1 <= 0) {
      board.undo(checkmate_result);
      continue;
    }

    // 王手から逃げる手をすべて試して、最も深い詰みの手順を見つける
    bool checkmated = true;
    std::vector<Move> longest_moves;

    for (Move& escape_move : escape_moves) {
      // 次の盤面を作成する
      MoveResult escape_result = board.play(escape_move);

      // 再帰的に詰みの手を探索する
      std::vector<Move> moves = searchCheckmateMoves(board, depth - 2);

      // 詰みの手が見つからなかった場合は不詰みを決定してループを抜ける
      if (moves.empty()) {
        board.undo(escape_result);
        checkmated = false;
        break;
      }

      // 最も深い詰みの手順を保存する
      if (longest_moves.empty() || moves.size() > longest_moves.size() - 1) {
        longest_moves = moves;
        longest_moves.push_back(escape_move);
      }

      // 盤面をもとに戻す
      board.undo(escape_result);
    }

    // すべての王手から逃げる手が詰まされるなら詰みの手順が見つかったことになる
    // 最も浅い詰みの手順を保存する
    if (checkmated) {
      if (shortest_moves.empty() || longest_moves.size() < shortest_moves.size() - 1) {
        shortest_moves = longest_moves;
        shortest_moves.push_back(move);
      }
    }

    // 盤面をもとに戻す
    board.undo(checkmate_result);
  }

  // 最も浅い詰みの手順を返す
  return shortest_moves;
}

/**
 * 盤面オブジェクトを生成する。
 * 盤面への駒の配置は行わない。
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
 * 盤面オブジェクトを生成する。
 * 盤面への駒の配置は行わない。
 * @param nyugyokuScoreBlack 先手番の入玉宣言に必要な点数
 * @param nyugyokuScoreWhite 後手番の入玉宣言に必要な点数
 * @param drawTurn 引き分けとなるまでの手数
 */
Board::Board(int8_t nyugyokuScoreBlack, int8_t nyugyokuScoreWhite, int16_t drawTurn)
    : Board() {
  _nyugyokuScores[0] = nyugyokuScoreBlack;
  _nyugyokuScores[1] = nyugyokuScoreWhite;
  _drawTurn = drawTurn;
}

/**
 * SFEN形式の文字列で盤面を初期化する。
 * @param sfen SFEN形式の文字列
 */
void Board::initialize(const std::string& sfen) {
  // 初期化する
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

  // SFEN文字列の分割位置を判定する
  char* c_sfen = const_cast<char*>(sfen.c_str());
  int32_t board_sfen_length = 0;
  int32_t hand_sfen_length = 0;

  for (int32_t i = 0; c_sfen[i] != ' '; i++) {
    board_sfen_length++;
  }

  for (int32_t i = board_sfen_length + 3; c_sfen[i] != ' '; i++) {
    hand_sfen_length++;
  }

  // 盤面のSFEN情報を反映する
  int32_t pos_x = BOARD_SIZE - 1;
  int32_t pos_y = 0;
  bool promote = false;

  for (int32_t i = 0; i < board_sfen_length; i++) {
    char c = c_sfen[i];

    // 行の区切りの場合は次の行に移動する
    if (c == '/') {
      pos_x = BOARD_SIZE - 1;
      pos_y += 1;
      continue;
    }

    // 成りの記号の場合は成りフラグを立てる
    if (c == '+') {
      promote = true;
      continue;
    }

    // 盤面上の位置を確認する
    if (pos_x < 0 || pos_y >= BOARD_SIZE) {
      break;
    }

    // 数値の場合は空白マス分だけ次の位置に移動する
    if ('1' <= c && c <= '9') {
      pos_x -= c - '0';
      continue;
    }

    // 駒の記号の場合は駒を設定する
    Position pos(pos_x, pos_y);
    uint8_t piece = SFEN_PIECE_TYPES.at(c);

    // 成りの処理を行う
    if (promote) {
      piece += PIECE_PROMOTE;
    }

    _putPiece(pos, piece);

    // 次の位置に移動する
    promote = false;
    pos_x -= 1;
  }

  // 持ち駒のSFEN情報を反映する
  int32_t hand_piece_num = 0;

  for (int32_t i = 0; i < hand_sfen_length; i++) {
    char c = c_sfen[board_sfen_length + 3 + i];

    // 数値の場合は駒の数を設定する
    if ('0' <= c && c <= '9') {
      hand_piece_num = hand_piece_num * 10 + (c - '0');
      continue;
    }

    // 駒の記号の場合は持ち駒を設定する
    // 駒の数が指定されていない場合は1とみなす
    if (SFEN_HAND_PIECE_TYPES.count(c) > 0) {
      auto [color, piece] = SFEN_HAND_PIECE_TYPES.at(c);

      if (hand_piece_num == 0) {
        hand_piece_num = 1;
      }

      _addHand(color, piece, hand_piece_num);
    }

    // 数値以外の場合は駒の数を0にリセットする
    hand_piece_num = 0;
  }

  // 手番を設定する
  if (c_sfen[board_sfen_length + 1] == 'b') {
    _color = COLOR_BLACK;
  } else if (c_sfen[board_sfen_length + 1] == 'w') {
    _color = COLOR_WHITE;
  }

  // ターン数を設定する
  _turn = static_cast<int16_t>(
      std::stoi(sfen.substr(board_sfen_length + 3 + hand_sfen_length + 1)) - 1);
}

/**
 * 駒を動かす。
 * @param move 着手
 * @return 着手の結果
 */
MoveResult Board::play(const Move& move) {
  Position src = move.getSrc();
  Position dst = move.getDst();
  uint8_t captured_piece = PIECE_EMPTY;

  // 持ち駒から駒を打つ場合
  if (src.getX() == BOARD_SIZE) {
    // 持ち駒を減らす
    uint8_t hand_piece = src.getY();

    _removeHand(_color, hand_piece);

    // 駒を配置する
    uint8_t dst_piece =
        (_color == COLOR_BLACK)
            ? (hand_piece - PIECE_HAND_BEGIN + PIECE_BLACK_BEGIN)
            : (hand_piece - PIECE_HAND_BEGIN + PIECE_WHITE_BEGIN);

    _putPiece(dst, dst_piece);
  }
  // 盤面上の駒を移動する場合
  else {
    // 移動先に駒がある場合は駒を取る
    captured_piece = _cells[dst.getIndex()];

    if (captured_piece != PIECE_EMPTY) {
      int32_t hand_piece =
          (captured_piece < PIECE_WHITE_BEGIN)
              ? (captured_piece - PIECE_BLACK_BEGIN + PIECE_HAND_BEGIN)
              : (captured_piece - PIECE_WHITE_BEGIN + PIECE_HAND_BEGIN);

      // 鳴っている駒の場合は、成り前の駒の種類に変換する
      if (hand_piece >= PIECE_HAND_END) {
        hand_piece -= PIECE_PROMOTE;
      }

      // 移動先の駒を取り除く
      _removePiece(dst);

      // 取った駒を持ち駒として追加する
      _addHand(_color, hand_piece);
    }

    // 移動元の駒を取得する
    uint8_t src_piece = _cells[src.getIndex()];

    // 成りの処理
    if (move.isPromote()) {
      src_piece += PIECE_PROMOTE;
    }

    // 駒を移動する
    _removePiece(src);
    _putPiece(dst, src_piece);
  }

  // 手番を変更する
  _color = (_color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;

  // ターン数を増やす
  _turn += 1;

  // 着手を保存する
  _lastMove = move;

  // 着手の結果を返す
  return MoveResult(move, captured_piece);
}

/**
 * 指定された着手の内容を盤面から取り消す。
 * この関数は指定された着手が直前の着手であることを前提としている。
 * @param result 取り消す着手の結果
 */
void Board::undo(const MoveResult& result) {
  Move move = result.getMove();
  Position src = move.getSrc();
  Position dst = move.getDst();
  uint8_t captured_piece = result.getCaptured();

  // 手番を変更する
  _color = (_color == COLOR_BLACK) ? COLOR_WHITE : COLOR_BLACK;

  // ターン数を減らす
  _turn -= 1;

  // 保存している着手をリセットする
  _lastMove = MOVE_INVALID;

  // 駒を元の位置に戻す
  if (src.getX() == BOARD_SIZE) {
    // 持ち駒から駒を打つ場合は、駒を取り除いて持ち駒を増やす
    _removePiece(dst);
    _addHand(_color, src.getY());
  } else {
    // 盤面上の駒を移動する場合は、駒を移動して取った駒がある場合は元に戻す
    uint8_t dst_piece = _cells[dst.getIndex()];

    if (move.isPromote()) {
      dst_piece -= PIECE_PROMOTE;
    }

    _removePiece(dst);
    _putPiece(src, dst_piece);

    // 取った駒がある場合は元に戻す
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
 * 指定された座標に効きがある駒の位置を取得する。
 * @param position 確認する座標
 * @return 指定された座標に効きがある駒の位置のリスト
 */
std::vector<Position> Board::getAttackers(const Position& position) const {
  // 最大10個の駒が利いている可能性があるため、あらかじめサイズを確保しておく
  std::vector<Position> attackers;
  attackers.reserve(10);

  // 指定された座標に効きがある駒の位置を取得する
  for (int8_t color : {COLOR_BLACK, COLOR_WHITE}) {
    for (int8_t attacker : _getAttackers<false, false>(color, position.getIndex())) {
      attackers.emplace_back(attacker);
    }
  }

  return attackers;
}

/**
 * 現在の盤面の合法手の一覧を取得する。
 * @param removeUnpromote 歩、角、飛車、2行目の香の不成の手を削除する場合はtrue
 * @param checkOnly 王手が発生する手のみを取得する場合はtrue
 * @return 合法手の一覧
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
 * 現在の盤面の詰み筋の着手手順を取得する。
 * @param depth 詰み探索の深さ
 * @return 詰み筋の着手手順
 */
std::vector<Move> Board::getCheckmateMoves(int32_t depth) const {
  // 計算用の盤面の配列を作成する
  // 探索中はBoardオブジェクトの内容が変化するため探索用の盤面を用意する
  Board clone_board(*this);

  // 詰み探索を実行する
  std::vector<Move> moves = searchCheckmateMoves(clone_board, depth);

  // 詰み筋の着手手順を返す（逆順で格納されているので順番を入れ替える）
  std::reverse(moves.begin(), moves.end());

  return moves;
}

/**
 * 入玉宣言可能な状態であればtrueを返す。
 * @param color 宣言側の手番
 * @return 入玉宣言可能な状態であればtrue
 */
bool Board::isNyugyoku(int8_t color) const {
  int8_t my_color_idx = (color == COLOR_BLACK) ? 0 : 1;

  // [条件1] 宣言側の手番である
  // [条件6] 宣言側の持ち時間が残っている

  // [条件5] 宣言側の玉に王手がかかっていない
  if (isCheck(color)) {
    return false;
  }

  // [条件2] 宣言側の玉が敵陣三段目以内に入っている
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

  if (king_pos_idx < 0 ||
      !BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(king_pos_idx)) {
    return false;
  }

  // [条件4] 宣言側の敵陣三段目以内の駒は、玉を除いて10枚以上存在する
  // 王が敵陣に入っているため、合計11枚以上存在する必要がある
  BitBoard invasion_bitboard =
      _colorBitBoards[my_color_idx] & BITBOARD_ENEMY_AREAS[my_color_idx];

  if (invasion_bitboard.countBit() < 11) {
    return false;
  }

  // [条件3] 宣言側が、大駒5点小駒1点で計算する
  // 点数の対象となるのは、宣言側の持駒と敵陣に存在する玉を除く宣言側の駒のみである。
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

  // 入玉宣言に必要な点数を満たしているか確認する
  return nyugyoku_score >= _nyugyokuScores[my_color_idx];
}

/**
 * 王手がかかっていればtrueを返す。
 * @param color 王手をかけられている側の色
 * @return 王手がかかっていればtrue
 */
bool Board::isCheck(int8_t color) const {
  int8_t color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int8_t king_pos_idx = _kingPositions[color_idx].getIndex();

  // 王が盤面上に存在しない場合は王手をかけられない
  if (king_pos_idx < 0) {
    return false;
  }
  // 王が存在しているなら王手がかかっているかどうかを確認する
  else {
    return !_getAttackers<true, false>(color, king_pos_idx).empty();
  }
}

/**
 * SFEN形式の文字列を取得する。
 * @return SFEN形式の文字列
 */
std::string Board::getSfen() const {
  std::stringstream ss;

  // 盤面情報を設定する
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

  // 手番を設定する
  if (_color == COLOR_BLACK) {
    ss << " b ";
  } else {
    ss << " w ";
  }

  // 持ち駒情報を設定する
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

  // ターン数を設定する
  ss << " " << (_turn + 1);

  return ss.str();
}

/**
 * モデルに入力するデータを取得する。
 * @param inputs モデルに入力するデータ
 */
void Board::getInputs(int32_t* inputs) const {
  getInputs(inputs, _color);
}

/**
 * モデルに入力するデータを取得する。
 * @param inputs モデルに入力するデータ
 * @param color 手番
 */
void Board::getInputs(int32_t* inputs, int8_t color) const {
  // 初期化する
  std::fill_n(inputs, MODEL_INPUT_PACK_SIZE, 0);

  // モデルへの入力データを取得する
  _getBoardInputs(inputs, color);
  _getInfoInputs(inputs, color);
}

/**
 * 盤面の状態をコピーする。
 * @param board コピー元の盤面
 */
void Board::copyFrom(const Board* board) {
  *this = *board;
}

/**
 * 盤面情報を表示するための文字列を取得する。
 * @return 盤面情報を表示するための文字列
 */
std::string Board::toString() const {
  // 駒情報を表示するためのCSA形式の文字列。
  const char piece_names[][3] = {
      "FU", "KY", "KE", "GI", "KA", "HI", "KI", "OU",
      "TO", "NY", "NK", "NG", "UM", "RY"};
  const char hand_color_names[][3] = {"P+", "P-"};

  // 文字列を作成するためのストリームオブジェクト
  std::stringstream ss;

  // 手番とターン数を作成する
  ss << "Color: " << ((_color == COLOR_WHITE) ? "white" : "black");
  ss << ", Turn: " << _turn << std::endl;

  // 盤面の駒情報を作成する
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

  // 持ち駒の情報を作成する
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
 * 指定された位置に駒を配置する。
 * このメソッドは指定座標に駒がない状態で呼び出されることを想定している。
 * @param pos 駒を配置する座標
 * @param piece 配置する駒を表す整数値
 */
void Board::_putPiece(const Position& pos, uint8_t piece) {
  int8_t pos_idx = pos.getIndex();

  // 指定された位置にすでに駒がある場合は例外を発生させる
  if (_cells[pos_idx] != PIECE_EMPTY) {
    throw std::invalid_argument("Position already has a piece");
  }

  // ハッシュ値を更新する
  _cellHash ^= BOARD_HASH_VALUES[pos_idx][piece];

  // 指定された位置に駒を配置する
  _cells[pos_idx] = piece;

  // 駒の種類に応じてビットボードを更新する
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

  // 王の位置を更新する
  if (piece == PIECE_BLACK_KING) {
    _kingPositions[0] = pos;
  } else if (piece == PIECE_WHITE_KING) {
    _kingPositions[1] = pos;
  }
}

/**
 * 指定された位置から駒を取り除く。
 * @param pos 駒を取り除く座標
 */
void Board::_removePiece(const Position& pos) {
  int8_t pos_idx = pos.getIndex();
  uint8_t piece = _cells[pos_idx];

  // 指定された位置に駒がない場合は例外を発生させる
  if (piece == PIECE_EMPTY) {
    throw std::invalid_argument("Position does not have a piece");
  }

  // ハッシュ値を更新する
  _cellHash ^= BOARD_HASH_VALUES[pos_idx][piece];

  // 指定された位置から駒を取り除く
  _cells[pos_idx] = PIECE_EMPTY;

  // 駒の種類に応じてビットボードを更新する
  // と金、成香、成桂、成銀は金と同じビットボードを使用する
  // 馬は角と王のビットボードを使用する
  // 龍は飛と王のビットボードを使用する
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

  // 王の位置を更新する
  if (piece == PIECE_BLACK_KING) {
    _kingPositions[0] = POSITION_INVALID;
  } else if (piece == PIECE_WHITE_KING) {
    _kingPositions[1] = POSITION_INVALID;
  }
}

/**
 * 指定された駒を持ち駒として追加する。
 * @param color 手番（COLOR_BLACK または COLOR_WHITE）
 * @param piece 追加する駒を表す整数値
 * @param num 追加する駒の数
 */
void Board::_addHand(int8_t color, uint8_t piece, int32_t num) {
  int32_t color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int32_t piece_idx = piece - PIECE_HAND_BEGIN;

  // 持ち駒のビット表現を更新する
  int8_t offset = HAND_BIT_OFFSETS[piece_idx] + _hands[color_idx][piece_idx];

  for (int8_t i = 0; i < num; i++) {
    _handBits[color_idx] |= (1ULL << (offset + i));
  }

  // 指定された駒を持ち駒として追加する
  _hands[color_idx][piece_idx] += num;
}

/**
 * 指定された駒を持ち駒として追加する。
 * @param color 手番（COLOR_BLACK または COLOR_WHITE）
 * @param piece 追加する駒を表す整数値
 */
void Board::_addHand(int8_t color, uint8_t piece) {
  _addHand(color, piece, 1);
}

/**
 * 指定された駒を持ち駒から取り除く。
 * この関数は指定された駒が持ち駒に存在することを前提としている。
 * @param color 手番（COLOR_BLACK または COLOR_WHITE）
 * @param piece 取り除く駒を表す整数値
 */
void Board::_removeHand(int8_t color, uint8_t piece) {
  int32_t color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int32_t piece_idx = piece - PIECE_HAND_BEGIN;

  // 指定された駒が持ち駒に存在しない場合は例外を発生させる
  if (_hands[color_idx][piece_idx] <= 0) {
    throw std::invalid_argument("Hand does not have the specified piece");
  }

  // 持ち駒のビット表現を更新する
  int8_t offset = HAND_BIT_OFFSETS[piece_idx] + _hands[color_idx][piece_idx];

  _handBits[color_idx] &= ~(1ULL << (offset - 1));

  // 指定された駒を持ち駒から取り除く
  _hands[color_idx][piece_idx] -= 1;
}

/**
 * 指定された座標に効きをかけている駒の座標の一覧を返す。
 * template引数returnOnFirstAttackerがtrueの場合は、
 * 効きをかけている駒の座標のうち最初に見つかった1つのみを返す。
 * additionalOccIndexで指定された場合は、その座標に動けない駒があるとして飛び駒の効きを計算する。
 * template引数removeOwnKingがtrueの場合は、飛び駒の効きを計算するときに自分の王の座標を削除する。
 * @param color 効きをかけられている側の色
 * @param posIndex 効きをかけている駒の存在を確認する座標
 * @param additionalOccIndex 追加で駒があると仮定する座標（追加しない場合は-1を指定）
 * @return 指定された座標に効きをかけている駒の座標の一覧
 */
template <bool returnOnFirstAttacker, bool removeOwnKing>
std::vector<int8_t> Board::_getAttackers(
    int8_t color, int8_t posIndex, int8_t additionalOccIndex) const {
  // 効きをかけられている側の色と効きをかけている側の色の配列番号を計算する
  int8_t my_color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;

  // 効きをかけている駒の座標を格納するオブジェクト
  // 最大で10個の座標が登録されるため、あらかじめ10個分の要素数を確保しておく
  std::vector<int8_t> attacker_indices;

  if constexpr (returnOnFirstAttacker) {
    attacker_indices.reserve(1);
  } else {
    attacker_indices.reserve(10);
  }

  // 歩の効きを確認する
  BitBoard pawn_bitboard =
      BITBOARD_PAWN_ATTACKS[my_color_idx][posIndex] &
      _pieceBitBoards[op_color_idx][PAWN_INDEX];

  while (pawn_bitboard) {
    attacker_indices.push_back(pawn_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // 桂の効きを確認する
  BitBoard knight_bitboard =
      BITBOARD_KNIGHT_ATTACKS[my_color_idx][posIndex] &
      _pieceBitBoards[op_color_idx][KNIGHT_INDEX];

  while (knight_bitboard) {
    attacker_indices.push_back(knight_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // 銀の効きを確認する
  BitBoard silver_bitboard =
      BITBOARD_SILVER_ATTACKS[my_color_idx][posIndex] &
      _pieceBitBoards[op_color_idx][SILVER_INDEX];

  while (silver_bitboard) {
    attacker_indices.push_back(silver_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // 金の効きを確認する
  BitBoard gold_bitboard =
      BITBOARD_GOLD_ATTACKS[my_color_idx][posIndex] &
      _pieceBitBoards[op_color_idx][GOLD_INDEX];

  while (gold_bitboard) {
    attacker_indices.push_back(gold_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // 王の効きを確認する
  // 王の効きと角・飛の効きが重複することがあるため、
  // ここで見つけた座標を除外するマスクを作成する
  BitBoard king_bitboard =
      BITBOARD_KING_ATTACKS[posIndex] & _pieceBitBoards[op_color_idx][KING_INDEX];
  BitBoard horse_dragon_bitboard = king_bitboard;

  while (king_bitboard) {
    attacker_indices.push_back(king_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // 香・角・飛の効きをとなる位置を表すビットボードを作成する
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
  BitBoard lance_attack_bitboard;
  BitBoard bishop_attack_bitboard;
  BitBoard rook_attack_bitboard;

  // removeOwnKingがtrueの場合は、自分の王の位置を占有ビットボードから削除する
  if constexpr (removeOwnKing) {
    int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

    if (king_pos_idx >= 0) {
      occ_bitboard.clearBit(king_pos_idx);
    }
  }

  // additionalOccIndexが-1以外の場合は、additionalOccIndexの位置に駒があると仮定する
  if (additionalOccIndex >= 0) {
    occ_bitboard.setBit(additionalOccIndex);
  }

  // 上方向の効きを確認する
  int8_t up_index =
      (BITBOARD_LONG_ATTACKS[0][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (up_index >= 0 && !horse_dragon_bitboard.hasBit(up_index)) {
    if (color == COLOR_BLACK) {
      lance_attack_bitboard.setBit(up_index);
    }

    rook_attack_bitboard.setBit(up_index);
  }

  //  右上方向の効きを確認する
  int8_t up_right_index =
      (BITBOARD_LONG_ATTACKS[1][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (up_right_index >= 0 && !horse_dragon_bitboard.hasBit(up_right_index)) {
    bishop_attack_bitboard.setBit(up_right_index);
  }

  // 右方向の効きを確認する
  int8_t right_index =
      (BITBOARD_LONG_ATTACKS[2][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (right_index >= 0 && !horse_dragon_bitboard.hasBit(right_index)) {
    rook_attack_bitboard.setBit(right_index);
  }

  // 右下方向の効きを確認する
  int8_t down_right_index =
      (BITBOARD_LONG_ATTACKS[3][posIndex] & occ_bitboard).getLeftmostBitIndex();

  if (down_right_index >= 0 && !horse_dragon_bitboard.hasBit(down_right_index)) {
    bishop_attack_bitboard.setBit(down_right_index);
  }

  // 下方向の効きを確認する
  int8_t down_index =
      (BITBOARD_LONG_ATTACKS[4][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (down_index >= 0 && !horse_dragon_bitboard.hasBit(down_index)) {
    if (color == COLOR_WHITE) {
      lance_attack_bitboard.setBit(down_index);
    }

    rook_attack_bitboard.setBit(down_index);
  }

  // 左下方向の効きを確認する
  int8_t down_left_index =
      (BITBOARD_LONG_ATTACKS[5][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (down_left_index >= 0 && !horse_dragon_bitboard.hasBit(down_left_index)) {
    bishop_attack_bitboard.setBit(down_left_index);
  }

  // 左方向の効きを確認する
  int8_t left_index =
      (BITBOARD_LONG_ATTACKS[6][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (left_index >= 0 && !horse_dragon_bitboard.hasBit(left_index)) {
    rook_attack_bitboard.setBit(left_index);
  }

  // 左上方向の効きを確認する
  int8_t up_left_index =
      (BITBOARD_LONG_ATTACKS[7][posIndex] & occ_bitboard).getRightmostBitIndex();

  if (up_left_index >= 0 && !horse_dragon_bitboard.hasBit(up_left_index)) {
    bishop_attack_bitboard.setBit(up_left_index);
  }

  // 香の効きがあるならその座標を登録する
  BitBoard lance_bitboard =
      lance_attack_bitboard & _pieceBitBoards[op_color_idx][LANCE_INDEX];

  if (lance_bitboard) {
    attacker_indices.push_back(lance_bitboard.getRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // 角の効きがあるならその座標を登録する
  BitBoard bishop_bitboard =
      bishop_attack_bitboard & _pieceBitBoards[op_color_idx][BISHOP_INDEX];

  while (bishop_bitboard) {
    attacker_indices.push_back(bishop_bitboard.popRightmostBitIndex());

    if constexpr (returnOnFirstAttacker) {
      return attacker_indices;
    }
  }

  // 飛の効きがあるならその座標を登録する
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
 * 現在の盤面の合法手の一覧を取得する。
 * template引数removeUnpromoteがtrueの場合は、歩、角、飛車、2行目の香の不成の手を削除する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 合法手の一覧を追加する配列オブジェクト
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalMoves(std::vector<Move>& legalMoves) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;

  // 王手の状況を確認する
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();
  std::vector<int8_t> checking_piece_indices;

  if (king_pos_idx >= 0) {
    checking_piece_indices = _getAttackers<false, false>(_color, king_pos_idx);
  }

  // 0個の駒から王手をかけられている場合は、すべての合法手が有効
  // 1個の駒から王手をかけられている場合は、その駒を取る手、王の移動、駒の間に駒を打つ手が合法手になる
  // 2個以上の駒から王手をかけられている場合は、王の移動のみが合法手になる
  BitBoard destination_bitboard;  // 最初は0で初期化されている

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

  // 盤面上の駒を移動する手を作成する
  // 馬と龍の1マス移動は王の移動と馬・龍の移動で重複することがあるため、
  // 最初に王・馬・龍の合法手生成を行って重複する手を除外する
  _getLegalKingMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalBishopMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);
  _getLegalRookMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);

  // 重複している着手を削除する
  std::sort(legalMoves.begin(), legalMoves.end());
  legalMoves.erase(std::unique(legalMoves.begin(), legalMoves.end()), legalMoves.end());

  // 歩・香・桂・銀・金の合法手生成を行う
  _getLegalPawnMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);
  _getLegalLanceMoves<removeUnpromote, checkOnly>(legalMoves, destination_bitboard);
  _getLegalKnightMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalSilverMoves<checkOnly>(legalMoves, destination_bitboard);
  _getLegalGoldMoves<checkOnly>(legalMoves, destination_bitboard);

  // 歩・香・桂・銀・金・角・飛を打つ合法手生成を行う
  // 持ち駒を打つ合法手生成なので、相手の駒を取る手を除外する
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
 * 歩の移動の合法手を作成する。
 * template引数removeUnpromoteがtrueの場合は、不成の手を削除する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 歩の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 歩の移動の目的地として有効な座標を表すビットボード
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalPawnMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 歩のビットボードを作成する
  BitBoard pawn_bitboard = _pieceBitBoards[my_color_idx][PAWN_INDEX];

  // それぞれの歩について移動の合法手を作成する
  while (pawn_bitboard) {
    int8_t src_idx = pawn_bitboard.popRightmostBitIndex();
    BitBoard move_bitboard =
        BITBOARD_PAWN_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // 移動先がない場合は次の歩について確認する
    if (!move_bitboard) {
      continue;
    }

    // 移動先の座標を取得する
    int8_t dst_idx = move_bitboard.getRightmostBitIndex();

    // 移動が可能かどうかを確認する（空き王手でないかどうかを確認する）
    if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
      continue;
    }

    // 成れるかどうかを確認する
    bool promote = BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

    // 王手のみに限定する場合は王手の場合にのみ合法手に登録する
    // そうでない場合は無条件に合法手に登録する
    if constexpr (checkOnly) {
      bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                             : _isCheckMove<PIECE_BLACK_PAWN>(src_idx, dst_idx);
      if (check) {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }
    } else {
      legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
    }

    // 成りの手を登録し、removeUnpromoteがfalseの場合は不成の手も登録する
    // ただし、1行目に移動した場合は成るしかないため、不成の手は登録しない
    if constexpr (!removeUnpromote) {
      if (promote && BITBOARD_PAWN_DROPABLES[my_color_idx].hasBit(dst_idx)) {
        // 王手のみに限定する場合は王手の場合にのみ合法手に登録する
        // そうでない場合は無条件に合法手に登録する
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
 * 香の移動の合法手を作成する。
 * template引数removeUnpromoteがtrueの場合は、2行目の不成の手を削除する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 香の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 香の移動の目的地として有効な座標を表すビットボード
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalLanceMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 香のビットボードを作成する
  BitBoard lance_bitboard = _pieceBitBoards[my_color_idx][LANCE_INDEX];

  // それぞれの香について移動の合法手を作成する
  BitBoard occupied_bitboard = _colorBitBoards[0] | _colorBitBoards[1];

  while (lance_bitboard) {
    int8_t src_idx = lance_bitboard.popRightmostBitIndex();

    // 香の移動先のビットボードを作成する
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

    // それぞれの移動先について合法手を作成する
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // 移動が可能かどうかを確認する（空き王手でないかどうかを確認する）
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // 成れるかどうかを確認する
      bool promote = BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

      // 王手のみに限定する場合は王手の場合にのみ合法手に登録する
      // そうでない場合は無条件に合法手に登録する
      if constexpr (checkOnly) {
        bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                               : _isCheckMove<PIECE_BLACK_LANCE>(src_idx, dst_idx);
        if (check) {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
        }
      } else {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }

      // removeUnpromoteがfalseの場合は2行目・3行目の不成の手も登録する
      // removeUnpromoteがtrueの場合は3行目の不成の手のみを登録する
      if constexpr (removeUnpromote) {
        // 3列目の判定は桂馬を打てるかどうかの判定と同じビットボードを使用する
        if (promote && BITBOARD_KNIGHT_DROPABLES[my_color_idx].hasBit(dst_idx)) {
          // 王手のみに限定する場合は王手の場合にのみ合法手に登録する
          // そうでない場合は無条件に合法手に登録する
          if constexpr (checkOnly) {
            if (_isCheckMove<PIECE_BLACK_LANCE>(src_idx, dst_idx)) {
              legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
            }
          } else {
            legalMoves.emplace_back(Position(src_idx), Position(dst_idx), false);
          }
        }
      } else {
        // 2行目の判定は歩を打てるかどうかの判定と同じビットボードを使用する
        if (promote && BITBOARD_PAWN_DROPABLES[my_color_idx].hasBit(dst_idx)) {
          // 王手のみに限定する場合は王手の場合にのみ合法手に登録する
          // そうでない場合は無条件に合法手に登録する
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
 * 桂の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 桂の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 桂の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalKnightMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 桂のビットボードを作成する
  BitBoard knight_bitboard = _pieceBitBoards[my_color_idx][KNIGHT_INDEX];

  // それぞれの桂について移動の合法手を作成する
  while (knight_bitboard) {
    int8_t src_idx = knight_bitboard.popRightmostBitIndex();
    BitBoard move_bitboard =
        BITBOARD_KNIGHT_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // それぞれの移動先について合法手を作成する
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // 移動が可能かどうかを確認する（空き王手でないかどうかを確認する）
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // 成れるかどうかを確認する
      bool promote = BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

      // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
      // そうでない場合は無条件に合法手に登録する
      if constexpr (checkOnly) {
        bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                               : _isCheckMove<PIECE_BLACK_KNIGHT>(src_idx, dst_idx);
        if (check) {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
        }
      } else {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }

      // 成りの手を登録した場合は不成の手も登録する
      if (promote && BITBOARD_KNIGHT_DROPABLES[my_color_idx].hasBit(dst_idx)) {
        // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
        // そうでない場合は無条件に合法手に登録する
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
 * 銀の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 銀の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 銀の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalSilverMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 銀のビットボードを作成する
  BitBoard silver_bitboard = _pieceBitBoards[my_color_idx][SILVER_INDEX];

  // それぞれの銀について移動の合法手を作成する
  while (silver_bitboard) {
    int8_t src_idx = silver_bitboard.popRightmostBitIndex();

    // 銀の移動先のビットボードを作成する
    BitBoard move_bitboard =
        BITBOARD_SILVER_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // それぞれの移動先について合法手を作成する
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // 移動が可能かどうかを確認する（空き王手でないかどうかを確認する）
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // 成れるかどうかを確認する
      bool promote =
          BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(src_idx) ||
          BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx);

      // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
      // そうでない場合は無条件に合法手に登録する
      if constexpr (checkOnly) {
        bool check = (promote) ? _isCheckMove<PIECE_BLACK_GOLD>(src_idx, dst_idx)
                               : _isCheckMove<PIECE_BLACK_SILVER>(src_idx, dst_idx);
        if (check) {
          legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
        }
      } else {
        legalMoves.emplace_back(Position(src_idx), Position(dst_idx), promote);
      }

      // 成りの手を登録した場合は不成の手も登録する
      if (promote) {
        // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
        // そうでない場合は無条件に合法手に登録する
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
 * 金の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 金の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 金の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalGoldMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 金のビットボードを作成する
  BitBoard gold_bitboard = _pieceBitBoards[my_color_idx][GOLD_INDEX];

  // それぞれの金について移動の合法手を作成する
  while (gold_bitboard) {
    int8_t src_idx = gold_bitboard.popRightmostBitIndex();

    // 金の移動先のビットボードを作成する
    BitBoard move_bitboard =
        BITBOARD_GOLD_ATTACKS[my_color_idx][src_idx] & destinationBitBoard;

    // それぞれの移動先について合法手を作成する
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // 移動が可能かどうかを確認する（空き王手でないかどうかを確認する）
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
      // そうでない場合は無条件に合法手に登録する
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
 * 王の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 王の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 王の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalKingMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

  // 王の合法手を作成する
  // 王の位置が設定されていない場合は王の合法手を作成しない
  // 王の移動先はdestinationBitBoardの影響を受けない
  // 王の移動先は自分の駒がない、かつ、相手の駒からの効きがない座標に限られる
  // 王が王手をすることはないので、王手のみを対象とする場合は空き王手のみを合法手に登録する
  if (king_pos_idx >= 0) {
    BitBoard king_move_bitboard =
        BITBOARD_KING_ATTACKS[king_pos_idx] & ~_colorBitBoards[my_color_idx];

    // それぞれの移動先について合法手を作成する
    while (king_move_bitboard) {
      int8_t dst_idx = king_move_bitboard.popRightmostBitIndex();

      // 自殺手になる場合は合法手に登録しない
      if (!_getAttackers<true, true>(_color, dst_idx).empty()) {
        continue;
      }

      // 王手のみに限定する場合は空き王手になる場合のみ合法手に登録する
      // そうでない場合は無条件に合法手に登録する
      if constexpr (checkOnly) {
        if (_isDiscoveredCheckMove(king_pos_idx, dst_idx, OPPOSITE_COLOR(_color))) {
          legalMoves.emplace_back(Position(king_pos_idx), Position(dst_idx), false);
        }
      } else {
        legalMoves.emplace_back(Position(king_pos_idx), Position(dst_idx), false);
      }
    }
  }

  // 馬・龍のビットボードを作成する
  BitBoard bishop_bitboard = _pieceBitBoards[my_color_idx][BISHOP_INDEX];
  BitBoard other_bitboard = _pieceBitBoards[my_color_idx][KING_INDEX];

  if (king_pos_idx >= 0) {
    other_bitboard.clearBit(king_pos_idx);
  }

  // それぞれの駒について移動の合法手を作成する
  while (other_bitboard) {
    int8_t src_idx = other_bitboard.popRightmostBitIndex();
    BitBoard move_bitboard = BITBOARD_KING_ATTACKS[src_idx] & destinationBitBoard;

    // それぞれの移動先について合法手を作成する
    while (move_bitboard) {
      int8_t dst_idx = move_bitboard.popRightmostBitIndex();

      // 移動が可能かどうかを確認する（空き王手でないかどうかを確認する）
      if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
        continue;
      }

      // 王手のみに限定する場合は王手の着手のみを合法手に登録する
      // そうでない場合は無条件に合法手に登録する
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
 * 角の移動の合法手を作成する。
 * template引数removeUnpromoteがtrueの場合は、不成の手を削除する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 角の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 角の移動の目的地として有効な座標を表すビットボード
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalBishopMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 角の合法手を作成する
  BitBoard bishop_bitboard = _pieceBitBoards[my_color_idx][BISHOP_INDEX];

  // それぞれの座標について移動の合法手を作成する
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];

  while (bishop_bitboard) {
    int8_t src_idx = bishop_bitboard.popRightmostBitIndex();

    // 馬かどうかを確認する（王のビットボードの座標が1になっている場合は馬に成っている）
    bool already_promoted =
        _pieceBitBoards[my_color_idx][KING_INDEX].hasBit(src_idx);

    // 4方向の移動を確認する
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

      // それぞれの移動先について合法手を作成する
      while (move_bitboard) {
        int8_t dst_idx = move_bitboard.popRightmostBitIndex();

        // 移動が可能かどうかを確認する（空き王手でないかどうかを確認する）
        if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
          continue;
        }

        // 成れるかどうかを確認する
        bool promote = !already_promoted &&
                       (BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(src_idx) ||
                        BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx));

        // 王手のみに限定する場合は王手となる場合にのみ合法手に登録する
        // そうでない場合は無条件に合法手に登録する
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

        // 不成の手を削除する場合は、成らない手をさらに確認する
        if constexpr (!removeUnpromote) {
          // 成らない手を追加する
          if (promote) {
            // 王手に限定する場合は王手となる場合にのみ合法手に登録する
            // そうでない場合は無条件に合法手に登録する
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
 * 飛の移動の合法手を作成する。
 * template引数removeUnpromoteがtrueの場合は、不成の手を削除する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 飛の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 飛の移動の目的地として有効な座標を表すビットボード
 */
template <bool removeUnpromote, bool checkOnly>
void Board::_getLegalRookMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 飛の合法手を作成する
  BitBoard rook_bitboard = _pieceBitBoards[my_color_idx][ROOK_INDEX];

  // それぞれの座標について移動の合法手を作成する
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];

  while (rook_bitboard) {
    int8_t src_idx = rook_bitboard.popRightmostBitIndex();

    // 龍かどうかを確認する（王のビットボードの座標が1になっている場合は龍に成っている）
    bool already_promoted =
        _pieceBitBoards[my_color_idx][KING_INDEX].hasBit(src_idx);

    // 4方向の移動を確認する
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

      // それぞれの移動先について合法手を作成する
      while (move_bitboard) {
        int8_t dst_idx = move_bitboard.popRightmostBitIndex();

        // 移動が可能かどうかを確認する（空き王手でないかどうかを確認する）
        if (_isDiscoveredCheckMove(src_idx, dst_idx, _color)) {
          continue;
        }

        // 成れるかどうかを確認する
        bool promote = !already_promoted &&
                       (BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(src_idx) ||
                        BITBOARD_ENEMY_AREAS[my_color_idx].hasBit(dst_idx));

        // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
        // そうでない場合は無条件に合法手に登録する
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

        // 不成の手を削除する場合は、成らない手をさらに確認する
        if constexpr (!removeUnpromote) {
          // 成らない手を追加する
          if (promote) {
            // 王手に限定する場合は王手になる場合にのみ合法手に登録する
            // そうでない場合は無条件に合法手に登録する
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
 * 持ち駒の歩の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 持ち駒の歩の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 持ち駒の歩の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalHandPawnMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;

  // 持ち駒に歩がない場合は、合法手は存在しない
  if (_hands[my_color_idx][PAWN_INDEX] == 0) {
    return;
  }

  // すでに歩がある列のビットボードを作成する
  BitBoard pawn_exist_bitboard;
  BitBoard pawn_piece_bitboard = _pieceBitBoards[my_color_idx][PAWN_INDEX];

  while (pawn_piece_bitboard) {
    constexpr uint64_t mask = (1ULL << BOARD_SIZE) - 1;
    int8_t idx = pawn_piece_bitboard.popRightmostBitIndex();
    int8_t shift = idx / BOARD_SIZE * BOARD_SIZE;

    pawn_exist_bitboard |= BitBoard(mask, 0) << shift;
  }

  // 歩を打てる場所のビットボードを作成する
  BitBoard hand_pawn_bitboard =
      BITBOARD_PAWN_DROPABLES[my_color_idx] & ~pawn_exist_bitboard & destinationBitBoard;

  // 王手となる（打ち歩詰めになる可能性がある）座標を計算しておく
  int8_t op_king_idx = _kingPositions[op_color_idx].getIndex();
  int8_t pawn_mate_idx =
      (op_king_idx >= 0)
          ? BITBOARD_PAWN_ATTACKS[op_color_idx][op_king_idx].getRightmostBitIndex()
          : -1;

  // それぞれの座標について合法手を作成する
  while (hand_pawn_bitboard) {
    int8_t dst_idx = hand_pawn_bitboard.popRightmostBitIndex();

    // 打ち歩詰めの場合は次の座標について確認する
    if (dst_idx == pawn_mate_idx && _isDropPawnCheckmateMove(dst_idx)) {
      continue;
    }

    // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
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
 * 持ち駒の香の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 持ち駒の香の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 持ち駒の香の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalHandLanceMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 持ち駒に香がない場合は、合法手は存在しない
  if (_hands[my_color_idx][LANCE_INDEX] == 0) {
    return;
  }

  // 香を打てる場所のビットボードを作成する
  // 香の打てる場所は二歩を考慮しない歩を打てる場所と同じなので、
  // 歩のビットボードを再利用する
  BitBoard hand_lance_bitboard = BITBOARD_PAWN_DROPABLES[my_color_idx] & destinationBitBoard;

  // それぞれの座標について合法手を作成する
  while (hand_lance_bitboard) {
    int8_t dst_idx = hand_lance_bitboard.popRightmostBitIndex();

    // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
    // そうでない場合は無条件に合法手に登録する
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
 * 持ち駒の桂の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 持ち駒の桂の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 持ち駒の桂の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalHandKnightMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 持ち駒に桂がない場合は、合法手は存在しない
  if (_hands[my_color_idx][KNIGHT_INDEX] == 0) {
    return;
  }

  // 桂を打てる場所のビットボードを作成する
  BitBoard hand_knight_bitboard = BITBOARD_KNIGHT_DROPABLES[my_color_idx] & destinationBitBoard;

  // それぞれの座標について合法手を作成する
  while (hand_knight_bitboard) {
    int8_t dst_idx = hand_knight_bitboard.popRightmostBitIndex();

    // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
    // そうでない場合は無条件に合法手に登録する
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
 * 持ち駒の銀の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 持ち駒の銀の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 持ち駒の銀の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalHandSilverMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 持ち駒に銀がない場合は、合法手は存在しない
  if (_hands[my_color_idx][SILVER_INDEX] == 0) {
    return;
  }

  // 銀を打てる場所のビットボードを作成する
  BitBoard hand_silver_bitboard = destinationBitBoard;

  // それぞれの座標について合法手を作成する
  while (hand_silver_bitboard) {
    int8_t dst_idx = hand_silver_bitboard.popRightmostBitIndex();

    // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
    // そうでない場合は無条件に合法手に登録する
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
 * 持ち駒の金の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 持ち駒の金の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 持ち駒の金の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalHandGoldMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 持ち駒に金がない場合は、合法手は存在しない
  if (_hands[my_color_idx][GOLD_INDEX] == 0) {
    return;
  }

  // 金を打てる場所のビットボードを作成する
  BitBoard hand_gold_bitboard = destinationBitBoard;

  // それぞれの座標について合法手を作成する
  while (hand_gold_bitboard) {
    int8_t dst_idx = hand_gold_bitboard.popRightmostBitIndex();

    // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
    // そうでない場合は無条件に合法手に登録する
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
 * 持ち駒の角の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 持ち駒の角の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 持ち駒の角の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalHandBishopMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 持ち駒に角がない場合は、合法手は存在しない
  if (_hands[my_color_idx][BISHOP_INDEX] == 0) {
    return;
  }

  // 角を打てる場所のビットボードを作成する
  BitBoard hand_bishop_bitboard = destinationBitBoard;

  // それぞれの座標について合法手を作成する
  while (hand_bishop_bitboard) {
    int8_t dst_idx = hand_bishop_bitboard.popRightmostBitIndex();

    // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
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
 * 持ち駒の飛の移動の合法手を作成する。
 * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
 * @param legalMoves 持ち駒の飛の移動の合法手の一覧を追加する配列オブジェクト
 * @param destinationBitBoard 持ち駒の飛の移動の目的地として有効な座標を表すビットボード
 */
template <bool checkOnly>
void Board::_getLegalHandRookMoves(
    std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;

  // 持ち駒に飛がない場合は、合法手は存在しない
  if (_hands[my_color_idx][ROOK_INDEX] == 0) {
    return;
  }

  // 飛を打てる場所のビットボードを作成する
  BitBoard hand_rook_bitboard = destinationBitBoard;

  // それぞれの座標について合法手を作成する
  while (hand_rook_bitboard) {
    int8_t dst_idx = hand_rook_bitboard.popRightmostBitIndex();

    // 王手のみに限定する場合は王手になる場合にのみ合法手に登録する
    // そうでない場合は無条件に合法手に登録する
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
 * 指定された位置から指定された位置への移動が王手になるかを返す。
 * template引数pieceで移動させる駒の種類を指定する（PIECE_BLACK_XXXで指定する）。
 * @param srcIndex 移動元の位置を表す整数値
 * @param dstIndex 移動先の位置を表す整数値
 * @return 指定された位置から指定された位置への移動が王手になる場合はtrue
 */
template <uint8_t piece>
bool Board::_isCheckMove(int8_t srcIndex, int8_t dstIndex) const {
  return _isDropCheckMove<piece>(dstIndex) ||
         _isDiscoveredCheckMove(srcIndex, dstIndex, OPPOSITE_COLOR(_color));
}

/**
 * 指定された位置から指定された位置への移動が空き王手を発生させるかを返す。
 * @param srcIndex 移動元の位置を表す整数値
 * @param dstIndex 移動先の位置を表す整数値
 * @param color 確認対象となる王の駒の手番
 * @return 指定された位置から指定された位置への移動が空き王手を発生させる場合はtrue
 */
bool Board::_isDiscoveredCheckMove(int8_t srcIndex, int8_t dstIndex, int8_t color) const {
  int8_t my_color_idx = (color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t king_pos_idx = _kingPositions[my_color_idx].getIndex();

  // 王が盤上に存在しない場合は空き王手にはならない
  if (king_pos_idx < 0) {
    return false;
  }

  // 王との位置関係を確認する
  int8_t direction = DIRECTION_INDICES[srcIndex][king_pos_idx];

  // 王が8方向の延長線上に存在しないなら空き王手にはならない
  if (direction == 8) {
    return false;
  }

  // 移動方向が王と同じ方向、もしくは、反対方向なら空き王手にはならない
  int8_t move_direction = DIRECTION_INDICES[srcIndex][dstIndex];

  if (move_direction == direction || move_direction == (direction + 4) % 8) {
    return false;
  }

  // 王との間に駒が存在するなら空き王手にはならない
  BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
  BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][srcIndex] & occ_bitboard;
  int8_t between_idx = (direction < 4)
                           ? between_bitboard.getLeftmostBitIndex()
                           : between_bitboard.getRightmostBitIndex();

  if (between_idx != king_pos_idx) {
    return false;
  }

  // 反対側に存在する駒を確認する
  BitBoard opposite_bitboard =
      BITBOARD_LONG_ATTACKS[(direction + 4) % 8][srcIndex] & occ_bitboard;
  int8_t opposite_idx = (direction < 4)
                            ? opposite_bitboard.getRightmostBitIndex()
                            : opposite_bitboard.getLeftmostBitIndex();

  // 駒が存在しないなら空き王手にはならない
  if (opposite_idx < 0) {
    return false;
  }
  // 移動方向が奇数（斜め）、かつ、存在する駒が角の効きがあるなら空き王手となる
  else if ((direction % 2 == 1) &&
           _pieceBitBoards[op_color_idx][BISHOP_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // 移動方向が偶数（縦横）、かつ、存在する駒が飛の効きがあるなら空き王手となる
  else if ((direction % 2 == 0) &&
           _pieceBitBoards[op_color_idx][ROOK_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // 先手で移動方向が下、かつ、存在する駒が香の効きがあるなら空き王手となる
  else if (color == COLOR_BLACK && direction == 4 &&
           _pieceBitBoards[op_color_idx][LANCE_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // 後手で移動方向が上、かつ、存在する駒が香の効きがあるなら空き王手となる
  else if (color == COLOR_WHITE && direction == 0 &&
           _pieceBitBoards[op_color_idx][LANCE_INDEX].hasBit(opposite_idx)) {
    return true;
  }
  // それ以外の場合は空き王手にはならない
  else {
    return false;
  }
}

/**
 * 指定された位置への駒の打ちが王手になるかを返す。
 * template引数pieceで打つ駒の種類を指定する（PIECE_BLACK_XXXで指定する）。
 * @param dstIndex 移動先の位置を表す整数値
 * @return 指定された位置への駒の打ちが王手になる場合はtrue
 */
template <uint8_t piece>
bool Board::_isDropCheckMove(int8_t dstIndex) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t op_king_idx = _kingPositions[op_color_idx].getIndex();

  // 王が盤上に存在しない場合は王手にはならない
  if (op_king_idx < 0) {
    return false;
  }

  // 歩の場合（打ち歩詰めは別途確認するため、ここでは考慮しない）
  if constexpr (piece == PIECE_BLACK_PAWN) {
    return BITBOARD_PAWN_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // 香の場合
  if constexpr (piece == PIECE_BLACK_LANCE) {
    // 先手で移動方向が上、後手で移動方向が下でないなら王手にはならない
    int8_t direction = DIRECTION_INDICES[dstIndex][op_king_idx];

    if ((_color == COLOR_BLACK && direction != 0) ||
        (_color == COLOR_WHITE && direction != 4)) {
      return false;
    }

    // 相手の王との間に駒が存在しないなら王手になる
    BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
    BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][dstIndex] & occ_bitboard;
    int8_t between_idx = (direction < 4)
                             ? between_bitboard.getLeftmostBitIndex()
                             : between_bitboard.getRightmostBitIndex();

    return between_idx == op_king_idx;
  }

  // 桂の場合
  if constexpr (piece == PIECE_BLACK_KNIGHT) {
    return BITBOARD_KNIGHT_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // 銀の場合
  if constexpr (piece == PIECE_BLACK_SILVER) {
    return BITBOARD_SILVER_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // 金の場合
  if constexpr (piece == PIECE_BLACK_GOLD) {
    return BITBOARD_GOLD_ATTACKS[my_color_idx][dstIndex].hasBit(op_king_idx);
  }

  // 馬と龍の場合（飛び駒の効きは後で確認する）
  if constexpr (piece == PIECE_BLACK_HORSE || piece == PIECE_BLACK_DRAGON) {
    if (BITBOARD_KING_ATTACKS[dstIndex].hasBit(op_king_idx)) {
      return true;
    }
  }

  // 角の場合（馬の1マス移動は確認済みであるため、斜めの方向を見て結論を出す）
  if constexpr (piece == PIECE_BLACK_BISHOP || piece == PIECE_BLACK_HORSE) {
    // 相手の王へのの方向が斜めでないなら王手にはならない
    int8_t direction = DIRECTION_INDICES[dstIndex][op_king_idx];

    if (direction % 2 == 0) {
      return false;
    }

    // 相手の王との間に駒が存在しないなら王手になる
    BitBoard occ_bitboard = _colorBitBoards[0] | _colorBitBoards[1];
    BitBoard between_bitboard = BITBOARD_LONG_ATTACKS[direction][dstIndex] & occ_bitboard;
    int8_t between_idx = (direction < 4)
                             ? between_bitboard.getLeftmostBitIndex()
                             : between_bitboard.getRightmostBitIndex();

    return between_idx == op_king_idx;
  }

  // 飛の場合（龍の1マス移動は確認済みであるため、縦横の方向を見て結論を出す）
  if constexpr (piece == PIECE_BLACK_ROOK || piece == PIECE_BLACK_DRAGON) {
    // 相手の王へのの方向が縦横でないなら王手にはならない
    int8_t direction = DIRECTION_INDICES[dstIndex][op_king_idx];

    if (direction == 8 || direction % 2 == 1) {
      return false;
    }

    // 相手の王との間に駒が存在しないなら王手になる
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
 * 指定された位置への歩の打ちが打ち歩詰めになるかを返す。
 * @param dstIndex 移動先の位置を表す整数値
 * @return 指定された位置への歩の打ちが打ち歩詰めになる場合はtrue
 */
bool Board::_isDropPawnCheckmateMove(int8_t dstIndex) const {
  int8_t my_color_idx = (_color == COLOR_BLACK) ? 0 : 1;
  int8_t op_color_idx = 1 - my_color_idx;
  int8_t op_king_idx = _kingPositions[op_color_idx].getIndex();

  // 王が盤上に存在しない場合は打ち歩詰めにはならない
  if (op_king_idx < 0) {
    return false;
  }

  // 指定された座標が王の直前の座標でない場合は打ち歩詰めにはならない
  int8_t pawn_mate_idx =
      BITBOARD_PAWN_ATTACKS[op_color_idx][op_king_idx].getRightmostBitIndex();

  if (dstIndex != pawn_mate_idx) {
    return false;
  }

  // 王が移動できる場所のいずれかに効いている駒がない場合は打ち歩詰めにならない
  // 歩によって飛び駒の効きが遮断される可能性を考慮して計算すること
  BitBoard king_move_bitboard =
      BITBOARD_KING_ATTACKS[op_king_idx] & ~_colorBitBoards[op_color_idx];

  while (king_move_bitboard) {
    int8_t dst_idx = king_move_bitboard.popRightmostBitIndex();

    if (_getAttackers<true, true>(OPPOSITE_COLOR(_color), dst_idx, pawn_mate_idx).empty()) {
      return false;
    }
  }

  // 王以外のいずれかの駒が歩の場所に移動できる場合は打ち歩詰めにならない
  for (int8_t attacker_pos : _getAttackers<false, false>(_color, pawn_mate_idx)) {
    if (attacker_pos != op_king_idx &&
        !_isDiscoveredCheckMove(attacker_pos, pawn_mate_idx, OPPOSITE_COLOR(_color))) {
      return false;
    }
  }

  // 以上のいずれの条件も満たさない場合は打ち歩詰めになる
  return true;
}

/**
 * モデルに入力する盤面データを取得する。
 * @param inputs モデルに入力する盤面データ
 * @param color 手番（COLOR_BLACK または COLOR_WHITE）
 */
void Board::_getBoardInputs(int32_t* inputs, int8_t color) const {
  constexpr int32_t black_offset = 1;
  constexpr int32_t white_offset = black_offset + 14 + 14 + 6;
  constexpr int32_t other_offset = white_offset + 14 + 14 + 6;
  constexpr int32_t board_square = BOARD_SIZE * BOARD_SIZE;

  for (int32_t src = 0; src < board_square; src++) {
    // 値を設定する場所のインデックスを計算する
    // 後手番の場合、盤面を180度回転する
    int32_t dst = (color == COLOR_BLACK) ? src : (board_square - 1 - src);

    // 空マスの情報を設定する
    if (_cells[src] == PIECE_EMPTY) {
      setInputBit(inputs, 0 * board_square + dst);
    }

    // 駒の配置の値を設定する
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

    // 駒の利きの値を設定する
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

    // 駒の利きの数を設定する
    black_att_count = std::min(black_att_count, 5);
    white_att_count = std::min(white_att_count, 5);

    setInputBit(inputs, (black_offset + 14 + 14 + black_att_count) * board_square + dst);
    setInputBit(inputs, (white_offset + 14 + 14 + white_att_count) * board_square + dst);

    // 最後に移動した駒の座標を設定する
    if (_lastMove != MOVE_INVALID && _lastMove.getDst().getIndex() == src) {
      setInputBit(inputs, other_offset * board_square + dst);
    }

    // 行番号と列番号を設定する
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
 * モデルに入力するゲームデータを取得する。
 * @param inputs モデルに入力するゲームデータ
 * @param color 手番（COLOR_BLACK または COLOR_WHITE）
 */
void Board::_getInfoInputs(int32_t* inputs, int8_t color) const {
  constexpr int32_t info_offset = MODEL_FEATURES * BOARD_SIZE * BOARD_SIZE;
  constexpr int32_t hand_offsets[] = {0, 18, 22, 26, 30, 32, 34};
  constexpr int32_t hand_length = 38;

  // 持ち駒の情報を設定する
  // 持ち駒の情報は持ち駒のビット表現をそのまま利用する
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

  // 王手の情報を設定する
  if (isCheck(_color)) {
    setInputBit(inputs, info_offset + hand_length * 2);
  }

  // 入玉宣言に必要な点数を設定する
  if (color == COLOR_BLACK) {
    inputs[MODEL_INPUT_PACK_SIZE - 3] = (int)((_nyugyokuScores[0] - 27.5) / 5.0 * 0xfffff);
    inputs[MODEL_INPUT_PACK_SIZE - 2] = (int)((_nyugyokuScores[1] - 27.5) / 5.0 * 0xfffff);
  } else {
    inputs[MODEL_INPUT_PACK_SIZE - 3] = (int)((_nyugyokuScores[1] - 27.5) / 5.0 * 0xfffff);
    inputs[MODEL_INPUT_PACK_SIZE - 2] = (int)((_nyugyokuScores[0] - 27.5) / 5.0 * 0xfffff);
  }

  // 引き分けまでの残り手数を設定する
  float remaining_turn = 1.0f - (_drawTurn - _turn) / 50.0f;

  remaining_turn = std::min(std::max(remaining_turn, 0.0f), 1.0f);
  inputs[MODEL_INPUT_PACK_SIZE - 1] = (int)(remaining_turn * 0xfffff);
}

}  // namespace deepshogi

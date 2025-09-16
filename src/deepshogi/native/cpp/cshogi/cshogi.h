#pragma once

#include "book.hpp"
#include "dfpn.h"
#include "generateMoves.hpp"
#include "init.hpp"
#include "mate.h"
#include "position.hpp"
#include "usi.hpp"

namespace deepshogi::cshogi {

// 入力特徴量のための定数(dlshogi互換)
constexpr int PIECETYPE_NUM = 14;  // 駒の種類
constexpr int MAX_ATTACK_NUM = 3;  // 利き数の最大値
constexpr u32 MAX_FEATURES1_NUM = PIECETYPE_NUM /*駒の配置*/ + PIECETYPE_NUM /*駒の利き*/ + MAX_ATTACK_NUM /*利き数*/;

constexpr int MAX_HPAWN_NUM = 8;  // 歩の持ち駒の上限
constexpr int MAX_HLANCE_NUM = 4;
constexpr int MAX_HKNIGHT_NUM = 4;
constexpr int MAX_HSILVER_NUM = 4;
constexpr int MAX_HGOLD_NUM = 4;
constexpr int MAX_HBISHOP_NUM = 2;
constexpr int MAX_HROOK_NUM = 2;
constexpr u32 MAX_PIECES_IN_HAND[] = {
    MAX_HPAWN_NUM,    // PAWN
    MAX_HLANCE_NUM,   // LANCE
    MAX_HKNIGHT_NUM,  // KNIGHT
    MAX_HSILVER_NUM,  // SILVER
    MAX_HGOLD_NUM,    // GOLD
    MAX_HBISHOP_NUM,  // BISHOP
    MAX_HROOK_NUM,    // ROOK
};
constexpr u32 MAX_PIECES_IN_HAND_SUM = MAX_HPAWN_NUM + MAX_HLANCE_NUM + MAX_HKNIGHT_NUM + MAX_HSILVER_NUM + MAX_HGOLD_NUM + MAX_HBISHOP_NUM + MAX_HROOK_NUM;
constexpr u32 MAX_FEATURES2_HAND_NUM = (int)ColorNum * MAX_PIECES_IN_HAND_SUM;
constexpr u32 MAX_FEATURES2_NUM = MAX_FEATURES2_HAND_NUM + 1 /*王手*/;

// 移動の定数
enum MOVE_DIRECTION {
  UP,
  UP_LEFT,
  UP_RIGHT,
  LEFT,
  RIGHT,
  DOWN,
  DOWN_LEFT,
  DOWN_RIGHT,
  UP2_LEFT,
  UP2_RIGHT,
  UP_PROMOTE,
  UP_LEFT_PROMOTE,
  UP_RIGHT_PROMOTE,
  LEFT_PROMOTE,
  RIGHT_PROMOTE,
  DOWN_PROMOTE,
  DOWN_LEFT_PROMOTE,
  DOWN_RIGHT_PROMOTE,
  UP2_LEFT_PROMOTE,
  UP2_RIGHT_PROMOTE,
  MOVE_DIRECTION_NUM
};

int __dlshogi_get_features1_num();
int __dlshogi_get_features2_num();

bool nyugyoku(const Position& pos);

void HuffmanCodedPos_init();
void PackedSfen_init();

void Book_init();

std::string __to_usi(const int move);
std::string __to_csa(const int move);

unsigned short __move16_from_psv(const unsigned short move16);

class __Board {
 public:
  __Board();
  __Board(const std::string& sfen);
  virtual ~__Board();

  void set(const std::string& sfen);
  void set_position(const std::string& position);
  void set_pieces(const int pieces[], const int pieces_in_hand[][7]);
  void set_hcp(char* hcp);
  void set_psfen(char* psfen);
  void reset();

  std::string dump() const;

  void push(const int move);
  int pop();
  int peek();
  void push_pass();
  void pop_pass();

  std::vector<int> get_history() const;
  int get_last_move() const;
  bool is_game_over() const;

  int isDraw(const int checkMaxPly) const;
  int moveIsDraw(const int move, const int checkMaxPly) const;

  int move(const int from_square, const int to_square, const bool promotion) const;
  int drop_move(const int to_square, const int drop_piece_type) const;
  int move_from_usi(const std::string& usi) const;
  int move_from_csa(const std::string& csa) const;
  int move_from_move16(const unsigned short move16) const;
  int move_from_psv(const unsigned short move16) const;

  int turn() const;
  void setTurn(const int turn);
  int ply() const;
  void setPly(const int ply);
  std::string toSFEN() const;
  std::string toCSAPos() const;
  void toHuffmanCodedPos(char* data) const;
  void toPackedSfen(char* data) const;
  int piece(const int sq) const;
  int kingSquare(const int c);
  bool inCheck() const;
  int mateMoveIn1Ply();
  int mateMove(int ply);
  bool is_mate(int ply);
  unsigned long long getKey() const;
  bool moveIsPseudoLegal(const int move) const;
  bool pseudoLegalMoveIsLegal(const int move) const;
  bool moveIsLegal(const int move) const;
  bool is_nyugyoku() const;
  bool isOK() const;

  std::vector<int> pieces_in_hand(const int color) const;
  std::vector<int> pieces() const;
  void piece_planes(char* mem) const;

  // 白の場合、盤を反転するバージョン
  void piece_planes_rotate(char* mem) const;

  // 駒の利き、王手情報を含む特徴量(dlshogi互換)
  void _dlshogi_make_input_features(char* mem1, char* mem2) const;

  unsigned long long bookKey();
  unsigned long long bookKeyAfter(const unsigned long long key, const int move);

  Position pos;

 private:
  std::deque<std::pair<Move, StateInfo>> history;

  void bbToVector(PieceType pt, Color c, Piece piece, std::vector<int>& board) const;
};

class __LegalMoveList {
 public:
  __LegalMoveList();
  __LegalMoveList(const __Board& board);

  bool end() const;
  int move() const;
  void next();
  int size() const;

 private:
  std::shared_ptr<MoveList<LegalAll>> ml;
};

class __PseudoLegalMoveList {
 public:
  __PseudoLegalMoveList();
  __PseudoLegalMoveList(const __Board& board);

  bool end() const;
  int move() const;
  void next();
  int size() const;

 private:
  std::shared_ptr<MoveList<PseudoLegal>> ml;
};

int __piece_to_piece_type(const int p);
int __hand_piece_to_piece_type(const int hp);

int __make_file(const int sq);
int __make_rank(const int sq);

// 移動先
int __move_to(const int move);
// 移動元
int __move_from(const int move);
// 取った駒の種類
int __move_cap(const int move);
// 成るかどうか
bool __move_is_promotion(const int move);
// 駒打ちか
bool __move_is_drop(const int move);
// 移動する駒の種類
int __move_from_piece_type(const int move);
// 打つ駒の種類
int __move_drop_hand_piece(const int move);

unsigned short __move16(const int move);

unsigned short __move16_from_psv(const unsigned short move16);
unsigned short __move16_to_psv(const unsigned short move16);

// 反転
int __move_rotate(const int move);

std::string __move_to_usi(const int move);
std::string __move_to_csa(const int move);

inline MOVE_DIRECTION get_move_direction(const int dir_x, const int dir_y);

// 駒の移動を表すラベル(dlshogi互換)
int __dlshogi_make_move_label(const int move, const int color);

class __DfPn {
 public:
  __DfPn();
  __DfPn(const int max_depth, const uint32_t max_search_node, const int draw_ply);
  bool search(__Board& board);
  bool search_andnode(__Board& board);
  void stop(bool is_stop);
  int get_move(__Board& board);
  void get_pv(__Board& board);
  void set_draw_ply(const int draw_ply);
  void set_maxdepth(const int depth);
  void set_max_search_node(const uint32_t max_search_node);
  uint32_t get_searched_node();

  std::vector<u32> pv;

 private:
  DfPn dfpn;
};

} // namespace deepshogi::cshogi

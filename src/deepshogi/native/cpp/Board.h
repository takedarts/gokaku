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
 * 盤面の情報を保持するクラス。
 */
class Board {
 private:
  /**
   * 盤面のハッシュ値を計算するためのクラス。
   * Boardクラスのprivateメンバにアクセスできるようにするため、Boardクラスのfriendクラスとする。
   */
  friend class BoardHash;

 public:
  /**
   * 盤面オブジェクトを生成する。
   * 盤面への駒の配置は行わない。
   */
  Board();

  /**
   * 盤面オブジェクトを生成する。
   * 盤面への駒の配置は行わない。
   * @param nyugyokuScoreBlack 先手番の入玉宣言に必要な点数
   * @param nyugyokuScoreWhite 後手番の入玉宣言に必要な点数
   * @param drawTurn 引き分けとなるまでの手数
   */
  Board(int8_t nyugyokuScoreBlack, int8_t nyugyokuScoreWhite, int16_t drawTurn);

  /**
   * 盤面オブジェクトを破棄する。
   */
  virtual ~Board() = default;

  /**
   * SFEN形式の文字列で盤面を初期化する。
   * @param sfen SFEN形式の文字列
   */
  void initialize(const std::string& sfen);

  /**
   * 駒を動かす。
   * @param move 着手
   * @return 着手の結果
   */
  MoveResult play(const Move& move);

  /**
   * 指定された着手の内容を盤面から取り消す。
   * この関数は指定された着手が直前の着手であることを前提としている。
   * @param result 取り消す着手の結果
   */
  void undo(const MoveResult& result);

  /**
   * 指定された座標に効きがある駒の位置を取得する。
   * @param position 確認する座標
   * @return 指定された座標に効きがある駒の位置のリスト
   */
  std::vector<Position> getAttackers(const Position& position) const;

  /**
   * 現在の盤面の合法手の一覧を取得する。
   * @param removeUnpromote 歩、角、飛車、2行目の香の不成の手を削除する場合はtrue
   * @param checkOnly 王手が発生する手のみを取得する場合はtrue
   * @return 合法手の一覧
   */
  std::vector<Move> getLegalMoves(bool removeUnpromote, bool checkOnly) const;

  /**
   * 現在の盤面の詰み筋の着手手順を取得する。
   * @param depth 詰み探索の深さ
   * @return 詰み筋の着手手順
   */
  std::vector<Move> getCheckmateMoves(int32_t depth) const;

  /**
   * 入玉宣言可能な状態であればtrueを返す。
   * @param color 宣言側の手番
   * @return 入玉宣言可能な状態であればtrue
   */
  bool isNyugyoku(int8_t color) const;

  /**
   * 王手がかかっていればtrueを返す。
   * @param color 王手をかけられている側の色
   * @return 王手がかかっていればtrue
   */
  bool isCheck(int8_t color) const;

  /**
   * SFEN形式の文字列を取得する。
   * @return SFEN形式の文字列
   */
  std::string getSfen() const;

  /**
   * モデルに入力するデータを取得する。
   * @param inputs モデルに入力するデータ
   */
  void getInputs(int32_t* inputs) const;

  /**
   * モデルに入力するデータを取得する。
   * @param inputs モデルに入力するデータ
   * @param color 手番
   */
  void getInputs(int32_t* inputs, int8_t color) const;

  /**
   * 盤面の状態をコピーする。
   * @param board コピー元の盤面
   */
  void copyFrom(const Board* board);

  /**
   * 盤面情報を表示するための文字列を取得する。
   * @return 盤面情報を表示するための文字列
   */
  std::string toString() const;

  /**
   * 手番を取得する。
   * @return 手番
   */
  inline int8_t getColor() const {
    return _color;
  }

  /**
   * 現在の手数を取得する。
   * @return 現在の手数
   */
  inline int16_t getTurn() const {
    return _turn;
  }

  /**
   * 引き分けとなる手数を取得する。
   * @return 引き分けとなる手数
   */
  inline int16_t getDrawTurn() const {
    return _drawTurn;
  }

  /**
   * この盤面が指定された盤面の同じ盤面か劣後盤面であればtrueを返す。
   * 劣後盤面とは、手番と盤上の駒の配置が同じであり、
   * すべての種類の持ち駒の数が同じか少ない盤面のことを意味する。
   * 盤上の駒の配置が同じであることは、盤面のハッシュ値が同じであることで確認する。
   * （ハッシュ値の衝突は、実際には非常に稀だと考えられるため、ここでは無視する）
   * この持ち駒の条件を確認するためには、持ち駒のビット表現のビットが立っている位置の集合が
   * もう片方の集合の部分集合になっていることを確認すればよい。
   * @param other 比較対象の盤面
   * @param color 評価対象の手番
   * @return 同じ盤面か劣後盤面であればtrue
   */
  inline bool isLesserThanOrEqual(const Board& other, int8_t color) const {
    // 手番が異なるならば違う盤面と判断する
    if (_color != other._color) {
      return false;
    }

    // 盤面のハッシュ値が異なるならば、盤上の駒の配置が異なると判断する
    if (_cellHash != other._cellHash) {
      return false;
    }

    // 盤面の配置が異なるならFalseを返す
    if (_colorBitBoards[0] != other._colorBitBoards[0] ||
        _colorBitBoards[1] != other._colorBitBoards[1]) {
      return false;
    }

    // 持ち駒のビット表現のビットが立っている位置の集合が
    // もう片方の集合の部分集合になっていることを確認する
    int8_t color_idx = (color == COLOR_BLACK) ? 0 : 1;

    return (_handBits[color_idx] & other._handBits[color_idx]) == _handBits[color_idx];
  }

  /**
   * この盤面が指定された盤面の劣後盤面であればtrueを返す。
   * @param other 比較対象の盤面
   * @param color 評価対象の手番
   * @return 劣後盤面であればtrue
   */
  inline bool isLesserThan(const Board& other, int8_t color) const {
    // 手番が異なるならば違う盤面と判断する
    if (_color != other._color) {
      return false;
    }

    // 盤面のハッシュ値が異なるならば、盤上の駒の配置が異なると判断する
    if (_cellHash != other._cellHash) {
      return false;
    }

    // 盤面の配置が異なるならFalseを返す
    if (_colorBitBoards[0] != other._colorBitBoards[0] ||
        _colorBitBoards[1] != other._colorBitBoards[1]) {
      return false;
    }

    // 持ち駒のビット表現が同じならば、持ち駒の数も同じであると判断する
    if (_handBits[0] == other._handBits[0] && _handBits[1] == other._handBits[1]) {
      return false;
    }

    // 持ち駒のビット表現のビットが立っている位置の集合が
    // もう片方の集合の部分集合になっていることを確認する
    int8_t color_idx = (color == COLOR_BLACK) ? 0 : 1;

    return (_handBits[color_idx] & other._handBits[color_idx]) == _handBits[color_idx];
  }

  /**
   * 指定された座標の駒を取得する。
   * @param x x座標
   * @param y y座標
   * @return 駒の種類
   */
  inline uint8_t getPiece(const Position& position) const {
    return _cells[position.getIndex()];
  }

  /**
   * 指定された種類の持ち駒の数を取得する。
   * @param color 手番
   * @param piece 駒の種類
   * @return 持ち駒の数
   */
  inline int8_t getHandPieceNum(int8_t color, uint8_t piece) const {
    return _hands[(color == COLOR_BLACK) ? 0 : 1][piece];
  }

  /**
   * 最後の着手を取得する。
   * @return 最後の着手
   */
  inline Move getLastMove() const {
    return _lastMove;
  }

  /**
   * 盤面情報の文字列表現を出力ストリームに書き込む。
   * @param os 出力ストリーム
   * @param board 盤面オブジェクト
   * @return 出力ストリーム
   */
  friend std::ostream& operator<<(std::ostream& os, const Board& board) {
    os << board.toString();
    return os;
  }

 private:
  /**
   * 盤面オブジェクトをコピーする。
   * 演算子`=`によるコピーは内部でのみ使用可能とする。
   * @param board コピー元の盤面オブジェクト
   */
  Board(const Board& board) = default;

  /**
   * 盤面上の各マスの状態を表す配列。
   */
  uint8_t _cells[BOARD_SIZE * BOARD_SIZE];

  /**
   * 盤面上の駒の配置を表すハッシュ値。
   */
  uint64_t _cellHash;

  /**
   * 各プレイヤーの持ち駒の数を表す配列。
   */
  uint8_t _hands[2][PIECE_HAND_END - PIECE_HAND_BEGIN];

  /**
   * 各プレイヤーの持ち駒の数をビットで表現する配列。
   * 以下の範囲のビットを駒の数だけ立てる。
   * 歩: 0-17, 香: 18-21, 桂: 22-25, 銀: 26-29, 角: 30-31, 飛: 32-33, 金: 34-37
   *
   * このビット表現は以下の目的で使用される。
   * - 劣後盤面を判定するときの持ち駒の条件を確認するため
   * - モデルへの入力データを生成するときの持ち駒の情報を作成するため
   */
  uint64_t _handBits[2];

  /**
   * 王の位置（0:先手、1:後手）。
   */
  Position _kingPositions[2];

  /**
   * 入玉宣言に必要な点数（0:先手、1:後手）。
   */
  int8_t _nyugyokuScores[2];

  /**
   * 手番。
   */
  int8_t _color;

  /**
   * 現在の手数。
   */
  int16_t _turn;

  /**
   * 引き分けとなる手数。
   */
  int16_t _drawTurn;

  /**
   * 直前の着手を保存するオブジェクト。
   */
  Move _lastMove;

  /**
   * 白黒の駒の所在を表すビットボード（0: 先手の駒、1: 後手の駒）。
   * 各ビットは盤面上のマスに対応し、駒が存在する場合は1、それ以外は0となる。
   */
  BitBoard _colorBitBoards[2];

  /**
   * 各種類の駒の所在を表すビットボード（0: 先手の駒、1: 後手の駒）。
   * 各ビットは盤面上のマスに対応し、駒が存在する場合は1、それ以外は0となる。
   * 配列のインデックスは、駒の種類に対応する。
   * ただし、以下の駒は別の駒のビットボードに割り当てられる。
   *  - と金、成香、成桂、成銀は、それぞれ金のビットボードに割り当てられる
   *  - 馬は、角と王のビットボードに割り当てられる
   *  - 龍は、飛と王のビットボードに割り当てられる
   */
  BitBoard _pieceBitBoards[2][PIECE_BLACK_PRO_PAWN - PIECE_BLACK_BEGIN];

  /**
   * 指定された位置に駒を配置する。
   * この関数は指定座標に駒がないことを前提としている。
   * @param pos 駒を配置する座標
   * @param piece 配置する駒を表す整数値
   */
  void _putPiece(const Position& pos, uint8_t piece);

  /**
   * 指定された位置から駒を取り除く。
   * @param pos 駒を取り除く座標
   */
  void _removePiece(const Position& pos);

  /**
   * 指定された駒を持ち駒として追加する。
   * @param color 手番（COLOR_BLACK または COLOR_WHITE）
   * @param piece 追加する駒を表す整数値
   * @param num 追加する駒の数
   */
  void _addHand(int8_t color, uint8_t piece, int32_t num);

  /**
   * 指定された駒を持ち駒として追加する。
   * @param color 手番（COLOR_BLACK または COLOR_WHITE）
   * @param piece 追加する駒を表す整数値
   */
  void _addHand(int8_t color, uint8_t piece);

  /**
   * 指定された駒を持ち駒から取り除く。
   * この関数は指定された駒が持ち駒に存在することを前提としている。
   * @param color 手番（COLOR_BLACK または COLOR_WHITE）
   * @param piece 取り除く駒を表す整数値
   */
  void _removeHand(int8_t color, uint8_t piece);

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
  std::vector<int8_t> _getAttackers(
      int8_t color, int8_t posIndex, int8_t additionalOccIndex = -1) const;

  /**
   * 現在の盤面の合法手の一覧を取得する。
   * template引数removeUnpromoteがtrueの場合は、歩、角、飛車、2行目の香の不成の手を削除する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 合法手の一覧を追加する配列オブジェクト
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalMoves(std::vector<Move>& legalMoves) const;

  /**
   * 歩の移動の合法手を作成する。
   * template引数removeUnpromoteがtrueの場合は、不成の手を削除する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 歩の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 歩の移動の目的地として有効な座標を表すビットボード
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalPawnMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 香の移動の合法手を作成する。
   * template引数removeUnpromoteがtrueの場合は、2行目の不成の手を削除する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 香の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 香の移動の目的地として有効な座標を表すビットボード
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalLanceMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 桂の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 桂の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 桂の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalKnightMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 銀の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 銀の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 銀の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalSilverMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 金の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 金の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 金の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalGoldMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 王の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 王の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 王の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalKingMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 角の移動の合法手を作成する。
   * template引数removeUnpromoteがtrueの場合は、不成の手を削除する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 角の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 角の移動の目的地として有効な座標を表すビットボード
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalBishopMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 飛の移動の合法手を作成する。
   * template引数removeUnpromoteがtrueの場合は、不成の手を削除する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 飛の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 飛の移動の目的地として有効な座標を表すビットボード
   */
  template <bool removeUnpromote, bool checkOnly>
  void _getLegalRookMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 持ち駒の歩の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 持ち駒の歩の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 持ち駒の歩の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalHandPawnMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 持ち駒の香の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 持ち駒の香の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 持ち駒の香の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalHandLanceMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 持ち駒の桂の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 持ち駒の桂の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 持ち駒の桂の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalHandKnightMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 持ち駒の銀の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 持ち駒の銀の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 持ち駒の銀の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalHandSilverMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 持ち駒の金の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 持ち駒の金の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 持ち駒の金の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalHandGoldMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 持ち駒の角の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 持ち駒の角の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 持ち駒の角の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalHandBishopMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 持ち駒の飛の移動の合法手を作成する。
   * template引数checkOnlyがtrueの場合は、王手が発生する手のみを取得する。
   * @param legalMoves 持ち駒の飛の移動の合法手の一覧を追加する配列オブジェクト
   * @param destinationBitBoard 持ち駒の飛の移動の目的地として有効な座標を表すビットボード
   */
  template <bool checkOnly>
  void _getLegalHandRookMoves(
      std::vector<Move>& legalMoves, const BitBoard& destinationBitBoard) const;

  /**
   * 指定された位置から指定された位置への移動が王手になるかを返す。
   * template引数pieceで移動させる駒の種類を指定する（PIECE_BLACK_XXXで指定する）。
   * @param srcIndex 移動元の位置を表す整数値
   * @param dstIndex 移動先の位置を表す整数値
   * @return 指定された位置から指定された位置への移動が王手になる場合はtrue
   */
  template <uint8_t piece>
  bool _isCheckMove(int8_t srcIndex, int8_t dstIndex) const;

  /**
   * 指定された位置から指定された位置への移動が空き王手を発生させるかを返す。
   * @param srcIndex 移動元の位置を表す整数値
   * @param dstIndex 移動先の位置を表す整数値
   * @param color 確認対象となる王の駒の手番
   * @return 指定された位置から指定された位置への移動が空き王手を発生させる場合はtrue
   */
  bool _isDiscoveredCheckMove(int8_t srcIndex, int8_t dstIndex, int8_t color) const;

  /**
   * 指定された位置への駒の打ちが王手になるかを返す。
   * template引数pieceで打つ駒の種類を指定する（PIECE_BLACK_XXXで指定する）。
   * @param dstIndex 移動先の位置を表す整数値
   * @return 指定された位置への駒の打ちが王手になる場合はtrue
   */
  template <uint8_t piece>
  bool _isDropCheckMove(int8_t dstIndex) const;

  /**
   * 指定された位置への歩の打ちが打ち歩詰めになるかを返す。
   * @param dstIndex 移動先の位置を表す整数値
   * @return 指定された位置への歩の打ちが打ち歩詰めになる場合はtrue
   */
  bool _isDropPawnCheckmateMove(int8_t dstIndex) const;

  /**
   * モデルに入力する盤面データを取得する。
   * @param inputs モデルに入力する盤面データ
   * @param color 手番（COLOR_BLACK または COLOR_WHITE）
   */
  void _getBoardInputs(int32_t* inputs, int8_t color) const;

  /**
   * モデルに入力するゲームデータを取得する。
   * @param inputs モデルに入力するゲームデータ
   * @param color 手番（COLOR_BLACK または COLOR_WHITE）
   */
  void _getInfoInputs(int32_t* inputs, int8_t color) const;
};

}  // namespace deepshogi

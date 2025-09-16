#pragma once

#include "Move.h"
#include "cshogi/cshogi.h"

namespace deepshogi {

class Board {
 public:
  /**
   * 初期盤面のインスタンスを生成する。
   * @param nyugyokuScoreBlack 先手番の入玉宣言に必要な点数
   * @param nyugyokuScoreWhite 後手番の入玉宣言に必要な点数
   * @param drawSteps 引き分けとなるまでの手数
   */
  Board(int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps);

  /**
   * 盤面を指定してインスタンスを生成する。
   * @param board 盤面情報を保持するオブジェクト
   */
  Board(const Board& board);

  /**
   * 盤面のインスタンスを生成する。
   */
  Board();

  /**
   * インスタンスを破棄する。
   */
  virtual ~Board() = default;

  /**
   * SFEN形式の文字列で盤面を初期化する。
   * @param sfen SFEN形式の文字列
   */
  void initializeWithSfen(const std::string& sfen);

  /**
   * ハフマン符号化された盤面情報で盤面を初期化する。
   * @param data ハフマン符号化された盤面情報
   */
  void initializeWithPackedSfen(char* data);

  /**
   * 駒を動かす。
   * @param move 着手
   * @return 合法手ならtrue
   */
  bool play(const Move& move);

  /**
   * 手番を取得する。
   * @return 手番
   */
  int32_t getColor() const;

  /**
   * 現在の手数を取得する。
   * @return 現在の手数
   */
  int32_t getTurn() const;

  /**
   * 指定された座標の駒を取得する。
   * @param x x座標
   * @param y y座標
   * @return 駒の種類
   */
  int32_t getPiece(int32_t x, int32_t y) const;

  /**
   * 移動した後の駒の種類を取得する。
   * @param move 着手
   * @return 駒の種類
   */
  int32_t getMovedPiece(const Move& move) const;

  /**
   * 指定された種類の持ち駒の数を取得する。
   * @param color 手番
   * @param piece 駒の種類
   * @return 持ち駒の数
   */
  int32_t getHandPieceNum(int32_t color, int32_t piece) const;

  /**
   * 指定された座標に利いている駒の座標の一覧を取得する。
   * @param x x座標
   * @param y y座標
   * @return 利いている駒の座標の一覧
   */
  std::vector<std::pair<int32_t, int32_t>> getAttackers(int32_t x, int32_t y) const;

  /**
   * 現在の盤面の合法手の一覧を取得する。
   * 歩、角、飛車の不成の手を削除した合法手の一覧を返す。
   * @return 合法手の一覧
   */
  std::vector<Move> getLegalMoves() const;

  /**
   * 着手履歴を取得する。
   * @return 着手履歴
   */
  std::vector<Move> getHistoryMoves() const;

  /**
   * 詰み筋を探索して、最初の着手を返す。
   * 詰み筋が見つからない場合はパス（MOVE_PASS）を返す。
   * checkSearchNodeが0の場合は全探索を行う。
   * checkSearchNodeが1以上の場合はdf-pnアルゴリズムを使用して探索を行う。
   * @param checkSearchDepth 詰み手筋の探索深さ
   * @param checkSearchNode 詰み手筋の探索ノード数（0なら全探索）
   * @return 詰み筋の着手
   */
  Move searchCheckMove(int32_t checkSearchDepth, int32_t checkSearchNode);

  /**
   * 入玉宣言可能な状態であればtrueを返す。
   * @return 入玉宣言可能な状態であればtrue
   */
  bool isNyugyoku() const;

  /**
   * 王手がかかっていればtrueを返す。
   * @return 王手がかかっていればtrue
   */
  bool isCheckmate() const;

  /**
   * SFEN形式の文字列を取得する。
   * @return SFEN形式の文字列
   */
  std::string getSfen() const;

  /**
   * ハフマン符号化された盤面情報を取得する。
   * @param data ハフマン符号化された盤面情報を格納するバッファ
   */
  void getPackedSfen(char* data) const;

  /**
   * モデルに入力するデータを取得する。
   * @param inputs モデルに入力するデータ
   */
  void getInputs(float* inputs) const;

  /**
   * モデルに入力するデータを取得する。
   * @param inputs モデルに入力するデータ
   * @param color 手番
   * @param steps 手数
   */
  void getInputs(float* inputs, int32_t color, int32_t steps) const;

  /**
   * 盤面の状態をコピーする。
   * @param board コピー元の盤面
   */
  void copyFrom(const Board* board);

  /**
   * 盤面情報を表示するための文字列を取得する。
   * @return 盤面情報を表示するための文字列
   */
  std::string dump() const;

  /**
   * 盤面の状態を出力する。
   * @param os 出力先
   */
  void print(std::ostream& os = std::cout) const;

 private:
  /**
   * 盤面情報を保持するオブジェクト。
   * cshogiの__Boardクラスを使用する。
   */
  cshogi::__Board _board;

  /**
   * 先手番の入玉宣言に必要な点数。
   */
  int32_t _nyugyokuScoreBlack;

  /**
   * 後手番の入玉宣言に必要な点数。
   */
  int32_t _nyugyokuScoreWhite;

  /**
   * 引き分けとなるまでの手数。
   */
  int32_t _drawSteps;

  /**
   * モデルに入力する盤面データを取得する。
   * @param inputs モデルに入力する盤面データ
   * @param color 手番
   */
  void _getBoardInputs(float* inputs, int32_t color) const;

  /**
   * モデルに入力するゲームデータを取得する。
   * @param inputs モデルに入力するゲームデータ
   * @param color 手番
   * @param steps 手数
   */
  void _getInfoInputs(float* inputs, int32_t color, int32_t steps) const;
};

}  // namespace deepshogi

#pragma once

#include <cstdint>

#include "Board.h"
#include "Move.h"

namespace deepshogi {
class PnSearchEngine;

/**
 * PN探索アルゴリズムの探索ノードを表すクラス。
 */
class PnSearchNode {
 public:
  /**
   * PN探索ノードのオブジェクトを生成する。
   * 不詰みの末端ノードを表すノードとして初期化する。
   */
  PnSearchNode();

  /**
   * コピーコンストラクタを削除する。
   */
  PnSearchNode(const PnSearchNode& node) = delete;

  /**
   * PN探索ノードのオブジェクトを破棄する。
   */
  virtual ~PnSearchNode() = default;

  /**
   * 指定された盤面情報でこのノードを末端ノードとして初期化する。
   * @param board 盤面オブジェクト
   * @param depth ノードの深さ
   */
  void initialize(const Board* board, int32_t depth);

  /**
   * ノードを展開して子ノードを生成する。
   * @param engine PN探索エンジンのオブジェクト
   * @return ノードを展開できた場合はtrue
   */
  bool expand(PnSearchEngine* engine);

  /**
   * このノードのPN/DN値を更新する。
   * @param depth_limit 深さの制限
   */
  void update(int32_t depth_limit);

  /**
   * 次に探索する子ノードを取得する。
   * このノードが末端ノードの場合はnullptrを返す。
   * 王手をかける手番の場合は「PN値+探索数の対数」が最小の子ノードを返し、
   * 王手から逃げる手番の場合は「DN値+探索数の対数」が最小の子ノードを返す。
   * 探索数を考慮した優先度を計算することで、探索の偏りを減らすことができ、
   * 手数の少ない詰み筋を発見できる可能性が高くなる。
   * @return 次に探索する子ノードのポインタ
   */
  PnSearchNode* getNextNode();

  /**
   * 詰み手順の着手と子ノードを取得する。
   * 詰み手順となる子ノードが存在しない場合はnullptrを返す。
   * @return 詰み手順の着手と子ノードのペア
   */
  std::pair<Move, PnSearchNode*> getCheckmateNode();

  /**
   * 指定された子ノードを新しい子ノードに置き換える。
   * @param targetNode 置き換える子ノード
   * @param newNode 新しい子ノード
   */
  void replaceChildNode(PnSearchNode* targetNode, PnSearchNode* newNode);

  /**
   * ノードの情報を文字列として取得する。
   * @return ノードの情報の文字列
   */
  std::string toString() const;

  /**
   * ノードの深さを取得する。
   * @return ノードの深さ
   */
  inline int32_t getDepth() const {
    return _depth;
  }

  /**
   * PN値を取得する。
   * @return PN値
   */
  inline int32_t getPn() const {
    return _pn;
  }

  /**
   * DN値を取得する。
   * @return DN値
   */
  inline int32_t getDn() const {
    return _dn;
  }

  /**
   * 詰みまでの手数を取得する。
   * @return 詰みまでの手数
   */
  inline int32_t getStep() const {
    return _step;
  }

  /**
   * ノードのサイズを取得する。
   * @return ノードのサイズ
   */
  inline int32_t getSize() const {
    return _size;
  }

  /**
   * このノードが指定されたノードの同じノードか劣後ノードであればtrueを返す。
   * 劣後ノードとは、盤上の駒の配置が同じであり、
   * すべての種類の持ち駒の数が同じか少ないノードのことを意味する。
   * 王手をかける側の手番が評価対象の手番となる。
   * @param node 比較対象のノード
   * @return 同じノードか劣後ノードであればtrue
   */
  inline bool isLesserThanOrEqual(const PnSearchNode* node) const {
    // 王手をかける側の手番：盤上の駒配置が同じで、自分の持ち駒が同じか少ないことを確認する
    if (_depth % 2 == 1) {
      return _board.isLesserThanOrEqual(node->_board, _board.getColor());
    }
    // 王手から逃げる側の手番：盤上の駒配置が同じで、相手の持ち駒が同じか少ないことを確認する
    else {
      return _board.isLesserThanOrEqual(node->_board, OPPOSITE_COLOR(_board.getColor()));
    }
  }

 private:
  /**
   * 盤面オブジェクト。
   */
  Board _board;

  /**
   * ノードの深さ。
   */
  int32_t _depth;

  /**
   * 子ノードの一覧。
   */
  std::vector<std::pair<Move, PnSearchNode*>> _children;

  /**
   * PN値。
   */
  int32_t _pn;

  /**
   * DN値。
   */
  int32_t _dn;

  /**
   * 詰みまでの手数。
   */
  int32_t _step;

  /**
   * ノードのサイズ。
   */
  int32_t _size;
};

}  // namespace deepshogi

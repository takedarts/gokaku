#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "Board.h"
#include "BoardHash.h"
#include "Move.h"
#include "PnSearchNode.h"

namespace deepshogi {

/**
 * PN探索アルゴリズムを使用して詰み筋を探索するエンジンのクラス。
 */
class PnSearchEngine {
 public:
  /**
   * PN探索エンジンのオブジェクトを生成する。
   * @param nodeSize 探索の最大ノード数
   */
  PnSearchEngine(int32_t nodeSize);

  /**
   * コピーコンストラクタを削除する。
   */
  PnSearchEngine(const PnSearchEngine& engine) = delete;

  /**
   * PN探索エンジンのオブジェクトを破棄する。
   */
  virtual ~PnSearchEngine() = default;

  /**
   * 詰み筋を探索して、着手手順を返す。
   * 詰み筋が見つからない場合は空の配列を返す。
   * @param board 盤面情報を保持するオブジェクト
   * @param depth 探索する深さ
   * @return 詰み筋の着手手順
   */
  std::vector<Move> getCheckmateMoves(const Board* board, int32_t depth);

 private:
  /**
   * 探索ノードの配列。
   */
  std::vector<PnSearchNode> _nodes;

  /**
   * 探索ノードの数。
   */
  int32_t _nodeSize;

  /**
   * 現在使用しているノード数。
   */
  int32_t _nodeCount;

  /**
   * 探索ノードのキャッシュ。
   */
  std::map<BoardHash, PnSearchNode*> _nodeCache;

  /**
   * 新しい探索ノードを取得する。
   * 同じ盤面のノードがキャッシュに存在する場合はそのノードを返す。
   * 最大ノード数に達している場合はnullptrを返す。
   * @param board 探索対象となる盤面情報
   * @param depth 現在の探索深さ
   * @return 探索ノードのポインタ
   */
  PnSearchNode* _getNode(const Board* board, int32_t depth);

  // PnSearchNodeクラスのexpand関数がPnSearchEngineクラスの_getNode関数を呼び出すため、
  // PnSearchNodeクラスのexpand関数をfriend関数として宣言する。
  friend bool PnSearchNode::expand(PnSearchEngine* engine);
};

}  // namespace deepshogi

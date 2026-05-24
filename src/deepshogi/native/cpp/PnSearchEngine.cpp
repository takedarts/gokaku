#include "PnSearchEngine.h"

#include <algorithm>
#include <set>

namespace deepshogi {

// 終端ノードを表す定数
static const PnSearchNode END_NODE = PnSearchNode();

/**
 * PN探索エンジンのオブジェクトを生成する。
 * @param nodeSize 探索の最大ノード数
 */
PnSearchEngine::PnSearchEngine(int32_t nodeSize)
    : _nodes(nodeSize),
      _nodeSize(nodeSize),
      _nodeCount(0),
      _nodeCache() {
}

/**
 * 詰み筋を探索して、着手手順を返す。
 * 詰み筋が見つからない場合は空の配列を返す。
 * @param board 盤面情報を保持するオブジェクト
 * @param depth 探索する深さ
 * @return 詰み筋の着手手順
 */
std::vector<Move> PnSearchEngine::getCheckmateMoves(const Board* board, int32_t depth) {
  // 探索深さは奇数でなければならない
  depth = std::max((depth % 2 == 0) ? depth + 1 : depth, 1);

  // ノードプールをリセットする
  _nodeCount = 0;
  _nodeCache.clear();

  // ルートノードを作成する
  PnSearchNode* root = _getNode(board, 0);

  // 探索を実行する
  std::vector<PnSearchNode*> parents;

  // 詰みが不詰みが見つかるまで探索を続ける
  // ただし、最大探索数に達した場合は探索を終了する
  while (root->getPn() != 0 && root->getDn() != 0) {
    // ルートノードから展開する末端ノードまで探索する
    PnSearchNode* node = root;
    PnSearchNode* next_node = node->getNextNode();

    parents.clear();

    // 末端ノードに到達するまで探索する
    while (next_node != nullptr) {
      // ループを検出するために現在のノードを親ノードのリストに追加する
      parents.push_back(node);

      // 次の探索ノードがいずれかの親ノードの同じノードか劣後ノードである場合は
      // 次の探索ノードを不詰みノードに置き換える
      if (std::any_of(parents.begin(), parents.end(), [next_node](PnSearchNode* parent) {
            return next_node->isLesserThanOrEqual(parent);
          })) {
        node->replaceChildNode(next_node, const_cast<PnSearchNode*>(&END_NODE));
        next_node = const_cast<PnSearchNode*>(&END_NODE);
      }

      // 次のノードに移動する
      node = next_node;
      next_node = node->getNextNode();
    }

    // 次の探索ノードが詰み/不詰みノードではない場合はノードを展開する
    if (node->getPn() != 0 && node->getDn() != 0) {
      // ノードを展開する
      // ノードの展開の失敗は最大探索ノード数に達したことを意味するため、
      // ノードの展開に失敗した場合は探索を終了する
      if (!node->expand(this)) {
        break;
      }

      // 末端ノードのPN/DN値を更新する
      node->update(depth);
    }

    // 親ノードのPN/DN値を更新する
    // 末端ノードが詰み/不詰みノードであっても親ノードのPN/DN値を更新する必要がある。
    // 親ノードのPN/DN値を更新することでルートノードのPN/DN値に末端ノードの結果を反映させる。
    for (auto it = parents.rbegin(); it != parents.rend(); it++) {
      (*it)->update(depth);
    }
  }

  // 詰み手順を取得する
  std::vector<Move> checkmate_moves;
  PnSearchNode* node = root;

  while (true) {
    auto [next_move, next_node] = node->getCheckmateNode();

    if (next_node == nullptr) {
      break;
    }

    checkmate_moves.push_back(next_move);
    node = next_node;
  }

  return checkmate_moves;
}

/**
 * 新しい探索ノードを取得する。
 * 同じ盤面のノードがキャッシュに存在する場合はそのノードを返す。
 * 最大ノード数に達している場合はnullptrを返す。
 * @param board 探索対象となる盤面情報
 * @param depth 現在の探索深さ
 * @return 探索ノードのポインタ
 */
PnSearchNode* PnSearchEngine::_getNode(const Board* board, int32_t depth) {
  // 最大ノード数に達している場合はnullptrを返す
  if (_nodeCount >= _nodeSize) {
    return nullptr;
  }

  // 盤面を表すノードのための変数を宣言する
  PnSearchNode* node = nullptr;

  // ノードキャッシュを確認する
  BoardHash node_key(board);
  auto it = _nodeCache.find(node_key);

  // キャッシュにノードが存在する場合
  if (it != _nodeCache.end()) {
    // キャッシュのノードを使用する
    node = it->second;
  }
  // キャッシュにノードが存在しない場合
  else {
    // ノードプールから新しいノードを取得する
    node = &_nodes[_nodeCount];

    _nodeCount += 1;

    // ノードを初期化する
    node->initialize(board, depth);

    // 新しいノードをキャッシュに登録する
    _nodeCache[node_key] = node;
  }

  return node;
}

}  // namespace deepshogi

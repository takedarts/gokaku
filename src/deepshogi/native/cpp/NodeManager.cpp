#include "NodeManager.h"

namespace deepshogi {

/**
 * ノードオブジェクトの管理クラスを作成する。
 * @param parameter ノードオブジェクトの設定パラメータ
 */
NodeManager::NodeManager(NodeParameter parameter)
    : _mutex(),
      _parameter(parameter),
      _nodes(),
      _poolNodes(),
      _usedNodes() {
}

/**
 * ノードオブジェクトを作成する。
 * 未使用のノードオブジェクトがあればそれを返し、なければ新規作成する。
 * @return ノードオブジェクト
 */
Node* NodeManager::createNode() {
  std::lock_guard<std::mutex> lock(_mutex);

  // ノードオブジェクトを作成する
  // 未使用のノードオブジェクトがあればそれを利用する
  Node* node;

  if (_poolNodes.empty()) {
    _nodes.emplace_back(std::make_unique<Node>(this, _parameter));
    node = _nodes.back().get();
  } else {
    node = _poolNodes.back();
    _poolNodes.pop_back();
  }

  // 使用中のノードオブジェクトとして登録する
  _usedNodes.insert(node);

  // ノードオブジェクトを返す
  return node;
}

/**
 * ノードオブジェクトを未使用状態にする。
 * @param node ノードオブジェクト
 */
void NodeManager::releaseNode(Node* node) {
  std::lock_guard<std::mutex> lock(_mutex);

  // 登録済みのノードオブジェクトでないなら何もしない
  if (_usedNodes.find(node) == _usedNodes.end()) {
    return;
  }

  // ノードオブジェクトをリセットして未使用状態にする
  _usedNodes.erase(node);
  _poolNodes.push_back(node);
}

/**
 * このノード管理オブジェクトのの情報を出力する。
 * @param os 出力先
 */
void NodeManager::print(std::ostream& os) {
  os << "NodeManager: nodes=" << _nodes.size();
  os << "(" << _usedNodes.size() << "/" << _poolNodes.size() << ")";
  os << std::endl;
}

}  // namespace deepshogi

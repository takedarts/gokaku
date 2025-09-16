#pragma once

#include <mutex>
#include <vector>

#include "Node.h"
#include "NodeParameter.h"

namespace deepshogi {

/**
 * ノードオブジェクトを管理するためのクラス。
 */
class NodeManager {
 public:
  /**
   * ノードオブジェクトの管理クラスを作成する。
   * @param parameter ノードオブジェクトの設定パラメータ
   */
  NodeManager(NodeParameter parameter);

  /**
   * ノードオブジェクトを作成する。
   * 未使用のノードオブジェクトがあればそれを返し、なければ新規作成する。
   * @return ノードオブジェクト
   */
  Node* createNode();

  /**
   * ノードオブジェクトを未使用状態にする。
   * @param node ノードオブジェクト
   */
  void releaseNode(Node* node);

  /**
   * このノード管理オブジェクトのの情報を出力する。
   * @param os 出力先
   */
  void print(std::ostream& os = std::cout);

 private:
  /**
   * 同期オブジェクト。
   */
  std::mutex _mutex;

  /**
   * ノード生成時に使用するパラメータ。
   */
  NodeParameter _parameter;

  /**
   * ノードオブジェクトの一覧。
   */
  std::vector<std::unique_ptr<Node>> _nodes;

  /**
   * 未使用のノードオブジェクトの一覧。
   */
  std::vector<Node*> _poolNodes;

  /**
   * 使用中のノードオブジェクトの一覧。
   */
  std::set<Node*> _usedNodes;
};

}  // namespace deepshogi

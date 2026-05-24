#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "MctsParameter.h"

namespace deepshogi {

// MCTSの探索ノードオブジェクトを管理するクラス。
// MctsNode.hでMctsNodeクラスが定義されている。
// 相互参照のため、MctsNodeクラスは前方宣言する。
class MctsNode;

/**
 * MCTSの探索ノードオブジェクトを管理するクラス。
 */
class MctsManager {
 private:
  friend class Player;

 public:
  /**
   * MCTSの探索ノード管理オブジェクトを作成する。
   * @param parameter MCTSの探索パラメータ
   */
  MctsManager(const MctsParameter& parameter);

  /**
   * MCTSの探索ノード管理オブジェクトを破棄する。
   */
  virtual ~MctsManager() = default;

  /**
   * ノードオブジェクトを作成する。
   * 未使用のノードオブジェクトがあればそれを返し、なければ新規作成する。
   * @return ノードオブジェクト
   */
  MctsNode* createNode();

  /**
   * ノードオブジェクトを未使用状態にする。
   * @param node ノードオブジェクト
   */
  void releaseNode(MctsNode* node);

  /**
   * 指定されたノードオブジェクトとそのすべての子ノードオブジェクトを未使用状態にする。
   * @param root ノードオブジェクト
   */
  void releaseTree(MctsNode* root);

  /**
   * MCTSの探索ノード管理オブジェクトの状態を文字列として取得する。
   * @return 状態を表す文字列
   */
  std::string toString();

  /**
   * MCTSの探索パラメータを取得する。
   * @return MCTSの探索パラメータ
   */
  inline const MctsParameter& getParameter() const {
    return _parameter;
  }

 private:
  /**
   * 同期オブジェクト。
   */
  std::mutex _mutex;

  /**
   * MCTSの探索パラメータ。
   */
  MctsParameter _parameter;

  /**
   * ノードオブジェクトの一覧。
   */
  std::vector<std::unique_ptr<MctsNode>> _nodes;

  /**
   * 未使用のノードオブジェクトの一覧。
   */
  std::vector<MctsNode*> _poolNodes;

  /**
   * 使用中のノードオブジェクトの一覧。
   */
  std::set<MctsNode*> _usedNodes;
};

}  // namespace deepshogi

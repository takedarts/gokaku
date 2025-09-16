#pragma once

#include <cstdint>
#include <iostream>
#include <queue>
#include <shared_mutex>

#include "Board.h"
#include "Config.h"
#include "Evaluator.h"
#include "Move.h"
#include "NodeParameter.h"
#include "NodeResult.h"
#include "Policy.h"

namespace deepshogi {
class NodeManager;

/**
 * 探索ノードクラス。
 */
class Node {
 public:
  /**
   * 探索ノードオブジェクトを作成する。
   * @param manager ノード管理オブジェクト
   * @param parameter ノード生成パラメータ
   */
  Node(NodeManager* manager, const NodeParameter& parameter);

  /**
   * SFEN形式で指定された初期盤面ノードとして設定する。
   * @param sfen SFEN形式の盤面
   */
  void initialize(const std::string sfen);

  /**
   * 探索ノードを評価する。
   * @param equally 探索回数を均等にする場合はtrue
   * @param width 探索幅(0の場合は探索幅を自動で調整する)
   * @param useUcb1 UCB1を使用する場合はtrue・PUCBを使用する場合はfalse
   * @param searchCheckMove 詰み手筋を探索する場合はtrue
   * @param temperature 探索の温度パラメータ
   * @param noise 探索のガンベルノイズの強さ
   * @return 評価結果
   */
  NodeResult evaluate(
      bool equally, int32_t width, bool useUcb1, bool searchCheckMove,
      float temperature, float noise);

  /**
   * 探索ノードの評価値を更新する。
   * @param value 評価値
   */
  void updateValue(float value);

  /**
   * 探索ノードの評価値をキャンセルする。
   * @param value 評価値
   */
  void cancelValue(float value);

  /**
   * PolicyNetworkの評価値が最も高い候補手を取得する。
   * @return 候補手
   */
  Move getPolicyMove();

  /**
   * 着手を取得する。
   * @return 着手
   */
  Move getMove();

  /**
   * 次の手番を取得する。
   * @return 手番
   */
  int32_t getColor();

  /**
   * このノードの予想着手確率を取得する。
   * @return 予想着手確率
   */
  float getPolicy();

  /**
   * 子ノードの一覧を取得する。
   * @return ノードオブジェクトの一覧
   */
  std::vector<Node*> getChildren();

  /**
   * 指定した着手を実行したときのノードオブジェクトを取得する。
   * ノードオブジェクトが存在しない場合は新しく作成したオブジェクトを返す。
   * 作成したノードオブジェクトはこのノードオブジェクトの子ノードとしては登録されない。
   * @param move 着手
   * @return ノードオブジェクトへのポインタ
   */
  Node* getChild(const Move& move);

  /**
   * このノードの詰み手筋を取得する。
   * 積み手筋が見つかっていない場合はMOVE_PASSを返す。
   * @return 詰み手筋
   */
  Move getCheckMove() const;

  /**
   * このノードの探索回数を取得する。
   * @return 探索回数
   */
  int32_t getVisits();

  /**
   * プレイアウト数を取得する。
   * @return プレイアウト数
   */
  int32_t getPlayouts();

  /**
   * プレイアウト数を設定する。
   * @param playouts プレイアウト数
   */
  void setPlayouts(int32_t playouts);

  /**
   * このノードの評価値を取得する。
   * @return 評価値
   */
  float getValue();

  /**
   * このノードの評価値の信頼区間の下限を取得する。
   * @return 信頼区間の下限
   */
  float getValueLCB();

  /**
   * PUCBに基づいてこのノードの優先度を取得する。
   * @param totalVisits 探索回数の合計
   * @return 優先度
   */
  float getPriorityByPUCB(int32_t totalVisits);

  /**
   * UCB1に基づいてこのノードの優先度を取得する。
   * @param totalVisits 探索回数の合計
   * @return 優先度
   */
  float getPriorityByUCB1(int32_t totalVisits);

  /**
   * このノードの予想進行を取得する。
   * @return 予想進行
   */
  std::vector<Move> getVariations();

  /**
   * 指定された盤面オブジェクトに盤面の状態をコピーする。
   * @param board 盤面オブジェクト
   */
  void copyBoardTo(Board* board);

  /**
   * このノードの情報を出力する。
   * @param os 出力先
   */
  void print(std::ostream& os = std::cout);

 private:
  /**
   * 評価同期用のミューテックス。
   */
  std::shared_mutex _evalMutex;

  /**
   * 値同期用のミューテックス。
   */
  std::shared_mutex _valueMutex;

  /**
   * ノード管理オブジェクト。
   */
  NodeManager* _manager;

  /**
   * このノードで評価する盤面。
   */
  Board _board;

  /**
   * 着手。
   */
  Move _move;

  /**
   * 予想着手確率。
   */
  float _policy;

  /**
   * 盤面を評価するオブジェクト。
   */
  Evaluator _evaluator;

  /**
   * 詰み手筋の探索深さ。
   */
  int32_t _checkSearchDepth;

  /**
   * 詰み手筋の探索ノード数。
   */
  int32_t _checkSearchNode;

  /**
   * 子ノードの一覧。
   */
  std::unordered_map<int32_t, Node*> _children;

  /**
   * 次の着手確率の一覧。
   */
  std::vector<Policy> _childPolicies;

  /**
   * 子ノードへの登録を待機している候補手の一覧。
   */
  std::queue<Policy> _waitingQueue;

  /**
   * 子ノードへの登録を待機している候補手のセット。
   */
  std::set<int32_t> _waitingSet;

  /**
   * 詰み手筋の着手。
   */
  Move _checkMove;

  /**
   * 浅い（3手）詰み手筋探索を実行済みならtrue。
   */
  bool _checkMoveShallowSearched;

  /**
   * 深い（DfPn）詰み手筋探索を実行済みならtrue。
   */
  bool _checkMoveDeepSearched;

  /**
   * 探索回数。
   */
  int32_t _visits;

  /**
   * プレイアウト数。
   */
  int32_t _playouts;

  /**
   * 予想勝率。
   */
  float _value;

  /**
   * 予想勝率と予想目数の加算回数。
   */
  int32_t _count;

  /**
   * このノードの盤面評価を実行する。
   * @param searchCheckMove 詰み手筋を探索する場合はtrue
   */
  void _evaluateBoard(bool searchCheckMove);

  /**
   * このノードの状態を評価して、評価結果を返す。
   * @param equally 探索回数を均等にする場合はtrue
   * @param width 探索幅(0の場合は探索幅を自動で調整する)
   * @param useUcb1 UCB1を使用する場合はtrue・PUCBを使用する場合はfalse
   * @param temperature 探索の温度パラメータ
   * @param noise 探索のガンベルノイズの強さ
   * @return 評価結果
   */
  NodeResult _evaluateNode(
      bool equally, int32_t width, bool useUcb1, float temperature, float noise);

  /**
   * ノードの評価情報を初期化する。
   */
  void _reset();

  /**
   * 指定されたノードの継続ノードとしての値を設定する。
   * @param prevNode 前のノード
   * @param move 着手
   * @param policy 予想着手確率
   */
  void _setAsNextNode(Node* prevNode, const Move& move, float policy);
};

}  // namespace deepshogi

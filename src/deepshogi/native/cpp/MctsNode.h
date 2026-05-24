#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

#include "Board.h"
#include "MctsManager.h"
#include "MctsParameter.h"
#include "MctsPolicy.h"
#include "MctsValue.h"
#include "Move.h"
#include "PnSearchEngine.h"

namespace deepshogi {

/**
 * MCTSの探索ノードの状態を管理するクラス。
 */
class MctsNode {
 public:
  /**
   * MCTSの探索ノードオブジェクトを生成する。
   * @param manager ノード管理オブジェクト
   */
  MctsNode(MctsManager* manager);

  /**
   * SFEN形式で指定された初期盤面ノードとして設定する。
   * @param sfen SFEN形式の盤面
   */
  void initialize(const std::string sfen);

  /**
   * 指定された推論結果をこのノードの評価値と予想着手確率の一覧に適用する。
   * @param value 盤面評価値
   * @param policies 次の着手の予想確率の一覧
   */
  void applyInferenceResult(
      float value, const std::vector<std::pair<Move, float>>& policies);

  /**
   * このノードのMCTS評価値を更新する。
   * @param mctsValue MCTS評価値
   */
  void updateMctsValue(float mctsValue);

  /**
   * 次に評価するノードオブジェクトを取得する。
   * 次に評価するノードが存在しない場合はnullptrを返す。
   * 次に評価するノードが存在しない条件は以下の通り
   * - 盤面評価が行われていない
   * - 合法手が存在しない
   * - 詰み探索によって詰み筋の着手手順が発見されている
   * - ルートノード以外であり、入玉宣言が可能な状態か引き分けて手数に達している
   * @param equally 探索回数を均等にする場合はtrue
   * @param width 探索幅(0の場合は探索幅を自動で調整する)
   * @param temperature 探索の温度パラメータ
   * @param noise 探索のガンベルノイズの強さ
   * @return 次に評価するノードオブジェクト
   */
  MctsNode* pickupNextNode(bool equally, int32_t width, float temperature, float noise);

  /**
   * 詰み探索を行う。
   * @param engine 詰み探索エンジン
   * @param depth 詰み探索の探索深さ
   */
  void searchCheckmateMoves(PnSearchEngine* engine, int32_t depth);

  /**
   * このノードをルートノードとして設定する。
   * この関数では以下の処理を行う。
   * - 親ノードを削除する
   * - 評価済みであり、合法手が存在しているがpolicyに登録されている手が空の場合
   *   - このノードの子ノードをすべて削除する
   *   - このノードの評価と統計情報を未評価状態にする
   */
  void setAsRootNode();

  /**
   * このノードの盤面評価が行われているならばtrueを返す。
   * @return 盤面評価が行われているならばtrue
   */
  bool isEvaluated();

  /**
   * このノードの詰み探索が行われているならばtrueを返す。
   * @return 詰み探索が行われているならばtrue
   */
  bool isCheckmateSearched();

  /**
   * このノードの盤面評価値を返す。
   * @return 盤面評価値
   */
  float getNodeValue();

  /**
   * このノードの次の着手の予想確率の一覧を返す。
   * @return 次の着手の予想確率の一覧
   */
  std::vector<MctsPolicy> getPolicies();

  /**
   * 親ノードを返す。
   * @return 親ノード
   */
  MctsNode* getParent();

  /**
   * 子ノードの一覧を返す。
   * @return 子ノードの一覧
   */
  std::vector<MctsNode*> getChildren();

  /**
   * 指定した着手を実行したときのノードオブジェクトを取得する。
   * ノードオブジェクトが存在しない場合は新しく作成したオブジェクトを返す。
   * 作成したノードオブジェクトはこのノードオブジェクトの子ノードとしては登録されない。
   * @param move 着手
   * @return ノードオブジェクトへのポインタ
   */
  MctsNode* getChild(const Move& move);

  /**
   * 指定した着手を実行したときのノードオブジェクトを子ノードの一覧から削除する。
   * @param move 着手
   */
  void removeChild(const Move& move);

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
   * このノードのMCTS評価値を取得する。
   * @return MCTS評価値
   */
  float getMctsValue();

  /**
   * このノードの評価値の信頼区間の下限を取得する。
   * @return 信頼区間の下限
   */
  float getMctsValueLCB();

  /**
   * PUCBに基づいてこのノードの優先度を取得する。
   * @param totalVisits 探索回数の合計
   * @return 優先度
   */
  float getPriorityByPUCB(int32_t totalVisits);

  /**
   * このノードの詰み手筋を取得する。
   * 詰み手筋が見つかっていない場合は空の配列を返す。
   * @return 詰み手筋
   */
  std::vector<Move> getCheckmateMoves();

  /**
   * このノードの予想進行を取得する。
   * @return 予想進行
   */
  std::vector<Move> getVariations();

  /**
   * PolicyNetworkの評価値が最も高い候補手を取得する。
   * @return 候補手
   */
  Move getPolicyMove();

  /**
   * このノードの盤面オブジェクトを返す。
   * @return このノードの盤面オブジェクト
   */
  inline const Board& getBoard() const {
    return _board;
  }

  /**
   * 直前の着手を返す。
   * このノードが初期盤面ノードである場合は、無効な着手オブジェクトを返す。
   * @return 直前の着手
   */
  inline Move getMove() const {
    return _move;
  }

  /**
   * このノードの直前の着手の予想着手確率を返す。
   * @return 予想着手確率
   */
  inline float getProbability() const {
    return _probability;
  }

 private:
  /**
   * 同期するためのミューテックス。
   */
  std::shared_mutex _mutex;

  /**
   * 評価の完了を待機するための条件変数。
   */
  std::condition_variable_any _condition;

  /**
   * ノード管理オブジェクト。
   */
  MctsManager* _manager;

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
  float _probability;

  /**
   * このノードが親ノードの最初の子ノードであるならばtrue。
   */
  bool _firstChild;

  /**
   * このノードの評価が実行中であるならばtrue。
   */
  bool _evaluating;

  /**
   * このノードが評価済みであるならばtrue。
   */
  bool _evaluated;

  /**
   * このノードの盤面評価値。
   */
  float _nodeValue;

  /**
   * このノードの次の着手の予想確率の一覧。
   * このノードの盤面評価値が更新されたときに、この一覧も更新される。
   * ただし、このノードが終局状態のときは、この一覧は空になる。
   * 以下のいずれかの状態であれば終局状態とする。
   * - 合法手が存在しない（負け）
   * - 入玉宣言が可能である（勝ち）
   * - 最長手数に達している（引き分け）
   * - 詰み手筋の着手手順が発見されている（勝ち）
   */
  std::vector<MctsPolicy> _policies;

  /**
   * 親ノード。
   */
  MctsNode* _parent;

  /**
   * 子ノードの一覧。
   */
  std::map<int32_t, MctsNode*> _children;

  /**
   * 探索回数。
   */
  int32_t _visits;

  /**
   * プレイアウト数。
   */
  std::atomic<int32_t> _playouts;

  /**
   * MCTS評価値。
   */
  MctsValue _mctsValue;

  /**
   * 詰み手筋。
   */
  std::vector<Move> _checkmateMoves;

  /**
   * 深い詰み探索が実行されているならばtrue。
   */
  bool _checkmateMoveSearched;

  /**
   * 子ノードへの登録を待機している候補手の一覧。
   */
  std::queue<MctsPolicy> _waitingPolicies;

  /**
   * 子ノードへの登録を待機している候補手のセット。
   */
  std::set<int32_t> _waitingMoves;

  /**
   * このノードの盤面オブジェクト以外の状態を初期化する。
   */
  void _resetNode();

  /**
   * 次に評価するノードオブジェクトを取得する。
   * この関数はこのノードが評価済みであることを前提としている。
   * @param equally 探索回数を均等にする場合はtrue
   * @param width 探索幅(0の場合は探索幅を自動で調整する)
   * @param temperature 探索の温度パラメータ
   * @param noise 探索のガンベルノイズの強さ
   * @return 次に評価するノードオブジェクト
   */
  MctsNode* _pickupNextNode(bool equally, int32_t width, float temperature, float noise);
};

}  // namespace deepshogi

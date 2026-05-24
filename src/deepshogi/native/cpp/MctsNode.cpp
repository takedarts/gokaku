#include "MctsNode.h"

#include <algorithm>
#include <random>

namespace deepshogi {

// 探索時のPolicyサンプリングとガンベルノイズで使用する
// スレッドローカル乱数生成器
thread_local static std::random_device random_seed_gen;
thread_local static std::default_random_engine random_engine(random_seed_gen());

/**
 * MCTSの探索ノードオブジェクトを生成する。
 * @param manager ノード管理オブジェクト
 */
MctsNode::MctsNode(MctsManager* manager)
    : _mutex(),
      _condition(),
      _manager(manager),
      _board(
          manager->getParameter().getNyugyokuScoreBlack(),
          manager->getParameter().getNyugyokuScoreWhite(),
          manager->getParameter().getDrawTurn()),
      _move(MOVE_INVALID),
      _probability(0.0f),
      _firstChild(false),
      _evaluating(false),
      _evaluated(false),
      _nodeValue(0.0f),
      _policies(),
      _parent(nullptr),
      _children(),
      _visits(0),
      _playouts(0),
      _mctsValue(),
      _checkmateMoves(),
      _checkmateMoveSearched(false),
      _waitingPolicies(),
      _waitingMoves() {
}

/**
 * SFEN形式で指定された初期盤面ノードとして設定する。
 * @param sfen SFEN形式の盤面
 */
void MctsNode::initialize(const std::string sfen) {
  std::unique_lock<std::shared_mutex> node_lock(_mutex);

  _resetNode();
  _board.initialize(sfen);
  _move = MOVE_INVALID;
}

/**
 * 指定された推論結果をこのノードの評価値と予想着手確率の一覧に適用する。
 * @param value 盤面評価値
 * @param policies 次の着手の予想確率の一覧
 */
void MctsNode::applyInferenceResult(
    float value, const std::vector<std::pair<Move, float>>& policies) {
  std::unique_lock<std::shared_mutex> lock(_mutex);

  // 詰み手順が見つかっていない場合は盤面評価値と予想着手確率の一覧を更新する
  if (_checkmateMoves.empty()) {
    // 盤面評価値を更新する
    _nodeValue = value;

    // 予想着手確率の一覧を更新する
    _policies.clear();

    for (const auto& policy : policies) {
      _policies.emplace_back(policy.first, policy.second);
    }
  }

  // 評価済みとする
  _evaluating = false;
  _evaluated = true;

  // 評価の完了を待機しているスレッドに通知する
  _condition.notify_all();
}

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
 * @param rootNode このノードがルートノードである場合はtrue
 * @return 次に評価するノードオブジェクト
 */
MctsNode* MctsNode::pickupNextNode(bool equally, int32_t width, float temperature, float noise) {
  std::unique_lock<std::shared_mutex> lock(_mutex);

  // このノードに到達した探索回数を増やす
  _visits += 1;

  // このノードが評価中ならば、評価の完了を待機する
  if (_evaluating) {
    _condition.wait(lock, [this] { return !_evaluating; });
  }

  // すでに評価済みの場合は次に評価するノードを返す
  if (_evaluated) {
    // 次に評価するノードが存在する場合は次に評価するノードを返す
    // プレイアウト数は末端ノードによって増やされる
    if (!_policies.empty()) {
      return _pickupNextNode(equally, width, temperature, noise);
    }
    // そうでない場合はプレイアウト数だけ増やしてこのノードを返す
    else {
      MctsNode* current_node = this;

      while (current_node != nullptr) {
        current_node->_playouts.fetch_add(1, std::memory_order_relaxed);
        current_node = current_node->_parent;
      }

      return nullptr;
    }
  }

  // 未評価ノードに到達したのでプレイアウト数を増やす
  _playouts.fetch_add(1, std::memory_order_relaxed);

  if (!_firstChild) {
    MctsNode* parent = _parent;

    while (parent) {
      parent->_playouts.fetch_add(1, std::memory_order_relaxed);
      parent = parent->_parent;
    }
  }

  // 合法手が存在しないならば、このノードの状態を終局状態（負け）にする
  if (_board.getLegalMoves(true, false).empty()) {
    _nodeValue = static_cast<float>(OPPOSITE_COLOR(_board.getColor()));
    _evaluated = true;
    _policies.clear();

    return nullptr;
  }

  // ルートノード以外で、入玉宣言が可能である場合、このノードの状態を終局状態（勝ち）にする
  if (_parent != nullptr && _board.isNyugyoku(_board.getColor())) {
    _nodeValue = static_cast<float>(_board.getColor());
    _evaluated = true;
    _policies.clear();

    return nullptr;
  }

  // ルートノード以外で、最長手数に達している場合、このノードの状態を終局状態（引き分け）にする
  if (_parent != nullptr && _board.getTurn() >= _board.getDrawTurn()) {
    _nodeValue = 0.0f;
    _evaluated = true;
    _policies.clear();

    return nullptr;
  }

  // 5手詰みの着手手順を探す
  int32_t remain_turn = _board.getDrawTurn() - _board.getTurn() + 1;
  int32_t search_depth = std::min(5, remain_turn);

  _checkmateMoves = _board.getCheckmateMoves(search_depth);

  // 詰み筋の着手手順が発見されたならば、このノードの状態を終局状態（勝ち）にする
  if (!_checkmateMoves.empty()) {
    _nodeValue = static_cast<float>(_board.getColor());
    _evaluated = true;
    _policies.clear();

    return nullptr;
  }

  // このノードの状態を評価中にする
  _evaluating = true;

  return nullptr;
}

/**
 * 詰み探索を行う。
 * @param engine 詰み探索エンジン
 * @param depth 詰み探索の探索深さ
 */
void MctsNode::searchCheckmateMoves(PnSearchEngine* engine, int32_t depth) {
  {
    std::unique_lock<std::shared_mutex> lock(_mutex);

    // すでに詰み探索が実行されている場合は何もしない
    if (_checkmateMoveSearched) {
      return;
    }

    // 探索済みのフラグを設定する
    _checkmateMoveSearched = true;

    // すでに詰み筋の着手手順が発見されている場合は何もしない
    if (!_checkmateMoves.empty()) {
      return;
    }
  }

  // 詰み探索を実行する
  std::vector<Move> checkmateMoves = engine->getCheckmateMoves(&_board, depth);

  // 詰み筋の着手手順が発見された場合は、詰み筋の着手手順を保存する
  // 評価値を次の手で勝ちになるように更新して、候補手の一覧を空にする
  if (!checkmateMoves.empty()) {
    std::unique_lock<std::shared_mutex> lock(_mutex);

    // 探索結果を保存する
    _checkmateMoves = checkmateMoves;

    // 評価値を次の手で勝ちになるように更新する
    _nodeValue = static_cast<float>(_board.getColor());
    _mctsValue.setValue(_nodeValue);

    // 候補手の一覧を空にする
    _policies.clear();
    _waitingPolicies = std::queue<MctsPolicy>();
    _waitingMoves.clear();
  }
}

/**
 * このノードのMCTS評価値を更新する。
 * @param mctsValue MCTS評価値
 */
void MctsNode::updateMctsValue(float mctsValue) {
  _mctsValue.update(mctsValue);
}

/**
 * このノードをルートノードとして設定する。
 * この関数では以下の処理を行う。
 * - 親ノードを削除する
 * - 評価済みであり、合法手が存在しているがpolicyに登録されている手が空の場合
 *   子ノードをすべて削除して、評価と統計情報をリセットする
 */
void MctsNode::setAsRootNode() {
  std::unique_lock<std::shared_mutex> lock(_mutex);

  // 親ノードを削除する
  _parent = nullptr;

  // 着手確率を1.0にする
  _probability = 1.0f;

  // 評価済みであり、合法手が存在しているがpolicyに登録されている手が空の場合
  if (_evaluated && !_board.getLegalMoves(true, false).empty() && _policies.empty()) {
    // このノードの子ノードをすべて削除する
    for (const auto& child : _children) {
      _manager->releaseTree(child.second);
    }

    // このノードの評価と統計情報を未評価状態にする
    Move move = _move;
    float probability = _probability;

    _resetNode();
    _move = move;
    _probability = probability;
  }
}

/**
 * このノードの盤面評価が行われているならばtrueを返す。
 * @return 盤面評価が行われているならばtrue
 */
bool MctsNode::isEvaluated() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _evaluated;
}

/**
 * このノードの詰み探索が行われているならばtrueを返す。
 * @return 詰み探索が行われているならばtrue
 */
bool MctsNode::isCheckmateSearched() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _checkmateMoveSearched || !_checkmateMoves.empty();
}

/**
 * このノードの盤面評価値を返す。
 * @return 盤面評価値
 */
float MctsNode::getNodeValue() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _nodeValue;
}

/**
 * このノードの次の着手の予想確率の一覧を返す。
 * @return 次の着手の予想確率の一覧
 */
std::vector<MctsPolicy> MctsNode::getPolicies() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _policies;
}

/**
 * 親ノードを返す。
 * @return 親ノード
 */
MctsNode* MctsNode::getParent() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _parent;
}

/**
 * 子ノードの一覧を返す。
 * @return 子ノードの一覧
 */
std::vector<MctsNode*> MctsNode::getChildren() {
  std::shared_lock<std::shared_mutex> lock(_mutex);

  std::vector<MctsNode*> children;

  for (const auto& child : _children) {
    children.push_back(child.second);
  }

  return children;
}

/**
 * 指定した着手を実行したときのノードオブジェクトを取得する。
 * ノードオブジェクトが存在しない場合は新しく作成したオブジェクトを返す。
 * 作成したノードオブジェクトはこのノードオブジェクトの子ノードとしては登録されない。
 * @param move 着手
 * @return ノードオブジェクトへのポインタ
 */
MctsNode* MctsNode::getChild(const Move& move) {
  std::unique_lock<std::shared_mutex> lock(_mutex);

  // 子ノードが存在する場合はそのノードを返す
  auto it = _children.find(move.getValue());

  if (it != _children.end()) {
    return it->second;
  }

  // 子ノードが存在しない場合は新しくノードオブジェクトを作成して返す
  // 作成したノードオブジェクトはこのノードオブジェクトの子ノードとしては登録しない
  MctsNode* child = _manager->createNode();

  child->_resetNode();
  child->_board.copyFrom(&_board);
  child->_board.play(move);
  child->_move = move;
  child->_probability = 0.0f;

  return child;
}

/**
 * 指定した着手を実行したときのノードオブジェクトを子ノードの一覧から削除する。
 * @param move 着手
 */
void MctsNode::removeChild(const Move& move) {
  std::unique_lock<std::shared_mutex> lock(_mutex);
  _children.erase(move.getValue());
}

/**
 * このノードの探索回数を取得する。
 * @return 探索回数
 */
int32_t MctsNode::getVisits() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _visits;
}

/**
 * プレイアウト数を取得する。
 * @return プレイアウト数
 */
int32_t MctsNode::getPlayouts() {
  return _playouts.load(std::memory_order_relaxed);
}

/**
 * このノードのMCTS評価値を取得する。
 * @return MCTS評価値
 */
float MctsNode::getMctsValue() {
  return _mctsValue.getValue(_nodeValue);
}

/**
 * このノードの評価値の信頼区間の下限を取得する。
 * @return 信頼区間の下限
 */
float MctsNode::getMctsValueLCB() {
  return _mctsValue.getValueLCB(OPPOSITE_COLOR(_board.getColor()), _nodeValue);
}

/**
 * PUCBに基づいてこのノードの優先度を取得する。
 * @param totalVisits 探索回数の合計
 * @return 優先度
 */
float MctsNode::getPriorityByPUCB(int32_t totalVisits) {
  std::shared_lock<std::shared_mutex> lock(_mutex);

  float pucb_constant_base = _manager->getParameter().getPucbConstantBase();
  float pucb_constant_init = _manager->getParameter().getPucbConstantInit();
  float value = _mctsValue.getValue(_nodeValue) * OPPOSITE_COLOR(_board.getColor());
  float c_pucb_inc = std::log((1 + totalVisits + pucb_constant_base) / pucb_constant_base);
  float c_pucb = pucb_constant_init * (1.0f + c_pucb_inc);
  float ucb = _probability * std::sqrt((float)totalVisits) / (1 + _visits);

  return value + c_pucb * ucb;
}

/**
 * このノードの詰み手筋を取得する。
 * 詰み手筋が見つかっていない場合は空の配列を返す。
 * @return 詰み手筋
 */
std::vector<Move> MctsNode::getCheckmateMoves() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _checkmateMoves;
}

/**
 * このノードの予想進行を取得する。
 * @return 予想進行
 */
std::vector<Move> MctsNode::getVariations() {
  std::vector<Move> variations;
  MctsNode* max_child = nullptr;

  {
    // 同期のためにロックを取得する
    std::shared_lock<std::shared_mutex> lock(_mutex);

    // このノードの着手を予想進行に追加する
    variations.push_back(_move);

    // 評価値のLCBが最大の子ノードを辿る手順で予想進行を作成する
    float max_value_lcb = -std::numeric_limits<float>::infinity();

    for (auto child : _children) {
      float child_value_lcb = child.second->getMctsValueLCB() * _board.getColor();

      if (child_value_lcb > max_value_lcb) {
        max_value_lcb = child_value_lcb;
        max_child = child.second;
      }
    }
  }

  // 最大の子ノードが存在する場合は、その子ノードの予想進行を予想進行に追加する
  if (max_child != nullptr) {
    std::vector<Move> child_variations = max_child->getVariations();
    variations.insert(variations.end(), child_variations.begin(), child_variations.end());
  }

  return variations;
}

/**
 * PolicyNetworkの評価値が最も高い候補手を取得する。
 * @return 候補手
 */
Move MctsNode::getPolicyMove() {
  std::shared_lock<std::shared_mutex> lock(_mutex);

  // 詰みの手筋が見つかっている場合はその手を返す
  if (!_checkmateMoves.empty()) {
    return _checkmateMoves[0];
  }

  // 候補手がない場合はパスを返す
  if (_policies.empty()) {
    return MOVE_INVALID;
  }

  // 最も着手確率が高い候補手を取得する
  MctsPolicy max_policy = _policies[0];

  for (MctsPolicy policy : _policies) {
    if (max_policy.getProbability() < policy.getProbability()) {
      max_policy = policy;
    }
  }

  // 最も着手確率が高い候補手を返す
  return max_policy.getMove();
}

/**
 * このノードの盤面オブジェクト以外の状態を初期化する。
 */
void MctsNode::_resetNode() {
  _move = MOVE_INVALID;
  _probability = 0.0f;
  _firstChild = false;

  _evaluating = false;
  _evaluated = false;
  _nodeValue = 0.0f;
  _policies.clear();

  _parent = nullptr;
  _children.clear();

  _visits = 0;
  _playouts.store(0, std::memory_order_relaxed);
  _mctsValue.reset();

  _checkmateMoves.clear();
  _checkmateMoveSearched = false;

  _waitingPolicies = std::queue<MctsPolicy>();
  _waitingMoves.clear();
}

/**
 * 次に評価するノードオブジェクトを取得する。
 * この関数はこのノードが評価済みであることを前提としている。
 * @param equally 探索回数を均等にする場合はtrue
 * @param width 探索幅(0の場合は探索幅を自動で調整する)
 * @param temperature 探索の温度パラメータ
 * @param noise 探索のガンベルノイズの強さ
 * @return 次に評価するノードオブジェクト
 */
MctsNode* MctsNode::_pickupNextNode(bool equally, int32_t width, float temperature, float noise) {
  // Policyの候補が残っていて探索幅に余裕がある場合は、新しい着手を展開候補にする
  int32_t children_size = (int32_t)(_children.size() + _waitingMoves.size());

  if (children_size < _policies.size() && (width < 1 || children_size < width)) {
    int32_t max_index = 0;
    int max_priority_type = 0;
    float max_priority = 0.0f;

    // 温度パラメータを計算する
    float win_chance = _mctsValue.getValue(_nodeValue) * _board.getColor() * 0.5f + 0.5f;
    float temperature_power =
        win_chance + (1.0f / std::max(temperature, 1e-3f)) * (1 - win_chance);

    // ガンベルノイズの生成オブジェクトを作成する
    // 子ノードの数が4以下の場合はノイズを加えない
    float noise_scale = (children_size <= 4) ? 0.0f : noise;
    std::extreme_value_distribution<float> noise_dist(0.0f, noise_scale);

    // 予測確率、温度、ガンベルノイズ、未展開優先の条件から次の候補を選ぶ
    for (int i = 0; i < _policies.size(); i++) {
      MctsPolicy& policy = _policies[i];
      float probability = policy.getProbability();

      // 温度パラメータを反映させる
      probability = std::pow(probability, temperature_power);

      // ガンベルノイズを加える
      // ノイズを加算する対象はロジットとなるため、確率に対してはe^noiseを乗算する
      probability *= std::exp(noise_dist(random_engine));

      // 優先度を計算する
      int32_t priority_type = 1;
      float priority = probability / (policy.getVisits() + 1);

      // 探索回数を均等にする設定となっている場合は登録済みの候補手の優先度を下げる
      if (equally) {
        int32_t policy_index = policy.getMove().getValue();

        if (_children.find(policy_index) != _children.end() ||
            _waitingMoves.find(policy_index) != _waitingMoves.end()) {
          priority_type = 0;
        }
      }

      // 優先度の高い候補手を残す
      if (priority_type > max_priority_type ||
          (priority_type == max_priority_type && priority > max_priority)) {
        max_index = i;
        max_priority_type = priority_type;
        max_priority = priority;
      }
    }

    // 評価に追加する候補手が未登録状態であれば新たに待機リストに登録する
    MctsPolicy& max_policy = _policies[max_index];
    int32_t max_policy_index = max_policy.getMove().getValue();

    if (_children.find(max_policy_index) == _children.end() &&
        _waitingMoves.find(max_policy_index) == _waitingMoves.end()) {
      // 次の盤面を作成して、親ノードのいずれかの劣後盤面であるかを確認する
      bool lesser_board = false;
      MctsNode* parent = _parent;
      Board next_board;

      next_board.copyFrom(&_board);
      next_board.play(max_policy.getMove());

      while (parent != nullptr) {
        if (next_board.isLesserThan(parent->_board, _board.getColor())) {
          lesser_board = true;
          break;
        }

        parent = parent->_parent;
      }

      // 劣後盤面であるなら、候補手から削除して終了する
      if (lesser_board) {
        _policies.erase(_policies.begin() + max_index);

        return nullptr;
      }

      // 評価に追加する候補手を待機リストに登録する
      _waitingPolicies.push(max_policy);
      _waitingMoves.insert(max_policy_index);
    }

    // 訪問数を増やす
    _policies[max_index].incrementVisits();
  }

  // 探索幅が指定されていない場合と子ノードの数が指定された探索幅に達していない場合、
  // 待機リストに候補手が存在する場合は新しい子ノードを作成して次の探索先として返す
  if (_waitingPolicies.size() > 0 && (width <= 0 || _children.size() < width)) {
    // 最初に登録された待機中の候補手を取得する
    MctsPolicy policy = _waitingPolicies.front();
    int32_t policy_index = policy.getMove().getValue();

    _waitingPolicies.pop();
    _waitingMoves.erase(policy_index);

    // 未登録の候補手であれば新しい子ノードを作成して次の探索先として返す
    // 暫定的にノードの評価値には最低評価値を設定する
    if (_children.find(policy_index) == _children.end()) {
      MctsNode* node = _manager->createNode();

      node->_resetNode();
      node->_board.copyFrom(&_board);
      node->_board.play(policy.getMove());
      node->_move = policy.getMove();
      node->_probability = policy.getProbability();
      node->_nodeValue = static_cast<float>(OPPOSITE_COLOR(_board.getColor()));
      node->_parent = this;
      node->_firstChild = (_children.size() == 0);
      _children[policy_index] = node;

      return node;
    }
  }

  // 探索対象とする子ノードの一覧を作成する
  std::vector<std::pair<MctsNode*, float>> children;

  for (std::pair<int32_t, MctsNode*> child : _children) {
    children.push_back(std::make_pair(
        child.second, child.second->getMctsValueLCB() * _board.getColor()));
  }

  // 探索幅が指定されている場合は探索対象とする子ノードの数を制限する
  if (width > 0 && children.size() > width) {
    std::sort(children.begin(), children.end(), [](auto a, auto b) {
      return a.second > b.second;
    });

    children.resize(width);
  }

  // 最も優先度が高いノードを次の探索先として返す
  MctsNode* max_node = children[0].first;
  float max_priority = -1.0;

  for (std::pair<MctsNode*, float> child : children) {
    // 優先度を計算する
    float priority;

    // 探索回数を均等にする設定となっている場合は、
    // 訪問回数に基づいて優先度を計算する（訪問回数が同じならば評価値を考慮する）
    if (equally) {
      float visits = (float)child.first->getVisits();
      float value = child.first->getMctsValue() * _board.getColor();
      priority = 1.0f / (visits + 1 - value * 0.5f);
    }
    // そうでない場合はPUCBに基づいて優先度を計算する
    else {
      priority = child.first->getPriorityByPUCB(_visits);
    }

    // 優先度の高いノードを残す
    if (max_priority < priority) {
      max_node = child.first;
      max_priority = priority;
    }
  }

  // 最も優先度の高いノードを次の探索先として返す
  return max_node;
}

}  // namespace deepshogi

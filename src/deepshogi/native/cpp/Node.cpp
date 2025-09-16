#include "Node.h"

#include <cmath>
#include <random>

#include "NodeManager.h"

namespace deepshogi {

// 乱数生成器
static std::random_device random_seed_gen;
static std::default_random_engine random_engine(random_seed_gen());

// 詰み手筋が見つからない場合の着手値
static const Move CHECK_MOVE_NOT_FOUND = Move(-1, -1, -1, -1, false);

/**
 * 探索ノードオブジェクトを作成する。
 * @param manager ノード管理オブジェクト
 * @param parameter ノード生成パラメータ
 */
Node::Node(NodeManager* manager, const NodeParameter& parameter)
    : _evalMutex(),
      _valueMutex(),
      _manager(manager),
      _board(
          parameter.getNyugyokuScoreBlack(),
          parameter.getNyugyokuScoreWhite(),
          parameter.getDrawSteps()),
      _move(MOVE_PASS),
      _policy(0.0f),
      _evaluator(parameter.getProcessor()),
      _checkSearchDepth(parameter.getCheckSearchDepth()),
      _checkSearchNode(parameter.getCheckSearchNode()),
      _children(),
      _childPolicies(),
      _waitingQueue(),
      _waitingSet(),
      _checkMove(CHECK_MOVE_NOT_FOUND),
      _checkMoveShallowSearched(false),
      _checkMoveDeepSearched(false),
      _visits(0),
      _playouts(0),
      _value(0.0f),
      _count(0) {
}

/**
 * SFEN形式で指定された初期盤面ノードとして設定する。
 * @param sfen SFEN形式の盤面
 */
void Node::initialize(const std::string sfen) {
  std::unique_lock<std::shared_mutex> lock(_evalMutex);

  _board.initializeWithSfen(sfen);
  _move = MOVE_PASS;
  _reset();
}

/**
 * 探索ノードを評価して、次に評価するノードオブジェクトを取得する。
 * 次に評価するノードオブジェクトが存在しない場合はnullptrを返す。
 * @param equally 探索回数を均等にする場合はtrue
 * @param width 探索幅(0の場合は探索幅を自動で調整する)
 * @param useUcb1 UCB1を使用する場合はtrue・PUCBを使用する場合はfalse
 * @param searchCheckMove 詰み手筋を探索する場合はtrue
 * @param temperature 探索の温度パラメータ
 * @param noise 探索のガンベルノイズの強さ
 * @return 次に評価するノードオブジェクト
 */
NodeResult Node::evaluate(
    bool equally, int32_t width, bool useUcb1, bool searchCheckMove,
    float temperature, float noise) {
  NodeResult result;
  Board search_board;

  {
    std::unique_lock<std::shared_mutex> lock(_evalMutex);

    // ノードの盤面評価を実行する
    _evaluateBoard(searchCheckMove);

    // 訪問数を増やす
    _visits += 1;

    // このノードの状態を評価する
    result = _evaluateNode(equally, width, useUcb1, temperature, noise);

    // 長手数の詰み探索を行わない場合は評価結果を返す
    if (!searchCheckMove || _checkMoveDeepSearched || _checkMove != CHECK_MOVE_NOT_FOUND) {
      return result;
    }

    // 長手数の詰み探索を実行した状態にする
    _checkMoveDeepSearched = true;

    // 詰み探索を行うための盤面を複製する
    // 完全に分離するためにsfen経由で複製する
    search_board.initializeWithSfen(_board.getSfen());
  }

  // 非同期で長手数の詰み探索を実行する
  Move move = search_board.searchCheckMove(_checkSearchDepth, _checkSearchNode);

  // 詰み探索の探索結果を保存する
  {
    std::unique_lock<std::shared_mutex> lock(_evalMutex);

    if (move != MOVE_PASS) {
      _checkMove = move;
    } else {
      _checkMove = CHECK_MOVE_NOT_FOUND;
    }
  }

  return result;
}

/**
 * 探索ノードの評価値を更新する。
 * @param value 評価値
 */
void Node::updateValue(float value) {
  std::unique_lock<std::shared_mutex> lock(_valueMutex);
  _count += 1;
  _value += value;
}

/**
 * 探索ノードの評価値をキャンセルする。
 * @param value 評価値
 */
void Node::cancelValue(float value) {
  std::unique_lock<std::shared_mutex> lock(_valueMutex);
  _count -= 1;
  _value -= value;
}

/**
 * PolicyNetworkの評価値が最も高い候補手を取得する。
 * @return 候補手
 */
Move Node::getPolicyMove() {
  {
    // このノードの盤面評価を実行する
    std::unique_lock<std::shared_mutex> lock(_evalMutex);
    _evaluateBoard(false);
  }

  // 詰みの手筋が見つかっている場合はその手を返す
  if (_checkMove != CHECK_MOVE_NOT_FOUND) {
    return _checkMove;
  }

  // 候補手の一覧を取得する
  std::vector<Policy> policies;

  {
    std::shared_lock<std::shared_mutex> lock(_evalMutex);
    for (Policy policy : _evaluator.getPolicies()) {
      policies.push_back(policy);
    }
  }

  // 候補手がない場合はパスを返す
  if (policies.empty()) {
    return MOVE_PASS;
  }

  // 最も着手確率が高い候補手を取得する
  Policy max_policy = policies[0];

  for (Policy policy : policies) {
    if (max_policy.policy < policy.policy) {
      max_policy = policy;
    }
  }

  // 最も着手確率が高い候補手を返す
  return max_policy.move;
}

/**
 * 着手を取得する。
 * @return 着手
 */
Move Node::getMove() {
  return _move;
}

/**
 * 次の手番を取得する。
 * @return 手番
 */
int32_t Node::getColor() {
  return _board.getColor();
}

/**
 * このノードの予想着手確率を取得する。
 * @return 予想着手確率
 */
float Node::getPolicy() {
  return _policy;
}

/**
 * 子ノードの一覧を取得する。
 * @return ノードオブジェクトの一覧
 */
std::vector<Node*> Node::getChildren() {
  std::shared_lock<std::shared_mutex> lock(_evalMutex);
  std::vector<Node*> children;

  for (std::pair<int32_t, Node*> item : _children) {
    children.push_back(item.second);
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
Node* Node::getChild(const Move& move) {
  std::unique_lock<std::shared_mutex> lock(_evalMutex);

  // 子ノードが存在する場合はそのノードを返す
  int32_t index = move.getValue();

  if (_children.find(index) != _children.end()) {
    return _children[index];
  }

  // 子ノードが存在しない場合は新しいノードを作成する
  Node* node = _manager->createNode();

  node->_setAsNextNode(this, move, 1.0);

  return node;
}

/**
 * このノードの詰み手筋を取得する。
 * 積み手筋が見つかっていない場合はMOVE_PASSを返す。
 * @return 詰み手筋
 */
Move Node::getCheckMove() const {
  if (_checkMove == CHECK_MOVE_NOT_FOUND) {
    return MOVE_PASS;
  } else {
    return _checkMove;
  }
}

/**
 * このノードの探索回数を取得する。
 * @return 探索回数
 */
int32_t Node::getVisits() {
  std::shared_lock<std::shared_mutex> lock(_evalMutex);
  return _visits;
}

/**
 * プレイアウト数を取得する。
 * @return プレイアウト数
 */
int32_t Node::getPlayouts() {
  std::shared_lock<std::shared_mutex> lock(_evalMutex);
  return _playouts;
}

/**
 * プレイアウト数を設定する。
 * @param playouts プレイアウト数
 */
void Node::setPlayouts(int32_t playouts) {
  std::unique_lock<std::shared_mutex> lock(_evalMutex);
  _playouts = playouts;
}

/**
 * このノードの評価値を取得する。
 * @return 評価値
 */
float Node::getValue() {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  if (_checkMove != CHECK_MOVE_NOT_FOUND) {
    return _board.getColor();
  } else if (_count == 0) {
    return 0.0f;
  } else {
    return _value / _count;
  }
}

/**
 * このノードの評価値の信頼区間の下限を取得する。
 * @return 信頼区間の下限
 */
float Node::getValueLCB() {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  if (_count == 0) {
    return 0.0f;
  } else {
    float value = _value / _count;
    float lower = 1.96 * 0.5 / std::sqrt(_visits + 1);
    return value - (lower * OPPOSITE_COLOR(_board.getColor()));
  }
}

/**
 * PUCBに基づいてこのノードの優先度を取得する。
 * @param totalVisits 探索回数の合計
 * @return 優先度
 */
float Node::getPriorityByPUCB(int32_t totalVisits) {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  if (_count == 0) {
    return -99.0f;
  } else {
    float c_puct = std::log((1 + totalVisits + 19652.0) / 19652.0) + 1.25;
    float value = (_value / _count) * OPPOSITE_COLOR(_board.getColor());
    float upper = c_puct * _policy * std::sqrt(totalVisits) / (1 + _visits);
    return value + 2 * upper;
  }
}

/**
 * UCB1に基づいてこのノードの優先度を取得する。
 * @param totalVisits 探索回数の合計
 * @return 優先度
 */
float Node::getPriorityByUCB1(int32_t totalVisits) {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  if (_count == 0) {
    return -99.0f;
  } else {
    float value = (_value / _count) * OPPOSITE_COLOR(_board.getColor());
    float upper = 0.5 * std::sqrt(std::log(totalVisits) / (_visits + 1));
    return value + upper;
  }
}

/**
 * このノードの予想進行を取得する。
 * @return 予想進行
 */
std::vector<Move> Node::getVariations() {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  std::vector<Move> variations;

  int32_t max_visits = 0;
  Node* max_child = nullptr;

  for (auto child : _children) {
    if (child.second->_visits > max_visits) {
      max_visits = child.second->_visits;
      max_child = child.second;
    }
  }

  variations.push_back(_move);

  if (max_child != nullptr) {
    std::vector<Move> child_variations = max_child->getVariations();
    variations.insert(variations.end(), child_variations.begin(), child_variations.end());
  }

  return variations;
}

/**
 * 指定された盤面オブジェクトに盤面の状態をコピーする。
 * @param board 盤面オブジェクト
 */
void Node::copyBoardTo(Board* board) {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  board->copyFrom(&_board);
}

/**
 * このノードの情報を出力する。
 * @param os 出力先
 */
void Node::print(std::ostream& os) {
  _board.print(os);
  os << "visits:" << _visits << std::endl;
  os << "value:" << getValue() << std::endl;
}

/**
 * このノードの盤面評価を実行する。
 * @param searchCheckMove 詰み手筋を探索する場合はtrue
 */
void Node::_evaluateBoard(bool searchCheckMove) {
  // 詰み手筋が未発見かつ未探索の場合は5手詰め探索を実行する
  // 探索中に盤面オブジェクトが変更されるため、複製した盤面オブジェクトを使用する
  if (!_checkMoveShallowSearched && _checkMove == CHECK_MOVE_NOT_FOUND) {
    _checkMoveShallowSearched = true;

    Board board = _board;
    Move move = board.searchCheckMove(5, 0);

    if (move != MOVE_PASS) {
      _checkMove = move;
    } else {
      _checkMove = CHECK_MOVE_NOT_FOUND;
    }
  }

  // 詰み手筋が見つかっている場合は何もしない
  if (_checkMove != CHECK_MOVE_NOT_FOUND) {
    return;
  }

  // 評価済みの場合は何もしない
  if (_evaluator.isEvaluated()) {
    return;
  }

  // 評価を実行して次に評価する候補手の一覧を作成する
  _evaluator.evaluate(&_board);

  for (Policy policy : _evaluator.getPolicies()) {
    _childPolicies.push_back(policy);
  }
}

/**
 * このノードの状態を評価して、評価結果を返す。
 * @param equally 探索回数を均等にする場合はtrue
 * @param width 探索幅(0の場合は探索幅を自動で調整する)
 * @param useUcb1 UCB1を使用する場合はtrue・PUCBを使用する場合はfalse
 * @param temperature 探索の温度パラメータ
 * @param noise 探索のガンベルノイズの強さ
 * @return 評価結果
 */
NodeResult Node::_evaluateNode(
    bool equally, int32_t width, bool useUcb1, float temperature, float noise) {
  // 入玉宣言が可能な状態であれば手番勝利の評価結果を返す
  if (_board.isNyugyoku()) {
    return NodeResult(nullptr, _board.getColor(), 1);
  }

  // 詰み手筋が見つかっている場合は手番勝利の評価結果を返す
  if (_checkMove != CHECK_MOVE_NOT_FOUND) {
    return NodeResult(nullptr, _board.getColor(), 1);
  }

  // 最初の訪問ならばこのノードの評価結果を返す
  if (_visits == 1) {
    return NodeResult(nullptr, _evaluator.getValue(), 1);
  }

  // 候補手がない場合はこのノードの評価値を返す
  if (_childPolicies.empty()) {
    return NodeResult(nullptr, _evaluator.getValue(), 1);
  }

  // キューから評価に追加する候補手を取得する
  int32_t children_size = _children.size() + _waitingSet.size();

  if (children_size < _childPolicies.size() && (width < 1 || children_size < width)) {
    int32_t max_index = 0;
    int max_priority_type = 0;
    float max_priority = 0.0f;

    // 温度パラメータを計算する
    float win_chance = getValue() * _board.getColor() * 0.5 + 0.5;
    float temperature_power = win_chance + (1.0 / temperature) * (1 - win_chance);

    // ガンベルノイズの生成オブジェクトを作成する
    // 子ノードの数が4以下の場合はノイズを加えない
    float noise_scale = (children_size <= 4) ? 0.0f : noise;
    std::extreme_value_distribution<float> noise_dist(0.0f, noise_scale);

    // 優先度が最も高い候補手を探す
    for (int i = 0; i < _childPolicies.size(); i++) {
      Policy& policy = _childPolicies[i];
      float probability = policy.policy;
      float probability_org = probability;

      // 温度パラメータを反映させる
      probability = std::pow(probability, temperature_power);

      // ガンベルノイズを加える
      // ノイズを加算する対象はロジットとなるため、確率に対してはe^noiseを乗算する
      probability *= std::exp(noise_dist(random_engine));

      // 優先度を計算する
      int32_t priority_type = 1;
      float priority = probability / (policy.visits + 1);

      // 探索回数を均等にする設定となっている場合は登録済みの候補手の優先度を下げる
      if (equally) {
        int32_t policy_index = policy.move.getValue();

        if (_children.find(policy_index) != _children.end() ||
            _waitingSet.find(policy_index) != _waitingSet.end()) {
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
    Policy& max_policy = _childPolicies[max_index];
    int32_t max_policy_index = max_policy.move.getValue();

    if (_children.find(max_policy_index) == _children.end() &&
        _waitingSet.find(max_policy_index) == _waitingSet.end()) {
      _waitingQueue.push(max_policy);
      _waitingSet.insert(max_policy_index);
    }

    // 訪問数を増やす
    _childPolicies[max_index].visits += 1;
  }

  // 探索幅が指定されていない場合と子ノードの数が指定された探索幅に達していない場合、
  // 待機リストに候補手が存在する場合は新しい子ノードを作成して次の探索先として返す
  if (_waitingQueue.size() > 0 && (width <= 0 || _children.size() < width)) {
    // 最初に登録された待機中の候補手を取得する
    Policy policy = _waitingQueue.front();
    int32_t policy_index = policy.move.getValue();

    _waitingQueue.pop();
    _waitingSet.erase(policy_index);

    // 未登録の候補手であれば新しい子ノードを作成して次の探索先として返す
    if (_children.find(policy_index) == _children.end()) {
      Node* node = _manager->createNode();
      bool leaf = _children.empty();

      node->_setAsNextNode(this, policy.move, policy.policy);
      _children[policy_index] = node;

      if (leaf) {
        return NodeResult(node, _evaluator.getValue(), -1);
      } else {
        return NodeResult(node, _evaluator.getValue(), 0);
      }
    }
  }

  // 探索対象とする子ノードの一覧を作成する
  std::vector<std::pair<Node*, float>> children;

  for (std::pair<int32_t, Node*> child : _children) {
    children.push_back(std::make_pair(
        child.second, child.second->getValueLCB() * getColor()));
  }

  // 探索幅が指定されている場合は探索対象とする子ノードの数を制限する
  if (width > 0 && children.size() > width) {
    std::sort(children.begin(), children.end(), [](auto a, auto b) {
      return a.second > b.second;
    });

    children.resize(width);
  }

  // 最も優先度が高いノードを次の探索先として返す
  Node* max_node = children[0].first;
  float max_priority = -1.0;

  for (std::pair<Node*, float> child : children) {
    float priority;

    // 優先度を計算する
    if (equally) {
      float visits = child.first->getVisits();
      float value = child.first->getValue() * getColor();
      priority = 1.0 / (visits + 1 - value * 0.5);
    } else if (useUcb1) {
      priority = child.first->getPriorityByUCB1(_visits);
    } else {
      priority = child.first->getPriorityByPUCB(_visits);
    }

    // 深い詰み探索が未実行のノードがある場合はそのノードを優先的に探索する
    if (!child.first->_checkMoveDeepSearched &&
        child.first->_checkMove == CHECK_MOVE_NOT_FOUND) {
      priority += 100.0;
    }

    if (max_priority < priority) {
      max_node = child.first;
      max_priority = priority;
    }
  }

  return NodeResult(max_node, _evaluator.getValue(), 0);
}

/**
 * ノードの評価情報を初期化する。
 */
void Node::_reset() {
  _evaluator.clear();
  _children.clear();
  _childPolicies.clear();
  _waitingQueue = std::queue<Policy>();
  _waitingSet.clear();
  _checkMove = CHECK_MOVE_NOT_FOUND;
  _checkMoveShallowSearched = false;
  _checkMoveDeepSearched = false;
  _visits = 0;
  _playouts = 0;
  _value = 0.0f;
  _count = 0;
}

/**
 * 指定されたノードの継続ノードとしての値を設定する。
 * @param prevNode 前のノード
 * @param move 着手情報
 * @param policy 予想着手確率
 */
void Node::_setAsNextNode(Node* prevNode, const Move& move, float policy) {
  _board.copyFrom(&prevNode->_board);
  _board.play(move);

  _move = move;
  _policy = policy;
  _reset();
}

}  // namespace deepshogi

#include "Player.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <queue>
#include <unordered_map>

namespace deepshogi {

/**
 * プレイヤオブジェクトを作成する。
 * @param processor 推論を実行するオブジェクト
 * @param threads スレッドの数
 * @param nyugyokuScoreBlack 先手番の入玉宣言に必要となる点数
 * @param nyugyokuScoreWhite 後手番の入玉宣言に必要となる点数
 * @param drawSteps 引き分けとなるまでの手数
 * @param checkSearchDepth 詰み手筋の探索深さ
 * @param checkSearchNode 詰み手筋の探索ノード数
 * @param evalLeafOnly 葉ノードのみ評価するならばtrue
 */
Player::Player(
    Processor* processor, int32_t threads,
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps,
    int32_t checkSearchDepth, int32_t checkSearchNode, bool evalLeafOnly)
    : _mutex(),
      _condition(),
      _nodeManager(NodeParameter(
          processor, nyugyokuScoreBlack, nyugyokuScoreWhite, drawSteps,
          checkSearchDepth, checkSearchNode)),
      _threadPool(threads),
      _thread(),
      _root(_nodeManager.createNode()),
      _evalLeafOnly(evalLeafOnly),
      _searchVisits(0),
      _searchPlayouts(0),
      _searchEqually(false),
      _searchUseUcb1(false),
      _searchCandidateWidth(0),
      _searchCheckNodeDepth(0),
      _searchTemperature(1.0f),
      _searchNoise(0.0f),
      _runnings(0),
      _paused(false),
      _stopped(true),
      _terminated(false) {
  _thread.reset(new std::thread([this]() { this->_run(); }));
  _root->initialize(cshogi::DefaultStartPositionSFEN);
}

/**
 * プレイヤオブジェクトを破棄する。
 */
Player::~Player() {
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _terminated = true;
    _condition.notify_all();
  }

  _thread->join();
}

/**
 * プレイヤオブジェクトの状態を初期化する。
 * @param sfen 初期局面のSFEN
 */
void Player::initialize(const std::string& sfen) {
  std::unique_lock<std::mutex> lock(_mutex);

  // スレッドを一時停止する
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // 現在のルートノードを保存する
  Node* old_root = _root;

  // 初期ノードをルートノードに設定する
  _root = _nodeManager.createNode();
  _root->initialize(sfen);

  // ルートノード以外のノードを解放する
  _releaseNode(old_root);

  // スレッドを再開する
  _paused = false;
  _condition.notify_all();
}

/**
 * 次の手番を取得する。
 * @return 手番
 */
int32_t Player::getColor() {
  return _root->getColor();
}

/**
 * 駒を動かす。
 * @param move 動かす駒の情報
 */
void Player::play(const Move& move) {
  std::unique_lock<std::mutex> lock(_mutex);

  // スレッドを一時停止する
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // 現在のルートノードを保存する
  Node* old_root = _root;

  // 新しいルートノードを設定する
  _root = old_root->getChild(move);

  // ルートノード以外のノードを解放する
  _releaseNode(old_root);

  // スレッドを再開する
  _paused = false;
  _condition.notify_all();
}

/**
 * 盤面評価を開始する。
 * 探索処理は別スレッドで実行される。
 * 最大訪問回数に0以下の値を指定すると停止命令を指示するまで探索を続ける。
 * @param equally 探索回数を均等にするならばtrue、UCB1かPUCBを使用するならばfalse
 * @param useUcb1 探索先の基準としてUCB1を使用するならばtrue、PUCBを使用するならばfalse
 * @param candidateWidth 候補手の探索幅(0の場合は探索幅を自動で調整する)
 * @param checkNodeDepth 詰み手筋を探索するノードの最大深さ
 * @param temperature 探索の温度パラメータ
 * @param noise 探索のガンベルノイズの強さ
 */
void Player::startEvaluation(
    bool equally, bool useUcb1, int32_t candidateWidth, int32_t checkNodeDepth,
    float temperature, float noise) {
  std::unique_lock<std::mutex> lock(_mutex);

  // スレッドを一時停止する
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // 探索の設定を変更する
  _searchVisits = _root->getVisits();
  _searchPlayouts = _root->getPlayouts();
  _searchEqually = equally;
  _searchUseUcb1 = useUcb1;
  _searchCandidateWidth = candidateWidth;
  _searchCheckNodeDepth = checkNodeDepth;
  _searchTemperature = temperature;
  _searchNoise = noise;

  // 実行状態に設定する
  _stopped = false;

  // スレッドを再開する
  _paused = false;
  _condition.notify_all();
}

/**
 * 設定された盤面評価処理が終了するまで待機する。
 * @param visits 探索の訪問回数
 * @param playouts 探索のプレイアウト回数
 * @param timelimit 待機する時間（秒）
 * @param stop 探索を停止するならばtrue
 */
void Player::waitEvaluation(int32_t visits, int32_t playouts, float timelimit, bool stop) {
  std::unique_lock<std::mutex> lock(_mutex);

  // 指定された訪問数とプレイアウト数になるまで待機する
  std::chrono::milliseconds timeout(static_cast<int32_t>(timelimit * 1000.0f));
  _condition.wait_for(lock, timeout, [this, visits, playouts]() {
    return _searchVisits >= visits && _searchPlayouts >= playouts;
  });

  // 探索を停止する
  _stopped = _stopped || stop;
}

/**
 * 候補手の一覧を取得する。
 * @return 候補手の一覧
 */
std::vector<Candidate> Player::getCandidates() {
  std::unique_lock<std::mutex> lock(_mutex);

  // スレッドを一時停止する
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // 候補手の一覧を作成する
  std::vector<Candidate> candidates;

  // 詰み手筋の着手がある場合は詰み手筋のみを候補手とする
  if (_root->getCheckMove() != MOVE_PASS) {
    candidates.emplace_back(
        _root->getCheckMove(), _root->getColor(),
        _root->getVisits() - 1, _root->getPlayouts(),
        1.0f, _root->getColor());
  }
  // 詰み手筋がない場合は子ノードの一覧を候補手とする
  // 子ノードに詰み手筋がある場合は評価値を相手勝利に設定する
  // 詰み手筋がない場合は評価値*0.999をノードの評価値に設定する
  else {
    for (Node* node : _root->getChildren()) {
      if (node->getCheckMove() != MOVE_PASS) {
        candidates.emplace_back(
            node->getMove(), _root->getColor(), node->getVisits(), node->getPlayouts(),
            node->getPolicy(), node->getColor(), node->getVariations());
      } else {
        candidates.emplace_back(
            node->getMove(), _root->getColor(), node->getVisits(), node->getPlayouts(),
            node->getPolicy(), node->getValue(), node->getVariations());
      }
    }
  }

  // 候補手がない場合はPolicyNetworkによる着手を追加する
  if (candidates.empty()) {
    candidates.emplace_back(
        _root->getPolicyMove(), _root->getColor(), 0, 0, 1.0f, _root->getValue());
  }

  // スレッドを再開する
  _paused = false;
  _condition.notify_all();

  return candidates;
}

/**
 * 指定された盤面オブジェクトに盤面の状態をコピーする。
 * @param board 盤面オブジェクト
 */
void Player::copyBoardTo(Board* board) {
  std::unique_lock<std::mutex> lock(_mutex);
  _root->copyBoardTo(board);
}

/**
 * 探索処理を起動する。
 */
void Player::_run() {
  while (true) {
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _condition.wait(lock, [this]() {
        if (_terminated) {
          return true;
        } else if (!_stopped && !_paused && _runnings < _threadPool.getSize()) {
          return true;
        } else {
          return false;
        }
      });

      if (_terminated) {
        break;
      }

      _runnings += 1;
      _searchVisits += 1;
      _condition.notify_all();
    }

    _threadPool.submit([this]() {
      int32_t playouts = _evaluate();
      std::unique_lock<std::mutex> lock(_mutex);
      _searchPlayouts += playouts;
      _runnings -= 1;
      _condition.notify_all();
    });
  }
}

/**
 * 探索を実行する。
 * @return プレイアウト数
 */
int32_t Player::_evaluate() {
  std::vector<Node*> nodes = {_root};
  bool search_equally = _searchEqually;
  int32_t search_width = _searchCandidateWidth;
  bool search_use_ucb1 = _searchUseUcb1;
  int32_t search_check_depth = _searchCheckNodeDepth;
  float search_temperature = _searchTemperature;
  float search_noise = _searchNoise;
  int32_t playouts = 0;

  while (true) {
    NodeResult result = nodes.back()->evaluate(
        search_equally, search_width, search_use_ucb1, search_check_depth > 0,
        search_temperature, search_noise);

    // ノードの評価値を更新する
    if (result.getPlayouts() == 1) {
      for (Node* node : nodes) {
        node->updateValue(result.getValue());
      }
    } else if (result.getPlayouts() == -1 && _evalLeafOnly) {
      for (Node* node : nodes) {
        node->cancelValue(result.getValue());
      }
    }

    // ノードのプレイアウト数を更新する
    for (Node* node : nodes) {
      node->setPlayouts(node->getPlayouts() + result.getPlayouts());
    }

    // この探索のプレイアウト数を更新する
    playouts += result.getPlayouts();

    // 子ノードが存在する場合は次のノードとして設定する
    if (result.getNode() != nullptr) {
      nodes.push_back(result.getNode());
    } else {
      break;
    }

    // ルートノードだけに適用する設定項目をリセットする
    search_equally = 0;
    search_width = 0;
    search_use_ucb1 = false;
    search_check_depth -= 1;
    search_temperature = 1.0f;
    search_noise = 0.0f;
  }

  // プレイアウト数を返す
  return playouts;
}

/**
 * ルートノード以外のノードオブジェクトを返却する。
 * @param node 返却するノードオブジェクト
 */
void Player::_releaseNode(Node* node) {
  std::vector<Node*> stack = {node};

  while (!stack.empty()) {
    Node* current = stack.back();
    stack.pop_back();

    if (current == _root) {
      continue;
    }

    for (Node* child : current->getChildren()) {
      stack.push_back(child);
    }

    _nodeManager.releaseNode(current);
  }
}

}  // namespace deepshogi

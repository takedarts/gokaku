#include "Player.h"

#include <sstream>

namespace deepshogi {

// Playerクラスのコンストラクタで使用する初期盤面のSFEN
constexpr char DEFAULT_SFEN[] = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

/**
 * プレイヤオブジェクトを作成する。
 * @param processor 推論を実行するオブジェクト
 * @param threads スレッドの数
 * @param searchMaxVisits ノードの最大訪問回数
 * @param nyugyokuScoreBlack 先手番の入玉宣言に必要となる点数
 * @param nyugyokuScoreWhite 後手番の入玉宣言に必要となる点数
 * @param drawTurn 引き分けとなるまでの手数
 * @param checkSearchDepth 詰み手筋の探索深さ
 * @param checkSearchNode 詰み手筋の探索ノード数
 * @param checkNodeDepth 詰み手筋を探索するノードの最大深さ
 * @param ucbConstant UCBの信頼上限に掛ける定数
 * @param pucbConstantInit PUCBの信頼上限に掛ける定数の初期値
 * @param pucbConstantBase PUCBの信頼上限に掛ける定数の変化値
 */
Player::Player(
    InferenceProcessor* processor, int32_t threads, int32_t searchMaxVisits,
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
    int32_t checkSearchDepth, int32_t checkSearchNode, int32_t checkNodeDepth,
    float ucbConstant, float pucbConstantInit, float pucbConstantBase)
    : _mutex(),
      _searchCondition(),
      _updateCondition(),
      _stopCondition(),
      _processor(processor),
      _pnsearch(checkSearchNode, threads),
      _threadPool(threads),
      _searchThread(),
      _updateThread(),
      _manager(MctsParameter(
          nyugyokuScoreBlack, nyugyokuScoreWhite, drawTurn,
          ucbConstant, pucbConstantInit, pucbConstantBase)),
      _root(_manager.createNode()),
      _searchMaxVisits(searchMaxVisits),
      _checkSearchDepth(checkSearchDepth),
      _checkNodeDepth(checkNodeDepth),
      _searchEqually(false),
      _searchCandidateWidth(0),
      _searchTemperature(0.0f),
      _searchNoise(0.0f),
      _runnings(0),
      _paused(false),
      _stopped(true),
      _terminated(false),
      _evaluatingNodes(),
      _checkingNodes() {
  _root->initialize(DEFAULT_SFEN);
  _searchThread = std::thread(&Player::_runSearch, this);
  _updateThread = std::thread(&Player::_runUpdate, this);
}

/**
 * プレイヤオブジェクトを破棄する。
 */
Player::~Player() {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    _terminated = true;
  }

  _searchCondition.notify_one();
  _updateCondition.notify_one();
  _searchThread.join();
  _updateThread.join();
}

/**
 * プレイヤオブジェクトの状態を初期化する。
 * @param sfen 初期局面のSFEN
 */
void Player::initialize(const std::string& sfen) {
  std::unique_lock<std::mutex> lock(_mutex);

  // スレッドを一時停止する
  _paused = true;
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // 現在のルートノードを保存する
  MctsNode* old_root = _root;

  // 初期ノードをルートノードに設定する
  _root = _manager.createNode();
  _root->initialize(sfen);

  // 古いルートノードを含む探索木を解放する
  _manager.releaseTree(old_root);

  // スレッドを再開する
  _paused = false;
  _searchCondition.notify_one();
}

/**
 * 次の手番を取得する。
 * @return 手番
 */
int32_t Player::getColor() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _root->getBoard().getColor();
}

/**
 * 指定された着手にしたがって駒を動かす。
 * @param move 動かす駒の情報
 */
void Player::play(const Move& move) {
  std::unique_lock<std::mutex> lock(_mutex);

  // スレッドを一時停止する
  _paused = true;
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // 現在のルートノードを保存する
  MctsNode* old_root = _root;

  // 新しいルートノードを設定する
  _root = old_root->getChild(move);
  _root->setAsRootNode();

  // 新しいルートノードを古いルードノードから切り離す
  old_root->removeChild(move);

  // ルートノード以外のノードを解放する
  _manager.releaseTree(old_root);

  // スレッドを再開する
  _paused = false;
  _searchCondition.notify_one();
}

/**
 * 盤面評価を開始する。
 * 探索処理は別スレッドで実行される。
 * @param equally 探索回数を均等にするならばtrue、UCBかPUCBを使用するならばfalse
 * @param candidateWidth 候補手の探索幅(0の場合は探索幅を自動で調整する)
 * @param temperature 探索の温度パラメータ
 * @param noise 探索のガンベルノイズの強さ
 */
void Player::startEvaluation(
    bool equally, int32_t candidateWidth, float temperature, float noise) {
  std::unique_lock<std::mutex> lock(_mutex);

  // スレッドを一時停止する
  _paused = true;
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // 探索の設定を変更する
  _searchEqually = equally;
  _searchCandidateWidth = candidateWidth;
  _searchTemperature = temperature;
  _searchNoise = noise;

  // 実行状態に設定する
  _stopped = false;

  // スレッドを再開する
  _paused = false;
  _searchCondition.notify_one();
}

/**
 * 探索が終了するまで待機する。
 * @param visits 探索の訪問回数
 * @param playouts 探索のプレイアウト回数
 * @param timelimit 待機する時間（秒）
 * @param stop 探索を停止するならばtrue
 */
void Player::waitEvaluation(int32_t visits, int32_t playouts, float timelimit, bool stop) {
  std::unique_lock<std::mutex> lock(_mutex);

  // 訪問数かプレイアウト数に0以上が指定された場合、
  // ルートノードの訪問数が1以上になるまで待機する
  if (visits > 0 || playouts > 0) {
    _stopCondition.wait(lock, [this]() {
      return _root->getVisits() > 0;
    });
  }

  // 指定された訪問数とプレイアウト数になるまで待機する
  std::chrono::milliseconds timeout(static_cast<int32_t>(timelimit * 1000.0f));

  _stopCondition.wait_for(lock, timeout, [this, visits, playouts]() {
    return _root->getVisits() >= visits && _root->getPlayouts() >= playouts;
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
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // 候補手の一覧を作成する
  std::vector<Candidate> candidates;

  // 詰み手筋の着手がある場合は詰み手筋のみを候補手とする
  std::vector<Move> checkmate_moves = _root->getCheckmateMoves();

  if (!checkmate_moves.empty()) {
    candidates.emplace_back(
        checkmate_moves[0], _root->getBoard().getColor(),
        _root->getVisits() - 1, _root->getPlayouts(),
        1.0f, _root->getBoard().getColor(),
        checkmate_moves);
  }
  // 詰み手筋がない場合は子ノードの一覧を候補手とする
  // 子ノードに詰み手筋がある場合は評価値を相手勝利に設定する
  // 詰み手筋がない場合は評価値*0.999をノードの評価値に設定する
  else {
    for (MctsNode* node : _root->getChildren()) {
      if (!node->getCheckmateMoves().empty()) {
        candidates.emplace_back(
            node->getMove(), _root->getBoard().getColor(),
            node->getVisits(), node->getPlayouts(),
            node->getProbability(), node->getBoard().getColor(),
            node->getVariations());
      } else {
        candidates.emplace_back(
            node->getMove(), _root->getBoard().getColor(),
            node->getVisits(), node->getPlayouts(),
            node->getProbability(), node->getMctsValue(),
            node->getVariations());
      }
    }
  }

  // 候補手がない場合はPolicyNetworkによる着手を追加する
  if (candidates.empty()) {
    Move policy_move = _root->getPolicyMove();

    if (policy_move != MOVE_INVALID) {
      candidates.emplace_back(
          _root->getPolicyMove(), _root->getBoard().getColor(),
          0, 0, 1.0f, _root->getMctsValue());
    }
  }

  // スレッドを再開する
  _paused = false;
  _searchCondition.notify_one();

  return candidates;
}

/**
 * 指定された盤面オブジェクトに盤面の状態をコピーする。
 * @param board 盤面オブジェクト
 */
void Player::copyBoardTo(Board* board) {
  std::unique_lock<std::mutex> lock(_mutex);
  board->copyFrom(&_root->getBoard());
}

/**
 * プレイヤオブジェクトの状態を表す文字列を取得する。
 * @return プレイヤオブジェクトの状態を表す文字列
 */
std::string Player::toString() {
  std::unique_lock<std::mutex> lock(_mutex);
  std::stringstream ss;

  // スレッドを一時停止する
  _paused = true;
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // 盤面の状態を文字列に変換する
  ss << "--- Board ---" << std::endl
     << _root->getBoard() << std::endl;

  // 探索木を深さ優先で辿りながら現在の状態を文字列に変換する
  std::vector<std::pair<MctsNode*, std::string>> stack = {{_root, ""}};

  ss << "--- Nodes ---" << std::endl;
  while (!stack.empty()) {
    MctsNode* current = stack.back().first;
    std::string prefix = stack.back().second;
    stack.pop_back();

    Move move = current->getMove();
    char color = (current->getBoard().getColor() == COLOR_BLACK) ? 'B' : 'W';
    ss << prefix
       << "Color=" << color << ", "
       << "Move=" << move << ", "
       << "Prob=" << std::fixed << std::setprecision(2) << current->getProbability() << ", "
       << "Value=" << std::fixed << std::setprecision(2) << current->getNodeValue() << ", "
       << "Visits=" << current->getVisits() << ", "
       << "Playouts=" << current->getPlayouts() << ", "
       << "MctsValue=" << std::fixed << std::setprecision(2) << current->getMctsValue() << ", "
       << "Checkmate=" << (!current->getCheckmateMoves().empty() ? "Yes" : "No")
       << std::endl;

    std::vector<MctsNode*> children = current->getChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      stack.emplace_back(*it, prefix + "  ");
    }
  }

  // スレッドを再開する
  _paused = false;
  _searchCondition.notify_one();

  return ss.str();
}

/**
 * 探索を実行する。
 */
void Player::_runSearch() {
  // 評価ノード数の最大数を計算する
  const int32_t max_evaluating_size =
      _processor->getBatchSize() * _processor->getThreadSize() * 10;

  // ルートノードのポインタを保存する変数を作成する
  // ルートノードの変更を検知するために使用する
  MctsNode* last_root_node = nullptr;

  while (true) {
    // 詰み手筋の探索を実行するノード
    MctsNode* checkmate_search_node = nullptr;

    {
      std::unique_lock<std::mutex> lock(_mutex);

      // 探索処理が実行可能になるまで待機する
      // 探索処理が実行可能になる条件は以下のいずれか
      // - [停止] 終了が要求されていて、実行中のスレッドがなくて、
      //   評価中のノードがなくて、詰み探索待機中のノードがない
      // - [詰み探索] 詰み探索待機中のノードがある
      // - [手順探索] 終了が要求、探索が停止要求、一時停止要求のいずれもなくて、
      //   実行スレッド数がスレッドプールのスレッド数未満で、
      //   評価中のノードの数が最大評価ノード数未満で、探索回数が最大訪問回数未満
      _searchCondition.wait(lock, [this, max_evaluating_size]() {
        if (_terminated && _runnings == 0 &&
            _evaluatingNodes.empty() && _checkingNodes.empty()) {
          return true;
        } else if (!_checkingNodes.empty()) {
          return true;
        } else if (
            !_terminated && !_stopped && !_paused &&
            _runnings < _threadPool.getSize() &&
            _evaluatingNodes.size() < max_evaluating_size &&
            _root->getVisits() < _searchMaxVisits) {
          return true;
        } else {
          return false;
        }
      });

      // 停止条件を満たしているならばループを抜ける
      if (_terminated && _runnings == 0 &&
          _evaluatingNodes.empty() && _checkingNodes.empty()) {
        break;
      }

      // ルートノードが変更されている場合は
      // 指定された深さまでのノードで、詰み手筋の探索が未実行であるノードを
      // 詰み手筋の探索待機キューに追加する
      if (last_root_node != _root) {
        last_root_node = _root;

        // 深さ優先探索で指定された深さまでのすべてのノードをチェックする
        std::vector<std::pair<MctsNode*, int32_t>> stack = {{_root, 0}};

        while (!stack.empty()) {
          MctsNode* node = stack.back().first;
          int32_t depth = stack.back().second;
          stack.pop_back();

          if (depth < _checkNodeDepth && !node->isCheckmateSearched()) {
            _checkingNodes.push(node);
          }

          if (depth + 1 < _checkNodeDepth) {
            for (MctsNode* child : node->getChildren()) {
              stack.emplace_back(child, depth + 1);
            }
          }
        }
      }

      // 詰み探索待機中のノードがある場合はそのノードを取り出す
      if (!_checkingNodes.empty()) {
        checkmate_search_node = _checkingNodes.front();
        _checkingNodes.pop();
      }

      // そうでない場合は探索処理を実行する
      _runnings += 1;
    }

    // 詰み探索のノードがある場合は詰み探索の処理をスレッドプールに登録する
    if (checkmate_search_node != nullptr) {
      _threadPool.submit([this, checkmate_search_node]() {
        _runCheckmateSearch(checkmate_search_node);

        {
          std::unique_lock<std::mutex> lock(_mutex);
          _runnings -= 1;
        }

        _searchCondition.notify_one();
        _updateCondition.notify_one();
        _stopCondition.notify_all();
      });
    }
    // そうでない場合は探索木の展開処理をスレッドプールに登録する
    else {
      _threadPool.submit([this]() {
        _runExpand();

        {
          std::unique_lock<std::mutex> lock(_mutex);
          _runnings -= 1;
        }

        _searchCondition.notify_one();
        _updateCondition.notify_one();
        _stopCondition.notify_all();
      });
    }
  }
}

/**
 * 探索木を展開する。
 */
void Player::_runExpand() {
  // 探索の設定をローカル変数にコピーする
  bool search_equally = _searchEqually;
  int32_t search_width = _searchCandidateWidth;
  float search_temperature = _searchTemperature;
  float search_noise = _searchNoise;

  // ルートノードから探索を開始する
  // 次に評価するノードを取得しながら探索木を辿る
  MctsNode* node = nullptr;
  MctsNode* next_node = _root;
  int32_t depth = 0;

  while (true) {
    // 次に評価するノードを取得する
    node = next_node;
    next_node = node->pickupNextNode(
        search_equally, search_width, search_temperature, search_noise);

    // 次に評価するノードが存在しない場合は探索を終了する
    if (next_node == nullptr) {
      break;
    }

    // 探索の設定を更新する
    search_equally = false;
    search_width = 0;
    search_temperature = 1.0f;
    search_noise = 0.0f;
    depth += 1;
  }

  // 未評価の場合
  if (!node->isEvaluated()) {
    // 盤面評価の推論モデルに評価対象としてノードを登録する
    _processor->submit(node, [this](MctsNode* node) {
      std::unique_lock<std::mutex> lock(_mutex);
      _updateCondition.notify_one();
    });

    // 指定された深さ未満のノードで、かつ詰み手筋の探索を実行していないノードの場合、
    // 詰み手筋の探索待機キューにノードを追加する
    if (depth < _checkNodeDepth && !node->isCheckmateSearched()) {
      std::unique_lock<std::mutex> lock(_mutex);
      _checkingNodes.push(node);
      _searchCondition.notify_one();
    }
  }

  // 評価中のノードの一覧にノードを追加する
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _evaluatingNodes.push(node);
    _updateCondition.notify_one();
  }
}

/**
 * 詰み探索を実行する。
 */
void Player::_runCheckmateSearch(MctsNode* node) {
  PnSearchEngine* engine = _pnsearch.acquire();
  int32_t remain_turn = node->getBoard().getDrawTurn() - node->getBoard().getTurn() + 1;
  int32_t search_depth = std::min(_checkSearchDepth, remain_turn);

  node->searchCheckmateMoves(engine, search_depth);
  _pnsearch.release(engine);
}

/**
 * ノードの状態を更新する。
 */
void Player::_runUpdate() {
  while (true) {
    std::vector<MctsNode*> finished_nodes;

    {
      std::unique_lock<std::mutex> lock(_mutex);

      // 更新処理が実行可能になるまで待機する
      // 更新処理が実行可能になる条件は以下のいずれか
      // - [停止] 終了が要求されていて、実行中のスレッドがなくて、
      //   評価中のノードがなくて、詰み探索待機中のノードがない
      // - [評価] 評価中のノードがあって、そのノードの評価が完了している
      _updateCondition.wait(lock, [this]() {
        if (_terminated && _runnings == 0 &&
            _evaluatingNodes.empty() && _checkingNodes.empty()) {
          return true;
        } else if (
            !_evaluatingNodes.empty() && _evaluatingNodes.front()->isEvaluated()) {
          return true;
        } else {
          return false;
        }
      });

      // 停止条件を満たしているならばループを抜ける
      if (_terminated && _runnings == 0 &&
          _evaluatingNodes.empty() && _checkingNodes.empty()) {
        break;
      }

      // 評価済みのノードを取り出す
      while (!_evaluatingNodes.empty() && _evaluatingNodes.front()->isEvaluated()) {
        finished_nodes.push_back(_evaluatingNodes.front());
        _evaluatingNodes.pop();
      }
    }

    // 評価済みのノードの統計情報を更新する
    // 詰み手順が見つかっているノードで評価値をNodeValueに設定する
    for (MctsNode* node : finished_nodes) {
      float mcts_value = node->getNodeValue();
      MctsNode* current_node = node;

      while (current_node != nullptr) {
        if (!current_node->getCheckmateMoves().empty()) {
          mcts_value = current_node->getNodeValue();
        }

        current_node->updateMctsValue(mcts_value);
        current_node = current_node->getParent();
      }
    }

    // 探索処理に通知する
    _searchCondition.notify_one();
    _stopCondition.notify_all();
  }
}

}  // namespace deepshogi

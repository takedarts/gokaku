#include "Player.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>

namespace deepshogi {

/**
 * Create a player object.
 * @param processor Object that performs inference
 * @param threads Number of threads
 * @param nyugyokuScoreBlack Points required for black's entering king declaration
 * @param nyugyokuScoreWhite Points required for white's entering king declaration
 * @param drawTurn Number of moves until a draw
 * @param checkSearchDepth Depth for mate search
 * @param checkSearchNode Number of nodes for mate search
 * @param ucbConstant Constant multiplied to UCB upper confidence bound
 * @param pucbConstantInit Initial value applied to PUCB upper confidence bound
 * @param pucbConstantBase Base value applied to PUCB upper confidence bound
 * @param evalLeafOnly True if only leaf nodes are evaluated
 * @param maxVisits Maximum number of visits for search
 */
Player::Player(
    Processor* processor, int32_t threads,
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
    int32_t checkSearchDepth, int32_t checkSearchNode,
    float ucbConstant, float pucbConstantInit, float pucbConstantBase,
    bool evalLeafOnly, int32_t maxVisits)
    : _mutex(),
      _condition(),
      _nodeManager(NodeParameter(
          processor, nyugyokuScoreBlack, nyugyokuScoreWhite, drawTurn,
          ucbConstant, pucbConstantInit, pucbConstantBase)),
      _threadPool(threads),
      _thread(),
      _dfpnEnginePool(checkSearchNode, threads),
      _root(_nodeManager.createNode()),
      _evalLeafOnly(evalLeafOnly),
      _maxVisits(maxVisits),
      _checkSearchDepth(checkSearchDepth),
      _searchVisits(0),
      _searchPlayouts(0),
      _searchEqually(false),
      _searchAlgorithm(SEARCH_PUCB),
      _searchCandidateWidth(0),
      _searchCheckNodeDepth(0),
      _searchTemperature(1.0f),
      _searchNoise(0.0f),
      _runnings(0),
      _paused(false),
      _stopped(true),
      _terminated(false) {
  _thread.reset(new std::thread([this]() { this->_run(); }));
  _root->initialize("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

/**
 * Destroy the player object.
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
 * Initialize the state of the player object.
 * @param sfen Initial position SFEN
 */
void Player::initialize(const std::string& sfen) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the thread
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // Save the current root node
  Node* old_root = _root;

  // Set the initial node as the root node
  _root = _nodeManager.createNode();
  _root->initialize(sfen);

  // Release nodes other than the root node
  _releaseNode(old_root);

  // Resume the thread
  _paused = false;
  _condition.notify_all();
}

/**
 * Get the next turn.
 * @return Turn
 */
int32_t Player::getColor() {
  return _root->getColor();
}

/**
 * Move a piece.
 * @param move Information of the piece to move
 */
void Player::play(const Move& move) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the thread
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // Save the current root node
  Node* old_root = _root;

  // Set the new root node
  _root = old_root->getChild(move);

  // Release nodes other than the root node
  _releaseNode(old_root);

  // Resume the thread
  _paused = false;
  _condition.notify_all();
}

/**
 * Start board evaluation.
 * The search process is executed in a separate thread.
 * @param equally True to make the number of searches equal, false to use UCB or PUCB
 * @param algorithm Search algorithm
 * @param candidateWidth Search width for candidate moves (if 0, the width is automatically adjusted)
 * @param checkNodeDepth Maximum depth of nodes for mate search
 * @param temperature Temperature parameter for search
 * @param noise Strength of Gumbel noise for search
 */
void Player::startEvaluation(
    bool equally, int32_t algorithm, int32_t candidateWidth, int32_t checkNodeDepth,
    float temperature, float noise) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the thread
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // Change the search settings
  _searchVisits = _root->getVisits();
  _searchPlayouts = _root->getPlayouts();
  _searchEqually = equally;
  _searchAlgorithm = algorithm;
  _searchCandidateWidth = candidateWidth;
  _searchCheckNodeDepth = checkNodeDepth;
  _searchTemperature = temperature;
  _searchNoise = noise;

  // Set to running state
  _stopped = false;

  // Resume the thread
  _paused = false;
  _condition.notify_all();
}

/**
 * Wait until the configured board evaluation process is finished.
 * @param visits Number of search visits
 * @param playouts Number of search playouts
 * @param timelimit Time to wait (seconds)
 * @param stop True to stop the search
 */
void Player::waitEvaluation(int32_t visits, int32_t playouts, float timelimit, bool stop) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Wait until the specified number of visits and playouts is reached
  std::chrono::milliseconds timeout(static_cast<int32_t>(timelimit * 1000.0f));
  _condition.wait_for(lock, timeout, [this, visits, playouts]() {
    return _searchVisits >= visits && _searchPlayouts >= playouts;
  });

  // Stop the search
  _stopped = _stopped || stop;
}

/**
 * Get the list of candidate moves.
 * @return List of candidate moves
 */
std::vector<Candidate> Player::getCandidates() {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the thread
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // Create the list of candidate moves
  std::vector<Candidate> candidates;

  // If there is a mate move, only the mate move is considered as a candidate
  std::vector<Move> checkmate_moves = _root->getCheckmateMoves();

  if (!checkmate_moves.empty()) {
    candidates.emplace_back(
        checkmate_moves[0], _root->getColor(),
        _root->getVisits() - 1, _root->getPlayouts(),
        1.0f, _root->getColor(), _root->getColor(), checkmate_moves);
  }
  // If there is no mate move, the list of child nodes is considered as candidates
  // If a child node has a mate move, set the evaluation value to opponent's win
  // If there is no mate move, set the evaluation value to node's value * 0.999
  else {
    for (Node* node : _root->getChildren()) {
      if (!node->getCheckmateMoves().empty()) {
        candidates.emplace_back(
            node->getMove(), _root->getColor(), node->getVisits(), node->getPlayouts(),
            node->getPolicy(), node->getColor(), node->getColor(), node->getVariations());
      } else {
        candidates.emplace_back(
            node->getMove(), _root->getColor(), node->getVisits(), node->getPlayouts(),
            node->getPolicy(), node->getValue(), node->getMinimax(), node->getVariations());
      }
    }
  }

  // If there are no candidate moves, add a move from the PolicyNetwork
  if (candidates.empty()) {
    candidates.emplace_back(
        _root->getPolicyMove(), _root->getColor(), 0, 0,
        1.0f, _root->getValue(), _root->getMinimax());
  }

  // Resume the thread
  _paused = false;
  _condition.notify_all();

  return candidates;
}

/**
 * Copy the board state to the specified board object.
 * @param board Board object
 */
void Player::copyBoardTo(Board* board) {
  std::unique_lock<std::mutex> lock(_mutex);
  _root->copyBoardTo(board);
}

/**
 * 探索木のデバッグ情報を出力する。
 */
std::string Player::getDebugInfo() {
  std::unique_lock<std::mutex> lock(_mutex);

  // スレッドを一時停止する
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // 探索木を深さ優先で辿りながらデバッグ情報を作成する
  std::vector<std::pair<Node*, std::string>> stack = {{_root, ""}};
  std::ostringstream output;

  while (!stack.empty()) {
    Node* current = stack.back().first;
    std::string prefix = stack.back().second;
    stack.pop_back();

    Move move = current->getMove();
    output << prefix
           << "Move: " << move.getSrcX() << "," << move.getSrcY()
           << "->" << move.getDstX() << "," << move.getDstY()
           << "Promote: " << (move.isPromote() ? "Y" : "N") << " "
           << "Color: " << current->getColor() << " "
           << "Visits: " << current->getVisits() << " "
           << "Playouts: " << current->getPlayouts() << " "
           << "Policy: " << current->getPolicy() << " "
           << "Value: " << current->getValue() << " "
           << "Minimax: " << current->getMinimax()
           << std::endl;

    std::vector<Node*> children = current->getChildren();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      stack.emplace_back(*it, prefix + "  ");
    }
  }

  // スレッドを再開する
  _paused = false;
  _condition.notify_all();

  return output.str();
}

/**
 * Start the search process.
 */
void Player::_run() {
  while (true) {
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _condition.wait(lock, [this]() {
        if (_terminated) {
          return true;
        } else if (
            !_stopped &&
            !_paused &&
            _runnings < _threadPool.getSize() &&
            _searchVisits < _maxVisits) {
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
 * Execute the search.
 * @return Number of playouts
 */
int32_t Player::_evaluate() {
  std::vector<Node*> nodes = {_root};
  bool search_equally = _searchEqually;
  int32_t search_width = _searchCandidateWidth;
  int32_t search_algorithm = _searchAlgorithm;
  int32_t search_check_node_depth = _searchCheckNodeDepth;
  float search_temperature = _searchTemperature;
  float search_noise = _searchNoise;
  int32_t playouts = 0;

  // Acquire the mate search engine object
  DfpnEngine* dfpn_engine = _dfpnEnginePool.acquire();

  while (true) {
    NodeResult result = nodes.back()->evaluate(
        search_equally, search_width, search_algorithm,
        dfpn_engine, search_check_node_depth > 0 ? _checkSearchDepth : 0,
        search_temperature, search_noise);

    // Update the evaluation value of the node
    // If the leaf node is reached (playout count is 1), update the evaluation value
    // Start updating from the leaf node and update the minimax evaluation value if necessary
    if (result.getPlayouts() == 1) {
      bool minimax_update = true;

      for (int32_t i = nodes.size() - 1; i >= 0; i--) {
        std::vector<Node*> children = nodes[i]->getChildren();
        int32_t color = nodes[i]->getColor();
        float minimax_value = result.getValue();

        if (!minimax_update) {
          minimax_value = nodes[i]->getMinimax();
        } else if (children.size() > 1) {
          float value = -2.0f;

          for (Node* child : children) {
            float child_minimax = child->getMinimax() * color;

            if (value < child_minimax) {
              value = child_minimax;
            }
          }

          minimax_value = value * color;

          if (std::fabs(minimax_value - nodes[i]->getMinimax()) < 1e-6) {
            minimax_update = false;
          }
        }

        nodes[i]->updateValue(result.getValue(), minimax_value);
      }
    }

    // If only leaf nodes are evaluated and child nodes are created for a leaf node,
    // cancel the evaluation value registered in the parent node
    if (_evalLeafOnly && result.getPlayouts() == -1) {
      for (Node* node : nodes) {
        node->cancelValue(result.getValue());
      }
    }

    // Update the node's playout count
    for (Node* node : nodes) {
      node->setPlayouts(node->getPlayouts() + result.getPlayouts());
    }

    // Update the playout count for this search
    playouts += result.getPlayouts();

    // If a child node exists, set it as the next node
    if (result.getNode() != nullptr) {
      nodes.push_back(result.getNode());
    } else {
      break;
    }

    // Update the configuration items
    search_equally = 0;
    search_width = 0;
    search_algorithm = SEARCH_PUCB;
    search_check_node_depth -= 1;
    search_temperature = 1.0f;
    search_noise = 0.0f;
  }

  // Return the mate search engine object
  _dfpnEnginePool.release(dfpn_engine);

  // Return the number of playouts
  return playouts;
}

/**
 * Release node objects other than the root node.
 * @param node Node object to release
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

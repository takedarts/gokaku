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
 * Create a player object.
 * @param processor Object that performs inference
 * @param threads Number of threads
 * @param nyugyokuScoreBlack Points required for black's entering king declaration
 * @param nyugyokuScoreWhite Points required for white's entering king declaration
 * @param drawSteps Number of moves until a draw
 * @param checkSearchDepth Depth for mate search
 * @param checkSearchNode Number of nodes for mate search
 * @param evalLeafOnly True if only leaf nodes are evaluated
 * @param maxVisits Maximum number of visits for search
 */
Player::Player(
    Processor* processor, int32_t threads,
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps,
    int32_t checkSearchDepth, int32_t checkSearchNode,
    bool evalLeafOnly, int32_t maxVisits)
    : _mutex(),
      _condition(),
      _nodeManager(NodeParameter(
          processor, nyugyokuScoreBlack, nyugyokuScoreWhite, drawSteps,
          checkSearchDepth, checkSearchNode)),
      _threadPool(threads),
      _thread(),
      _root(_nodeManager.createNode()),
      _evalLeafOnly(evalLeafOnly),
      _maxVisits(maxVisits),
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
 * @param equally True to make the number of searches equal, false to use UCB1 or PUCB
 * @param useUcb1 True to use UCB1 as the search criterion, false to use PUCB
 * @param candidateWidth Search width for candidate moves (if 0, the width is automatically adjusted)
 * @param checkNodeDepth Maximum depth of nodes for mate search
 * @param temperature Temperature parameter for search
 * @param noise Strength of Gumbel noise for search
 */
void Player::startEvaluation(
    bool equally, bool useUcb1, int32_t candidateWidth, int32_t checkNodeDepth,
    float temperature, float noise) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the thread
  _paused = true;
  _condition.wait(lock, [this]() { return _runnings == 0; });

  // Change the search settings
  _searchVisits = _root->getVisits();
  _searchPlayouts = _root->getPlayouts();
  _searchEqually = equally;
  _searchUseUcb1 = useUcb1;
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
  if (_root->getCheckMove() != MOVE_PASS) {
    candidates.emplace_back(
        _root->getCheckMove(), _root->getColor(),
        _root->getVisits() - 1, _root->getPlayouts(),
        1.0f, _root->getColor());
  }
  // If there is no mate move, the list of child nodes is considered as candidates
  // If a child node has a mate move, set the evaluation value to opponent's win
  // If there is no mate move, set the evaluation value to node's value * 0.999
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

  // If there are no candidate moves, add a move from the PolicyNetwork
  if (candidates.empty()) {
    candidates.emplace_back(
        _root->getPolicyMove(), _root->getColor(), 0, 0, 1.0f, _root->getValue());
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
  bool search_use_ucb1 = _searchUseUcb1;
  int32_t search_check_depth = _searchCheckNodeDepth;
  float search_temperature = _searchTemperature;
  float search_noise = _searchNoise;
  int32_t playouts = 0;

  while (true) {
    NodeResult result = nodes.back()->evaluate(
        search_equally, search_width, search_use_ucb1, search_check_depth > 0,
        search_temperature, search_noise);

    // Update the node's evaluation value
    if (result.getPlayouts() == 1) {
      for (Node* node : nodes) {
        node->updateValue(result.getValue());
      }
    } else if (result.getPlayouts() == -1 && _evalLeafOnly) {
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

    // Reset settings that apply only to the root node
    search_equally = 0;
    search_width = 0;
    search_use_ucb1 = false;
    search_check_depth -= 1;
    search_temperature = 1.0f;
    search_noise = 0.0f;
  }

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

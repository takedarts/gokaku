#include "Player.h"

#include <sstream>

namespace deepshogi {

// SFEN of the initial board used in the Player class constructor
constexpr char DEFAULT_SFEN[] = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

/**
 * Creates a player object.
 * @param processor Object that performs inference
 * @param threads Number of threads
 * @param searchMaxVisits Maximum visit count for a node
 * @param nyugyokuScoreBlack Score required for first-player nyugyoku declaration
 * @param nyugyokuScoreWhite Score required for second-player nyugyoku declaration
 * @param drawTurn Number of moves until draw
 * @param checkSearchDepth Search depth for checkmate sequences
 * @param checkSearchNode Number of search nodes for checkmate sequences
 * @param checkNodeDepth Maximum depth of nodes to perform checkmate search
 * @param pucbConstantInit Initial value of the constant multiplied by the PUCB confidence upper bound
 * @param pucbConstantBase Change value of the constant multiplied by the PUCB confidence upper bound
 */
Player::Player(
    InferenceProcessor* processor, int32_t threads, int32_t searchMaxVisits,
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
    int32_t checkSearchDepth, int32_t checkSearchNode, int32_t checkNodeDepth,
    float pucbConstantInit, float pucbConstantBase)
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
          pucbConstantInit, pucbConstantBase)),
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
 * Destroys the player object.
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
 * Initializes the state of the player object.
 * @param sfen SFEN of the initial position
 */
void Player::initialize(const std::string& sfen) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the threads
  _paused = true;
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // Save the current root node
  MctsNode* old_root = _root;

  // Set the initial node as the root node
  _root = _manager.createNode();
  _root->initialize(sfen);

  // Release the search tree including the old root node
  _manager.releaseTree(old_root);

  // Resume the threads
  _paused = false;
  _searchCondition.notify_one();
}

/**
 * Gets the next turn color.
 * @return Turn color
 */
int32_t Player::getColor() {
  std::lock_guard<std::mutex> lock(_mutex);
  return _root->getBoard().getColor();
}

/**
 * Moves a piece according to the specified move.
 * @param move Information about the piece to move
 */
void Player::play(const Move& move) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the threads
  _paused = true;
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // Save the current root node
  MctsNode* old_root = _root;

  // Set the new root node
  _root = old_root->getChild(move);
  _root->setAsRootNode();

  // Detach the new root node from the old root node
  old_root->removeChild(move);

  // Release nodes other than the root node
  _manager.releaseTree(old_root);

  // Resume the threads
  _paused = false;
  _searchCondition.notify_one();
}

/**
 * Starts board evaluation.
 * Search processing is executed on a separate thread.
 * @param equally true to equalize search visit count, false to use UCB or PUCB
 * @param candidateWidth Search width for candidate moves (0 means automatic adjustment)
 * @param temperature Temperature parameter for search
 * @param noise Strength of Gumbel noise for search
 */
void Player::startEvaluation(
    bool equally, int32_t candidateWidth, float temperature, float noise) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the threads
  _paused = true;
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // Update the search settings
  _searchEqually = equally;
  _searchCandidateWidth = candidateWidth;
  _searchTemperature = temperature;
  _searchNoise = noise;

  // Set to running state
  _stopped = false;

  // Resume the threads
  _paused = false;
  _searchCondition.notify_one();
}

/**
 * Waits until the search completes.
 * @param visits Search visit count
 * @param playouts Search playout count
 * @param timelimit Time to wait (seconds)
 * @param stop true to stop the search
 */
void Player::waitEvaluation(int32_t visits, int32_t playouts, float timelimit, bool stop) {
  std::unique_lock<std::mutex> lock(_mutex);

  // If 0 or more is specified for visit count or playout count,
  // wait until the root node's visit count reaches 1 or more
  if (visits > 0 || playouts > 0) {
    _stopCondition.wait(lock, [this]() {
      return _root->getVisits() > 0;
    });
  }

  // Wait until the specified visit count and playout count are reached
  std::chrono::milliseconds timeout(static_cast<int32_t>(timelimit * 1000.0f));

  _stopCondition.wait_for(lock, timeout, [this, visits, playouts]() {
    return _root->getVisits() >= visits && _root->getPlayouts() >= playouts;
  });

  // Stop the search
  _stopped = _stopped || stop;
}

/**
 * Gets the list of candidate moves.
 * @return List of candidate moves
 */
std::vector<Candidate> Player::getCandidates() {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the threads
  _paused = true;
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // Build the list of candidate moves
  std::vector<Candidate> candidates;

  // If a checkmate move exists, use only the checkmate move as the candidate
  std::vector<Move> checkmate_moves = _root->getCheckmateMoves();

  if (!checkmate_moves.empty()) {
    candidates.emplace_back(
        checkmate_moves[0], _root->getBoard().getColor(),
        _root->getVisits() - 1, _root->getPlayouts(),
        1.0f, _root->getBoard().getColor(),
        checkmate_moves);
  }
  // If no checkmate sequence exists, use the child node list as candidates
  // If a child node has a checkmate sequence, set the evaluation value to opponent win
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

  // If no candidates exist, add a move based on PolicyNetwork
  if (candidates.empty()) {
    Move policy_move = _root->getPolicyMove();

    if (policy_move != MOVE_INVALID) {
      candidates.emplace_back(
          _root->getPolicyMove(), _root->getBoard().getColor(),
          0, 0, 1.0f, _root->getMctsValue());
    }
  }

  // Resume the threads
  _paused = false;
  _searchCondition.notify_one();

  return candidates;
}

/**
 * Copies the board state to the specified board object.
 * @param board Board object
 */
void Player::copyBoardTo(Board* board) {
  std::unique_lock<std::mutex> lock(_mutex);
  board->copyFrom(&_root->getBoard());
}

/**
 * Gets a string representing the state of the player object.
 * @return String representing the state of the player object
 */
std::string Player::toString() {
  std::unique_lock<std::mutex> lock(_mutex);
  std::stringstream ss;

  // Pause the threads
  _paused = true;
  _stopCondition.wait(lock, [this]() {
    return _runnings == 0 && _evaluatingNodes.empty() && _checkingNodes.empty();
  });

  // Convert the board state to a string
  ss << "--- Board ---" << std::endl
     << _root->getBoard() << std::endl;

  // Traverse the search tree in depth-first order and convert the current state to a string
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

  // Resume the threads
  _paused = false;
  _searchCondition.notify_one();

  return ss.str();
}

/**
 * Executes search.
 */
void Player::_runSearch() {
  // Calculate the maximum number of evaluating nodes
  const int32_t max_evaluating_size =
      _processor->getBatchSize() * _processor->getThreadSize() * 10;

  // Create a variable to save the root node pointer
  // Used to detect changes in the root node
  MctsNode* last_root_node = nullptr;

  while (true) {
    // Node for which to execute checkmate search
    MctsNode* checkmate_search_node = nullptr;

    {
      std::unique_lock<std::mutex> lock(_mutex);

      // Wait until search processing becomes executable
      // Conditions for search processing to become executable (any of the following):
      // - [Stop] Termination is requested, no running threads,
      //   no evaluating nodes, no checkmate-search-waiting nodes
      // - [Checkmate search] There are checkmate-search-waiting nodes
      // - [Move search] No termination, stop, or pause request,
      //   running thread count < thread pool size,
      //   evaluating node count < max evaluating node count, search count < max visit count
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

      // If the stop condition is met, exit the loop
      if (_terminated && _runnings == 0 &&
          _evaluatingNodes.empty() && _checkingNodes.empty()) {
        break;
      }

      // If the root node has changed,
      // add nodes up to the specified depth that have not yet had checkmate search performed
      // to the checkmate search waiting queue
      if (last_root_node != _root) {
        last_root_node = _root;

        // Check all nodes up to the specified depth using depth-first search
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

      // If there are checkmate-search-waiting nodes, extract one
      if (!_checkingNodes.empty()) {
        checkmate_search_node = _checkingNodes.front();
        _checkingNodes.pop();
      }

      // Otherwise, execute search processing
      _runnings += 1;
    }

    // If there is a checkmate search node, register the checkmate search processing in the thread pool
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
    // Otherwise, register the search tree expansion processing in the thread pool
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
 * Expands the search tree.
 */
void Player::_runExpand() {
  // Copy the search settings to local variables
  bool search_equally = _searchEqually;
  int32_t search_width = _searchCandidateWidth;
  float search_temperature = _searchTemperature;
  float search_noise = _searchNoise;

  // Start search from the root node
  // Traverse the search tree while getting the next node to evaluate
  MctsNode* node = nullptr;
  MctsNode* next_node = _root;
  int32_t depth = 0;

  while (true) {
    // Get the next node to evaluate
    node = next_node;
    next_node = node->pickupNextNode(
        search_equally, search_width, search_temperature, search_noise);

    // If the next node to evaluate does not exist, end the search
    if (next_node == nullptr) {
      break;
    }

    // Update the search settings
    search_equally = false;
    search_width = 0;
    search_temperature = 1.0f;
    search_noise = 0.0f;
    depth += 1;
  }

  // If not yet evaluated
  if (!node->isEvaluated()) {
    // Register the node as a target in the board evaluation inference model
    _processor->submit(node, [this](MctsNode* node) {
      std::unique_lock<std::mutex> lock(_mutex);
      _updateCondition.notify_one();
    });

    // If the node is shallower than the specified depth and checkmate search has not been performed,
    // add the node to the checkmate search waiting queue
    if (depth < _checkNodeDepth && !node->isCheckmateSearched()) {
      std::unique_lock<std::mutex> lock(_mutex);
      _checkingNodes.push(node);
      _searchCondition.notify_one();
    }
  }

  // Add the node to the list of evaluating nodes
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _evaluatingNodes.push(node);
    _updateCondition.notify_one();
  }
}

/**
 * Executes checkmate search.
 */
void Player::_runCheckmateSearch(MctsNode* node) {
  PnSearchEngine* engine = _pnsearch.acquire();
  int32_t remain_turn = node->getBoard().getDrawTurn() - node->getBoard().getTurn() + 1;
  int32_t search_depth = std::min(_checkSearchDepth, remain_turn);

  node->searchCheckmateMoves(engine, search_depth);
  _pnsearch.release(engine);
}

/**
 * Updates node state.
 */
void Player::_runUpdate() {
  while (true) {
    std::vector<MctsNode*> finished_nodes;

    {
      std::unique_lock<std::mutex> lock(_mutex);

      // Wait until update processing becomes executable
      // Conditions for update processing to become executable (any of the following):
      // - [Stop] Termination is requested, no running threads,
      //   no evaluating nodes, no checkmate-search-waiting nodes
      // - [Evaluation] There are evaluating nodes and their evaluation is complete
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

      // If the stop condition is met, exit the loop
      if (_terminated && _runnings == 0 &&
          _evaluatingNodes.empty() && _checkingNodes.empty()) {
        break;
      }

      // Extract evaluated nodes
      while (!_evaluatingNodes.empty() && _evaluatingNodes.front()->isEvaluated()) {
        finished_nodes.push_back(_evaluatingNodes.front());
        _evaluatingNodes.pop();
      }
    }

    // Update statistics of evaluated nodes
    // For nodes where a checkmate sequence has been found, set the evaluation value to NodeValue
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

    // Notify the search processing
    _searchCondition.notify_one();
    _stopCondition.notify_all();
  }
}

}  // namespace deepshogi

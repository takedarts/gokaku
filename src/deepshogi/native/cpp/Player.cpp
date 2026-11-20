#include "Player.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace deepshogi {

// SFEN of the initial board used in the Player class constructor
constexpr char DEFAULT_SFEN[] = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1";

/**
 * Create the player object.
 * @param processor Inference processor.
 * @param threads Number of threads.
 * @param searchMaxVisits Maximum node visit count.
 * @param nyugyokuScoreBlack Points Black needs for nyugyoku (from -128 to 127).
 * @param nyugyokuScoreWhite Points White needs for nyugyoku (from -128 to 127).
 * @param sennichitePenalty Penalty assigned to repetition evaluations.
 * @param checkSearchDepth Checkmate search depth.
 * @param checkSearchNode Checkmate search node capacity.
 * @param checkNodeDepth Maximum node depth at which to search for checkmate.
 * @param pucbConstantInit Initial PUCB exploration coefficient.
 * @param pucbConstantBase Base controlling the PUCB exploration coefficient.
 * @param pucbMinVisitsRate Minimum visit ratio for prioritizing PUCB children.
 */
Player::Player(
    InferenceProcessor* processor, int32_t threads, int32_t searchMaxVisits,
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, float sennichitePenalty,
    int32_t checkSearchDepth, int32_t checkSearchNode, int32_t checkNodeDepth,
    float pucbConstantInit, float pucbConstantBase, float pucbMinVisitsRate)
    : _mutex(),
      _searchCondition(),
      _updateCondition(),
      _stopCondition(),
      _waitCondition(),
      _processor(processor),
      _pnsearch(checkSearchNode, threads),
      _threadPool(threads),
      _searchThread(),
      _updateThread(),
      _defaultNyugyokuScoreBlack(nyugyokuScoreBlack),
      _defaultNyugyokuScoreWhite(nyugyokuScoreWhite),
      _manager(MctsParameter(
          sennichitePenalty, pucbConstantInit, pucbConstantBase, pucbMinVisitsRate)),
      _root(nullptr),
      _searchMaxVisits(std::max(searchMaxVisits, 1)),
      _maxEvaluatingSize(0),
      _checkSearchDepth(checkSearchDepth),
      _checkNodeDepth(checkNodeDepth),
      _searchEqually(false),
      _searchCandidateWidth(0),
      _searchTemperature(1.0f),
      _searchNoise(0.0f),
      _runnings(0),
      _updatingNodes(0),
      _pendingInferences(0),
      _paused(false),
      _stopped(true),
      _stopRequested(false),
      _stopSequence(0),
      _terminated(false),
      _canceled(false),
      _checkmateRescan(true),
      _evaluatingNodes(),
      _checkingNodes() {
  // Reject a missing processor before creating the root or starting search threads.
  if (processor == nullptr) {
    throw std::invalid_argument("processor must not be null.");
  }

  // Validate the evaluation queue limit in a wider type and reuse it during search.
  const int64_t evaluating_size =
      static_cast<int64_t>(processor->getBatchSize()) * processor->getThreadSize();

  if (evaluating_size > std::numeric_limits<int32_t>::max() / 10) {
    throw std::invalid_argument("Evaluation queue size exceeds int32 range.");
  }

  _maxEvaluatingSize = static_cast<int32_t>(evaluating_size * 10);

  // Create the board and start search and update threads after validating settings.
  _root = _manager.createNode();
  _root->initialize(DEFAULT_SFEN, 0x7fff, _defaultNyugyokuScoreBlack, _defaultNyugyokuScoreWhite);
  _searchThread = std::thread(&Player::_runSearch, this);
  _updateThread = std::thread(&Player::_runUpdate, this);
}

/**
 * Destroys the player object.
 */
Player::~Player() {
  {
    std::lock_guard<std::mutex> lock(_mutex);
    // Abort PN searches and wait for running tasks and inferences to be collected.
    _stopRequested = true;
    _cancelSearch();
    _terminated = true;
  }

  _searchCondition.notify_one();
  _updateCondition.notify_one();
  _searchThread.join();
  _updateThread.join();
}

/**
 * Initializes the state of the player object with the nyugyoku scores given at construction.
 * @param sfen SFEN of the initial position
 * @param drawTurn Total move count at which the game is drawn.
 */
void Player::initialize(const std::string& sfen, int32_t drawTurn) {
  initialize(sfen, drawTurn, _defaultNyugyokuScoreBlack, _defaultNyugyokuScoreWhite);
}

/**
 * Initializes the state of the player object with the specified nyugyoku scores.
 * @param sfen SFEN of the initial position
 * @param drawTurn Total move count at which the game is drawn.
 * @param nyugyokuScoreBlack Points Black needs for nyugyoku (from -128 to 127).
 * @param nyugyokuScoreWhite Points White needs for nyugyoku (from -128 to 127).
 */
void Player::initialize(const std::string& sfen, int32_t drawTurn,
                        int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Initialize the new position before changing the original board or execution state.
  MctsNode* new_root = _manager.createNode();

  try {
    new_root->initialize(sfen, drawTurn, nyugyokuScoreBlack, nyugyokuScoreWhite);
  } catch (...) {
    // Return only the failed temporary node and preserve the original search tree.
    _manager.releaseNode(new_root);
    throw;
  }

  // Pause the threads.
  _paused = true;
  _cancelSearch();
  _stopCondition.wait(lock, [this]() {
    return _isSearchIdle();
  });

  // Save the current root node.
  MctsNode* old_root = _root;

  // Set the initial node as the root.
  _root = new_root;

  // Release the search tree including the old root node
  _manager.releaseTree(old_root);

  // Resume the threads
  _paused = false;
  _canceled.store(false, std::memory_order_release);
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

  // Save paused and canceled states for restoration if the move fails.
  const bool paused = _paused;
  const bool canceled = _canceled.load(std::memory_order_acquire);

  // Pause the threads.
  _paused = true;
  _cancelSearch();

  // Keep the original root until the move succeeds.
  MctsNode* new_root = nullptr;

  try {
    _stopCondition.wait(lock, [this]() {
      return _isSearchIdle();
    });

    new_root = _root->getChild(move);
    new_root->takeRepetitionHistory(_root);
  } catch (...) {
    // Return a temporary node that could not become the root; keep expanded children in the tree.
    if (new_root != nullptr && new_root->getParent() == nullptr) {
      _manager.releaseNode(new_root);
    }

    // Preserve stops received while waiting and resume if the search was runnable.
    _paused = paused;
    _canceled.store(canceled || _stopRequested, std::memory_order_release);
    _searchCondition.notify_one();

    throw;
  }

  // Save the current root node.
  MctsNode* old_root = _root;

  // Set the new root node.
  _root = new_root;
  _root->setAsRootNode();

  // Detach the new root from the old root.
  old_root->removeChild(move);

  // Release nodes other than the root node
  _manager.releaseTree(old_root);

  // Reset the search settings.
  _searchEqually = false;
  _searchCandidateWidth = 0;
  _searchTemperature = 1.0f;
  _searchNoise = 0.0f;

  // Resume the threads
  _paused = false;
  _canceled.store(false, std::memory_order_release);
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

  // Pause the threads.
  _paused = true;
  _cancelSearch();
  _stopCondition.wait(lock, [this]() {
    return _isSearchIdle();
  });

  // Update the search settings.
  _searchEqually = equally;
  _searchCandidateWidth = candidateWidth;
  _searchTemperature = temperature;
  _searchNoise = noise;

  // Set to running state
  _stopped = false;
  _stopRequested = false;

  // Resume the threads
  _paused = false;
  _canceled.store(false, std::memory_order_release);
  _searchCondition.notify_one();
}

/**
 * Wait for the search to finish.
 * @param visits Target search visit count.
 * @param timelimit Maximum wait time in seconds.
 * @param stop True to stop the search.
 */
void Player::waitEvaluation(int32_t visits, float timelimit, bool stop) {
  std::unique_lock<std::mutex> lock(_mutex);

  // Distinguish a search that has not started from one stopped after waiting began.
  const uint64_t stop_sequence = _stopSequence;

  // Wait for a first visit or checkmate, also waking when stopped during the wait.
  if (visits > 0) {
    _waitCondition.wait(lock, [this, stop_sequence]() {
      return _stopRequested || _stopSequence != stop_sequence || _root->getVisits() > 0 ||
             !_root->getCheckmateMoves().empty();
    });
  }

  // Wait until any of the following conditions is met.
  // [Condition 0] Visits reach the maximum visit count.
  // [Condition 1] A root or candidate visit target is met.
  // [Condition 2] The root discovers a checkmating sequence.
  // [Condition 3] The specified time elapses.
  std::chrono::milliseconds timeout(static_cast<int32_t>(timelimit * 1000.0f));

  _waitCondition.wait_for(
      lock, timeout, [this, visits, stop_sequence]() {
        if (_stopped || _stopRequested || _stopSequence != stop_sequence) {
          return true;
        } else if (_root->getVisits() >= _searchMaxVisits) {
          return true;
        } else if (_root->getVisits() >= visits) {
          return true;
        } else if (_root->getPvVisits() > static_cast<float>(visits) * 0.6f) {
          return true;
        } else if (!_root->getCheckmateMoves().empty()) {
          return true;
        } else {
          return false;
        }
      });

  // Do not let an older wait stop a search restarted after a stop.
  if (stop && _stopSequence == stop_sequence) {
    _stopped = true;
    _stopSequence += 1;
    _cancelSearch();
    lock.unlock();
    _waitCondition.notify_all();
  }
}

/**
 * Request stopping and wake threads waiting for search results.
 */
void Player::stopEvaluation() {
  {
    // Stop new searches and cancel active tree traversal.
    std::lock_guard<std::mutex> lock(_mutex);
    _stopRequested = true;
    _stopped = true;
    _stopSequence += 1;
    _cancelSearch();
  }

  // Wake both first-visit and timed result waits.
  _waitCondition.notify_all();
  _searchCondition.notify_all();
}

/**
 * Gets the list of candidate moves.
 * @return List of candidate moves
 */
std::vector<Candidate> Player::getCandidates() {
  std::unique_lock<std::mutex> lock(_mutex);

  // Pause the threads.
  _paused = true;
  _cancelSearch();

  _stopCondition.wait(lock, [this]() {
    return _isSearchIdle();
  });

  // Build the candidate list.
  std::vector<Candidate> candidates;

  // If a checkmate is known, use only its first move as a candidate.
  std::vector<Move> checkmate_moves = _root->getCheckmateMoves();

  if (!checkmate_moves.empty()) {
    candidates.emplace_back(
        checkmate_moves[0], _root->getBoard().getColor(),
        std::max(_root->getVisits() - 1, 0),
        1.0f, _root->getBoard().getColor(),
        static_cast<float>(checkmate_moves.size()), checkmate_moves);
  }
  // Otherwise, build candidates from the child nodes.
  // If a child has a checkmate, evaluate it as a win for the opponent.
  else {
    for (MctsNode* node : _root->getChildren()) {
      std::vector<Move> node_checkmate_moves = node->getCheckmateMoves();

      if (!node_checkmate_moves.empty()) {
        candidates.emplace_back(
            node->getMove(), _root->getBoard().getColor(),
            node->getVisits(),
            node->getProbability(), node->getBoard().getColor(),
            static_cast<float>(node_checkmate_moves.size()),
            node->getVariations());
      } else {
        candidates.emplace_back(
            node->getMove(), _root->getBoard().getColor(),
            node->getVisits(),
            node->getProbability(), node->getMctsValue(),
            node->getRemainingTurns(), node->getVariations());
      }
    }
  }

  // If no candidates exist, add the policy's selected move.
  if (candidates.empty()) {
    Move fallback = _root->getPolicyMove();

    if (fallback != MOVE_INVALID) {
      candidates.emplace_back(
          fallback, _root->getBoard().getColor(),
          0, 1.0f, _root->getMctsValue(),
          std::max(_root->getRemainingTurns() - 1.0f, 0.0f));
    }
  }

  // Resume the threads
  _paused = false;
  _canceled.store(false, std::memory_order_release);
  _searchCondition.notify_one();

  return candidates;
}

/**
 * Return the policy and search evaluation of the current position.
 * @param value Output for the current position's search evaluation from Black's perspective.
 * @param remainingTurns Output for the predicted moves remaining from the current position.
 * @return Moves and their predicted probabilities.
 */
std::vector<std::pair<Move, float>> Player::getPolicyMoves(float& value, float& remainingTurns) {
  // Prevent the root from changing and read the evaluation of the current position
  std::lock_guard<std::mutex> lock(_mutex);
  return _root->getPolicyMoves(value, remainingTurns);
}

/**
 * Gets the visit count of the root node.
 * @return Visit count of the root node
 */
int32_t Player::getVisits() {
  std::unique_lock<std::mutex> lock(_mutex);
  return _root->getVisits();
}

/**
 * Copies the board state to the specified board object.
 * @param board Destination for the current position
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

  // Pause the threads.
  _paused = true;
  _cancelSearch();
  _stopCondition.wait(lock, [this]() {
    return _isSearchIdle();
  });

  // Convert the board state to a string.
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
       << "MctsValue=" << std::fixed << std::setprecision(2) << current->getMctsValue() << ", "
       << "Checkmate=" << (!current->getCheckmateMoves().empty() ? "Yes" : "No")
       << std::endl;

    std::vector<MctsNode*> children = current->getChildren();
    for (auto it = children.rbegin(); it != children.rend(); it++) {
      stack.emplace_back(*it, prefix + "  ");
    }
  }

  // Resume the threads
  _paused = false;
  _canceled.store(false, std::memory_order_release);
  _searchCondition.notify_one();

  return ss.str();
}

/**
 * Run the search.
 */
void Player::_runSearch() {
  while (true) {
    // Node for which to execute checkmate search
    MctsNode* checkmate_search_node = nullptr;

    {
      std::unique_lock<std::mutex> lock(_mutex);

      // While stopped, only clean up the queue and exit; register tasks only while runnable
      _searchCondition.wait(lock, [this]() {
        if (_terminated && _isSearchIdle()) {
          return true;
        }

        if (_terminated || _stopRequested || _stopped || _paused) {
          return !_checkingNodes.empty();
        }

        return _runnings < _threadPool.getSize() &&
               (_checkmateRescan || !_checkingNodes.empty() ||
                (_evaluatingNodes.size() < _maxEvaluatingSize &&
                 _root->getVisits() < _searchMaxVisits));
      });

      // Also collect queue entries added concurrently with a stop and notify the update thread
      if (_terminated || _stopRequested || _stopped || _paused) {
        _cancelSearch();

        if (_terminated && _isSearchIdle()) {
          break;
        }

        continue;
      }

      // After resuming, re-register aborted or unstarted checkmate searches even for the same root
      // Add nodes within the specified depth whose checkmate search has not run
      // to the checkmate search waiting queue
      if (_checkmateRescan) {
        _checkmateRescan = false;

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

      // Wait for the next notification when nothing was re-registered and MCTS cannot run
      if (checkmate_search_node == nullptr &&
          (_evaluatingNodes.size() >= _maxEvaluatingSize ||
           _root->getVisits() >= _searchMaxVisits)) {
        continue;
      }

      _runnings += 1;
    }

    // Submit queued checkmate searches to the thread pool.
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
 * Abort running searches and defer unstarted PN searches to re-registration on the next resume.
 * Call this function while holding _mutex.
 */
void Player::_cancelSearch() {
  // Keep the abort request until running tasks are collected to guarantee node lifetimes
  _canceled.store(true, std::memory_order_release);
  _pnsearch.abortSearch();
  _checkmateRescan = true;

  while (!_checkingNodes.empty()) {
    _checkingNodes.pop();
  }

  // Also wake search, update, and collection waiters that can finish only by clearing the queue
  _searchCondition.notify_one();
  _updateCondition.notify_one();
  _stopCondition.notify_all();
}

/**
 * Return true when the search is idle.
 * Call this function while holding the synchronization lock.
 * @return True when the search is idle.
 */
bool Player::_isSearchIdle() const {
  return _runnings == 0 && _updatingNodes == 0 && _pendingInferences == 0 &&
         _evaluatingNodes.empty() && _checkingNodes.empty();
}

/**
 * Expand the search tree.
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
        search_equally, search_width, search_temperature, search_noise,
        [this]() { return _canceled.load(std::memory_order_acquire); });

    // If the search is canceled, terminate the search
    if (next_node == nullptr) {
      return;
    }

    // If there is no next node to evaluate, proceed to node evaluation
    if (next_node == node) {
      break;
    }

    // Update the search settings
    search_equally = false;
    search_width = 0;
    search_temperature = 1.0f;
    search_noise = 0.0f;
    depth += 1;
  }

  // If the node has not been evaluated,
  if (!node->isEvaluated()) {
    // submit it to the inference processor.
    _submitInference(node);

    // If the node is below the specified depth and checkmate search has not run,
    // add it to the checkmate search queue.
    if (depth < _checkNodeDepth && !node->isCheckmateSearched()) {
      std::unique_lock<std::mutex> lock(_mutex);
      if (!_stopped && !_stopRequested && !_paused && !_terminated) {
        _checkingNodes.push(node);
        _searchCondition.notify_one();
      }
    }
  }

  // Add the node to the list of evaluating nodes
  {
    std::unique_lock<std::mutex> lock(_mutex);
    _evaluatingNodes.push(node);
    _updateCondition.notify_one();
  }

  // Notify after passing through the waiters' mutex to prevent missed wakeups.
  _waitCondition.notify_all();
}

/**
 * Submit an inference request while tracking callback completion.
 * @param node Node to evaluate.
 */
void Player::_submitInference(MctsNode* node) {
  {
    // Count the request before submission so the evaluated flag alone cannot imply idleness.
    std::lock_guard<std::mutex> lock(_mutex);
    _pendingInferences += 1;
  }

  // Submit outside the lock so a synchronous cache-hit callback can acquire the same mutex.
  try {
    _processor->submit(node, [this](MctsNode*) { _finishInference(); });
  } catch (...) {
    // Restore the pending count if submission fails before the request is accepted.
    _finishInference();
    throw;
  }
}

/**
 * Record inference completion or submission failure and notify waiting threads.
 */
void Player::_finishInference() {
  // Hold the mutex through the last access because the player may be destroyed at zero.
  std::lock_guard<std::mutex> lock(_mutex);
  --_pendingInferences;
  _updateCondition.notify_one();
  _searchCondition.notify_one();
  _stopCondition.notify_all();
}

/**
 * Executes checkmate search.
 * @param node Node whose position is searched for checkmate.
 */
void Player::_runCheckmateSearch(MctsNode* node) {
  {
    // Skip tasks stopped while waiting in the thread pool.
    std::lock_guard<std::mutex> lock(_mutex);
    if (_stopped || _stopRequested || _paused || _terminated) {
      return;
    }
  }

  // Use the PN search engine to search for long-sequence checkmate moves
  PnSearchEngine* engine = _pnsearch.acquire();
  {
    // Do not start a new search even if a stop notification completed before the engine was lent
    std::lock_guard<std::mutex> lock(_mutex);
    if (_stopped || _stopRequested || _paused || _terminated) {
      _pnsearch.release(engine);
      return;
    }
  }

  // Determine the initial PN search depth from the setting and the moves left before the limit
  int32_t remain_turn = node->getBoard().getDrawTurn() - node->getBoard().getTurn() + 1;
  int32_t search_depth = std::min(_checkSearchDepth, remain_turn);

  // Search for the checkmate sequence synchronously with the lent engine
  try {
    node->searchCheckmateMoves(engine, search_depth);
  } catch (...) {
    // Do not leave the engine lent even if the node throws an exception
    _pnsearch.release(engine);
    throw;
  }

  // Return the engine to the pool after the search completes or is aborted
  _pnsearch.release(engine);

  // After releasing the node lock, synchronize mate notification with the waiters' mutex.
  bool notify_waiters;
  {
    std::lock_guard<std::mutex> lock(_mutex);
    notify_waiters = node == _root && !node->getCheckmateMoves().empty();
  }

  if (notify_waiters) {
    _waitCondition.notify_all();
  }
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
      // Updates can proceed under any of the following conditions.
      // - [Stop] Termination is requested and no threads are running,
      //   with no nodes being evaluated or awaiting checkmate search.
      // - [Evaluation] A pending node has finished evaluation.
      _updateCondition.wait(lock, [this]() {
        if (_terminated && _isSearchIdle()) {
          return true;
        } else if (
            !_evaluatingNodes.empty() && _evaluatingNodes.front()->isEvaluated()) {
          return true;
        } else {
          return false;
        }
      });

      // Exit the loop when the stopping conditions are met.
      if (_terminated && _isSearchIdle()) {
        break;
      }

      // Collect evaluated nodes.
      while (!_evaluatingNodes.empty() && _evaluatingNodes.front()->isEvaluated()) {
        finished_nodes.push_back(_evaluatingNodes.front());
        _evaluatingNodes.pop();
      }

      // Increase the number of nodes being updated.
      _updatingNodes += static_cast<int32_t>(finished_nodes.size());
    }

    // Update statistics for evaluated nodes.
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

    // Decrease the number of nodes being updated.
    {
      std::unique_lock<std::mutex> lock(_mutex);
      _updatingNodes -= static_cast<int32_t>(finished_nodes.size());
    }

    // Notify the search worker.
    _searchCondition.notify_one();
    _stopCondition.notify_all();
  }
}

}  // namespace deepshogi

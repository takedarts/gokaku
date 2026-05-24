#include "MctsNode.h"

#include <algorithm>
#include <random>

namespace deepshogi {

// Used for Policy sampling and Gumbel noise during search
// Thread-local random number generator
thread_local static std::random_device random_seed_gen;
thread_local static std::default_random_engine random_engine(random_seed_gen());

/**
 * Creates an MCTS search node object.
 * @param manager Node management object
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
 * Sets this node as the initial board node specified in SFEN format.
 * @param sfen Board in SFEN format
 */
void MctsNode::initialize(const std::string sfen) {
  std::unique_lock<std::shared_mutex> node_lock(_mutex);

  _resetNode();
  _board.initialize(sfen);
  _move = MOVE_INVALID;
}

/**
 * Applies the specified inference result to the evaluation value and predicted move probability list of this node.
 * @param value Board evaluation value
 * @param policies List of predicted probabilities for the next move
 */
void MctsNode::applyInferenceResult(
    float value, const std::vector<std::pair<Move, float>>& policies) {
  std::unique_lock<std::shared_mutex> lock(_mutex);

  // Update the board evaluation value and predicted move probability list if no checkmate sequence has been found
  if (_checkmateMoves.empty()) {
    // Update the board evaluation value
    _nodeValue = value;

    // Update the predicted move probability list
    _policies.clear();

    for (const auto& policy : policies) {
      _policies.emplace_back(policy.first, policy.second);
    }
  }

  // Mark as evaluated
  _evaluating = false;
  _evaluated = true;

  // Notify threads waiting for evaluation completion
  _condition.notify_all();
}

/**
 * Gets the next node object to evaluate.
 * Returns nullptr if no next node to evaluate exists.
 * Conditions under which no next node exists:
 * - The board has not been evaluated
 * - No legal moves exist
 * - A checkmate move sequence has been found by checkmate search
 * - This is not the root node, and an entering-king declaration is possible or the draw move count has been reached
 * @param equally true to equalize the search visit count
 * @param width Search width (0 means automatic adjustment)
 * @param temperature Temperature parameter for search
 * @param noise Strength of Gumbel noise for search
 * @param rootNode true if this node is the root node
 * @return Next node object to evaluate
 */
MctsNode* MctsNode::pickupNextNode(bool equally, int32_t width, float temperature, float noise) {
  std::unique_lock<std::shared_mutex> lock(_mutex);

  // Increment the visit count for reaching this node
  _visits += 1;

  // If this node is being evaluated, wait for evaluation to complete
  if (_evaluating) {
    _condition.wait(lock, [this] { return !_evaluating; });
  }

  // If already evaluated, return the next node to evaluate
  if (_evaluated) {
    // If a next node to evaluate exists, return it
    // The playout count is incremented by the terminal node
    if (!_policies.empty()) {
      return _pickupNextNode(equally, width, temperature, noise);
    }
    // Otherwise, only increment the playout count and return nullptr
    else {
      MctsNode* current_node = this;

      while (current_node != nullptr) {
        current_node->_playouts.fetch_add(1, std::memory_order_relaxed);
        current_node = current_node->_parent;
      }

      return nullptr;
    }
  }

  // Reached an unevaluated node, so increment the playout count
  _playouts.fetch_add(1, std::memory_order_relaxed);

  if (!_firstChild) {
    MctsNode* parent = _parent;

    while (parent) {
      parent->_playouts.fetch_add(1, std::memory_order_relaxed);
      parent = parent->_parent;
    }
  }

  // If no legal moves exist, set this node's state to terminal (loss)
  if (_board.getLegalMoves(true, false).empty()) {
    _nodeValue = static_cast<float>(OPPOSITE_COLOR(_board.getColor()));
    _evaluated = true;
    _policies.clear();

    return nullptr;
  }

  // If not the root node and an entering-king declaration is possible, set this node's state to terminal (win)
  if (_parent != nullptr && _board.isNyugyoku(_board.getColor())) {
    _nodeValue = static_cast<float>(_board.getColor());
    _evaluated = true;
    _policies.clear();

    return nullptr;
  }

  // If not the root node and the maximum move count has been reached, set this node's state to terminal (draw)
  if (_parent != nullptr && _board.getTurn() >= _board.getDrawTurn()) {
    _nodeValue = 0.0f;
    _evaluated = true;
    _policies.clear();

    return nullptr;
  }

  // Search for a 5-move checkmate sequence
  int32_t remain_turn = _board.getDrawTurn() - _board.getTurn() + 1;
  int32_t search_depth = std::min(5, remain_turn);

  _checkmateMoves = _board.getCheckmateMoves(search_depth);

  // If a checkmate sequence is found, set this node's state to terminal (win)
  if (!_checkmateMoves.empty()) {
    _nodeValue = static_cast<float>(_board.getColor());
    _evaluated = true;
    _policies.clear();

    return nullptr;
  }

  // Set this node's state to evaluating
  _evaluating = true;

  return nullptr;
}

/**
 * Performs checkmate search.
 * @param engine Checkmate search engine
 * @param depth Search depth for checkmate search
 */
void MctsNode::searchCheckmateMoves(PnSearchEngine* engine, int32_t depth) {
  {
    std::unique_lock<std::shared_mutex> lock(_mutex);

    // Do nothing if checkmate search has already been performed
    if (_checkmateMoveSearched) {
      return;
    }

    // Set the searched flag
    _checkmateMoveSearched = true;

    // Do nothing if a checkmate sequence has already been found
    if (!_checkmateMoves.empty()) {
      return;
    }
  }

  // Execute checkmate search
  std::vector<Move> checkmateMoves = engine->getCheckmateMoves(&_board, depth);

  // If a checkmate sequence is found, save it
  // Update the evaluation value to win on the next move and clear the candidate move list
  if (!checkmateMoves.empty()) {
    std::unique_lock<std::shared_mutex> lock(_mutex);

    // Save the search result
    _checkmateMoves = checkmateMoves;

    // Update the evaluation value to win on the next move
    _nodeValue = static_cast<float>(_board.getColor());
    _mctsValue.setValue(_nodeValue);

    // Clear the candidate move list
    _policies.clear();
    _waitingPolicies = std::queue<MctsPolicy>();
    _waitingMoves.clear();
  }
}

/**
 * Updates the MCTS evaluation value of this node.
 * @param mctsValue MCTS evaluation value
 */
void MctsNode::updateMctsValue(float mctsValue) {
  _mctsValue.update(mctsValue);
}

/**
 * Sets this node as the root node.
 * This function performs the following:
 * - Removes the parent node
 * - If evaluated and legal moves exist but no moves are registered in the policy,
 *   deletes all child nodes and resets the evaluation and statistics
 */
void MctsNode::setAsRootNode() {
  std::unique_lock<std::shared_mutex> lock(_mutex);

  // Remove the parent node
  _parent = nullptr;

  // Set the move probability to 1.0
  _probability = 1.0f;

  // If evaluated and legal moves exist but no moves are registered in the policy
  if (_evaluated && !_board.getLegalMoves(true, false).empty() && _policies.empty()) {
    // Delete all child nodes of this node
    for (const auto& child : _children) {
      _manager->releaseTree(child.second);
    }

    // Reset the evaluation and statistics of this node to an unevaluated state
    Move move = _move;
    float probability = _probability;

    _resetNode();
    _move = move;
    _probability = probability;
  }
}

/**
 * Returns true if this node's board has been evaluated.
 * @return true if the board has been evaluated
 */
bool MctsNode::isEvaluated() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _evaluated;
}

/**
 * Returns true if checkmate search has been performed on this node.
 * @return true if checkmate search has been performed
 */
bool MctsNode::isCheckmateSearched() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _checkmateMoveSearched || !_checkmateMoves.empty();
}

/**
 * Returns the board evaluation value of this node.
 * @return Board evaluation value
 */
float MctsNode::getNodeValue() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _nodeValue;
}

/**
 * Returns the list of predicted probabilities for the next move of this node.
 * @return List of predicted probabilities for the next move
 */
std::vector<MctsPolicy> MctsNode::getPolicies() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _policies;
}

/**
 * Returns the parent node.
 * @return Parent node
 */
MctsNode* MctsNode::getParent() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _parent;
}

/**
 * Returns the list of child nodes.
 * @return List of child nodes
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
 * Gets the node object for when the specified move is made.
 * If no node object exists, returns a newly created object.
 * The created node object is not registered as a child node of this node object.
 * @param move Move
 * @return Pointer to the node object
 */
MctsNode* MctsNode::getChild(const Move& move) {
  std::unique_lock<std::shared_mutex> lock(_mutex);

  // Return the node if a child node exists
  auto it = _children.find(move.getValue());

  if (it != _children.end()) {
    return it->second;
  }

  // If no child node exists, create a new node object and return it
  // The created node object is not registered as a child node of this node object
  MctsNode* child = _manager->createNode();

  child->_resetNode();
  child->_board.copyFrom(&_board);
  child->_board.play(move);
  child->_move = move;
  child->_probability = 0.0f;

  return child;
}

/**
 * Removes the node object for when the specified move is made from the child node list.
 * @param move Move
 */
void MctsNode::removeChild(const Move& move) {
  std::unique_lock<std::shared_mutex> lock(_mutex);
  _children.erase(move.getValue());
}

/**
 * Gets the visit count of this node.
 * @return Visit count
 */
int32_t MctsNode::getVisits() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _visits;
}

/**
 * Gets the playout count.
 * @return Playout count
 */
int32_t MctsNode::getPlayouts() {
  return _playouts.load(std::memory_order_relaxed);
}

/**
 * Gets the MCTS evaluation value of this node.
 * @return MCTS evaluation value
 */
float MctsNode::getMctsValue() {
  return _mctsValue.getValue(_nodeValue);
}

/**
 * Gets the lower confidence bound of the evaluation value of this node.
 * @return Lower confidence bound
 */
float MctsNode::getMctsValueLCB() {
  return _mctsValue.getValueLCB(OPPOSITE_COLOR(_board.getColor()), _nodeValue);
}

/**
 * Gets the priority of this node based on PUCB.
 * @param totalVisits Total visit count
 * @return Priority
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
 * Gets the checkmate move sequence of this node.
 * Returns an empty array if no checkmate sequence has been found.
 * @return Checkmate move sequence
 */
std::vector<Move> MctsNode::getCheckmateMoves() {
  std::shared_lock<std::shared_mutex> lock(_mutex);
  return _checkmateMoves;
}

/**
 * Gets the expected line of play of this node.
 * @return Expected line of play
 */
std::vector<Move> MctsNode::getVariations() {
  std::vector<Move> variations;
  MctsNode* max_child = nullptr;

  {
    // Acquire lock for synchronization
    std::shared_lock<std::shared_mutex> lock(_mutex);

    // Add this node's move to the expected line of play
    variations.push_back(_move);

    // Build the expected line of play by following the child node with the highest LCB value
    float max_value_lcb = -std::numeric_limits<float>::infinity();

    for (auto child : _children) {
      float child_value_lcb = child.second->getMctsValueLCB() * _board.getColor();

      if (child_value_lcb > max_value_lcb) {
        max_value_lcb = child_value_lcb;
        max_child = child.second;
      }
    }
  }

  // If the best child node exists, append its expected line of play
  if (max_child != nullptr) {
    std::vector<Move> child_variations = max_child->getVariations();
    variations.insert(variations.end(), child_variations.begin(), child_variations.end());
  }

  return variations;
}

/**
 * Gets the candidate move with the highest PolicyNetwork evaluation value.
 * @return Candidate move
 */
Move MctsNode::getPolicyMove() {
  std::shared_lock<std::shared_mutex> lock(_mutex);

  // If a checkmate sequence has been found, return that move
  if (!_checkmateMoves.empty()) {
    return _checkmateMoves[0];
  }

  // If no candidate moves exist, return an invalid move
  if (_policies.empty()) {
    return MOVE_INVALID;
  }

  // Get the candidate move with the highest move probability
  MctsPolicy max_policy = _policies[0];

  for (MctsPolicy policy : _policies) {
    if (max_policy.getProbability() < policy.getProbability()) {
      max_policy = policy;
    }
  }

  // Return the candidate move with the highest move probability
  return max_policy.getMove();
}

/**
 * Initializes the state of this node except for the board object.
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
 * Gets the next node object to evaluate.
 * This function assumes that this node has already been evaluated.
 * @param equally true to equalize the search visit count
 * @param width Search width (0 means automatic adjustment)
 * @param temperature Temperature parameter for search
 * @param noise Strength of Gumbel noise for search
 * @return Next node object to evaluate
 */
MctsNode* MctsNode::_pickupNextNode(bool equally, int32_t width, float temperature, float noise) {
  // If policy candidates remain and there is room in the search width, add a new move as an expansion candidate
  int32_t children_size = (int32_t)(_children.size() + _waitingMoves.size());

  if (children_size < _policies.size() && (width < 1 || children_size < width)) {
    int32_t max_index = 0;
    int max_priority_type = 0;
    float max_priority = 0.0f;

    // Calculate the temperature parameter
    float win_chance = _mctsValue.getValue(_nodeValue) * _board.getColor() * 0.5f + 0.5f;
    float temperature_power =
        win_chance + (1.0f / std::max(temperature, 1e-3f)) * (1 - win_chance);

    // Create a Gumbel noise generator object
    // Do not add noise if the number of child nodes is 4 or fewer
    float noise_scale = (children_size <= 4) ? 0.0f : noise;
    std::extreme_value_distribution<float> noise_dist(0.0f, noise_scale);

    // Select the next candidate based on predicted probability, temperature, Gumbel noise, and unexpanded priority
    for (int i = 0; i < _policies.size(); i++) {
      MctsPolicy& policy = _policies[i];
      float probability = policy.getProbability();

      // Apply the temperature parameter
      probability = std::pow(probability, temperature_power);

      // Add Gumbel noise
      // Since noise is added to the logit, multiply the probability by e^noise
      probability *= std::exp(noise_dist(random_engine));

      // Calculate priority
      int32_t priority_type = 1;
      float priority = probability / (policy.getVisits() + 1);

      // If equally-distributed search is set, lower the priority of already-registered candidates
      if (equally) {
        int32_t policy_index = policy.getMove().getValue();

        if (_children.find(policy_index) != _children.end() ||
            _waitingMoves.find(policy_index) != _waitingMoves.end()) {
          priority_type = 0;
        }
      }

      // Keep the highest-priority candidate
      if (priority_type > max_priority_type ||
          (priority_type == max_priority_type && priority > max_priority)) {
        max_index = i;
        max_priority_type = priority_type;
        max_priority = priority;
      }
    }

    // If the candidate to add for evaluation is not yet registered, add it to the waiting list
    MctsPolicy& max_policy = _policies[max_index];
    int32_t max_policy_index = max_policy.getMove().getValue();

    if (_children.find(max_policy_index) == _children.end() &&
        _waitingMoves.find(max_policy_index) == _waitingMoves.end()) {
      // Create the next board and check whether it is inferior to any ancestor node's board
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

      // If it is an inferior board, remove from candidates and return
      if (lesser_board) {
        _policies.erase(_policies.begin() + max_index);

        return nullptr;
      }

      // Register the candidate to add for evaluation in the waiting list
      _waitingPolicies.push(max_policy);
      _waitingMoves.insert(max_policy_index);
    }

    // Increment visit count
    _policies[max_index].incrementVisits();
  }

  // If no search width is specified or the number of child nodes has not reached the specified width,
  // if there are candidates in the waiting list, create a new child node and return it as the next search target
  if (_waitingPolicies.size() > 0 && (width <= 0 || _children.size() < width)) {
    // Get the first registered waiting candidate
    MctsPolicy policy = _waitingPolicies.front();
    int32_t policy_index = policy.getMove().getValue();

    _waitingPolicies.pop();
    _waitingMoves.erase(policy_index);

    // If the candidate is not yet registered, create a new child node and return it as the next search target
    // Tentatively set the node evaluation value to the minimum evaluation value
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

  // Build the list of child nodes to search
  std::vector<std::pair<MctsNode*, float>> children;

  for (std::pair<int32_t, MctsNode*> child : _children) {
    children.push_back(std::make_pair(
        child.second, child.second->getMctsValueLCB() * _board.getColor()));
  }

  // If a search width is specified, limit the number of child nodes to search
  if (width > 0 && children.size() > width) {
    std::sort(children.begin(), children.end(), [](auto a, auto b) {
      return a.second > b.second;
    });

    children.resize(width);
  }

  // Return the node with the highest priority as the next search target
  MctsNode* max_node = children[0].first;
  float max_priority = -1.0;

  for (std::pair<MctsNode*, float> child : children) {
    // Calculate priority
    float priority;

    // If equally-distributed search is set,
    // calculate priority based on visit count (if equal, consider the evaluation value)
    if (equally) {
      float visits = (float)child.first->getVisits();
      float value = child.first->getMctsValue() * _board.getColor();
      priority = 1.0f / (visits + 1 - value * 0.5f);
    }
    // Otherwise, calculate priority based on PUCB
    else {
      priority = child.first->getPriorityByPUCB(_visits);
    }

    // Keep the highest-priority node
    if (max_priority < priority) {
      max_node = child.first;
      max_priority = priority;
    }
  }

  // Return the node with the highest priority as the next search target
  return max_node;
}

}  // namespace deepshogi

#include "Node.h"

#include <cmath>
#include <random>

#include "NodeManager.h"

namespace deepshogi {

// Random number generator
static std::random_device random_seed_gen;
static std::default_random_engine random_engine(random_seed_gen());

/**
 * Create a search node object.
 * @param manager Node manager object
 * @param parameter Node creation parameters
 */
Node::Node(NodeManager* manager, const NodeParameter& parameter)
    : _evalMutex(),
      _valueMutex(),
      _manager(manager),
      _board(
          parameter.getNyugyokuScoreBlack(),
          parameter.getNyugyokuScoreWhite(),
          parameter.getDrawTurn()),
      _move(MOVE_PASS),
      _policy(0.0f),
      _evaluator(parameter.getProcessor()),
      _ucbConstant(parameter.getUcbConstant()),
      _pucbConstantInit(parameter.getPucbConstantInit()),
      _pucbConstantBase(parameter.getPucbConstantBase()),
      _children(),
      _childPolicies(),
      _waitingQueue(),
      _waitingSet(),
      _checkmateMoves(),
      _checkmateMoveShallowSearched(false),
      _checkmateMoveDeepSearched(false),
      _visits(0),
      _playouts(0),
      _value(0.0f),
      _count(0),
      _minimax(0.0f) {
}

/**
 * Set as the initial board node specified in SFEN format.
 * @param sfen Board in SFEN format
 */
void Node::initialize(const std::string sfen) {
  std::unique_lock<std::shared_mutex> lock(_evalMutex);

  _board.initialize(sfen);
  _move = MOVE_PASS;
  _reset();
}

/**
 * Evaluate the search node and get the next node object to evaluate.
 * If the next node object to evaluate does not exist, return nullptr.
 * @param equally If true, equalize the number of searches
 * @param width Search width (if 0, adjust automatically)
 * @param algorithm Search algorithm
 * @param dfpnEngine Mate search engine object (nullptr if not searching for mate)
 * @param checkSearchDepth Search depth for checkmate moves
 * @param temperature Temperature parameter for search
 * @param noise Strength of Gumbel noise for search
 * @return Evaluation result
 */
NodeResult Node::evaluate(
    bool equally, int32_t width, int32_t algorithm,
    DfpnEngine* dfpnEngine, int32_t checkSearchDepth,
    float temperature, float noise) {
  NodeResult result;

  {
    std::unique_lock<std::shared_mutex> lock(_evalMutex);

    // Execute board evaluation for the node
    _evaluateBoard();

    // Increase the number of visits
    _visits += 1;

    // Evaluate the state of this node
    result = _evaluateNode(equally, width, algorithm, temperature, noise);

    // If not searching for long checkmate sequences, return the evaluation result
    if (dfpnEngine == nullptr ||
        checkSearchDepth < 1 ||
        _checkmateMoveDeepSearched ||
        !_checkmateMoves.empty()) {
      return result;
    }

    // Mark that long checkmate search has been executed
    _checkmateMoveDeepSearched = true;
  }

  // Asynchronously execute long checkmate search
  int32_t remain_turn = _board.getDrawTurn() - _board.getTurn() + 1;
  int32_t search_depth = std::min(checkSearchDepth, remain_turn);
  std::vector<Move> checkmate_moves = dfpnEngine->getCheckmateMoves(&_board, search_depth);

  // Save the result of the checkmate search
  {
    std::unique_lock<std::shared_mutex> lock(_evalMutex);
    _checkmateMoves = checkmate_moves;
  }

  return result;
}

/**
 * Update the evaluation value of the search node.
 * @param value Evaluation value
 * @param minimax Minimax value
 */
void Node::updateValue(float value, float minimax) {
  std::unique_lock<std::shared_mutex> lock(_valueMutex);
  _count += 1;
  _value += value;
  _minimax = minimax;
}

/**
 * Cancel the evaluation value of the search node.
 * @param value Evaluation value
 */
void Node::cancelValue(float value) {
  std::unique_lock<std::shared_mutex> lock(_valueMutex);
  _count -= 1;
  _value -= value;
}

/**
 * Get the candidate move with the highest PolicyNetwork evaluation value.
 * @return Candidate move
 */
Move Node::getPolicyMove() {
  {
    // Execute board evaluation for this node
    std::unique_lock<std::shared_mutex> lock(_evalMutex);
    _evaluateBoard();
  }

  // If a checkmate move has been found, return that move
  if (!_checkmateMoves.empty()) {
    return _checkmateMoves[0];
  }

  // Get the list of candidate moves
  std::vector<Policy> policies;

  {
    std::shared_lock<std::shared_mutex> lock(_evalMutex);
    for (Policy policy : _evaluator.getPolicies()) {
      policies.push_back(policy);
    }
  }

  // If there are no candidate moves, return pass
  if (policies.empty()) {
    return MOVE_PASS;
  }

  // Get the candidate move with the highest move probability
  Policy max_policy = policies[0];

  for (Policy policy : policies) {
    if (max_policy.policy < policy.policy) {
      max_policy = policy;
    }
  }

  // Return the candidate move with the highest move probability
  return max_policy.move;
}

/**
 * Get the move.
 * @return Move
 */
Move Node::getMove() {
  return _move;
}

/**
 * Get the next side to move.
 * @return Side to move
 */
int32_t Node::getColor() {
  return _board.getColor();
}

/**
 * Get the predicted move probability of this node.
 * @return Predicted move probability
 */
float Node::getPolicy() {
  return _policy;
}

/**
 * Get the list of child nodes.
 * @return List of node objects
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
 * Get the node object when the specified move is made.
 * If the node object does not exist, return a newly created object.
 * The created node object is not registered as a child node of this node object.
 * @param move Move
 * @return Pointer to node object
 */
Node* Node::getChild(const Move& move) {
  std::unique_lock<std::shared_mutex> lock(_evalMutex);

  // If a child node exists, return that node
  int32_t index = move.getValue();

  if (_children.find(index) != _children.end()) {
    return _children[index];
  }

  // If a child node does not exist, create a new node
  Node* node = _manager->createNode();

  node->_setAsNextNode(this, move, 1.0);

  return node;
}

/**
 * Get the checkmate moves of this node.
 * If no checkmate moves are found, return an empty array.
 * @return Checkmate moves
 */
std::vector<Move> Node::getCheckmateMoves() {
  std::shared_lock<std::shared_mutex> lock(_evalMutex);
  return _checkmateMoves;
}

/**
 * Get the number of searches for this node.
 * @return Number of searches
 */
int32_t Node::getVisits() {
  std::shared_lock<std::shared_mutex> lock(_evalMutex);
  return _visits;
}

/**
 * Get the number of playouts.
 * @return Number of playouts
 */
int32_t Node::getPlayouts() {
  std::shared_lock<std::shared_mutex> lock(_evalMutex);
  return _playouts;
}

/**
 * Set the number of playouts.
 * @param playouts Number of playouts
 */
void Node::setPlayouts(int32_t playouts) {
  std::unique_lock<std::shared_mutex> lock(_evalMutex);
  _playouts = playouts;
}

/**
 * Get the evaluation value of this node.
 * @return Evaluation value
 */
float Node::getValue() {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  if (!_checkmateMoves.empty()) {
    return _board.getColor();
  } else if (_count == 0) {
    return 0.0f;
  } else {
    return _value / _count;
  }
}

/**
 * Get the minimax evaluation value of this node.
 * @return Minimax evaluation value
 */
float Node::getMinimax() {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  if (!_checkmateMoves.empty()) {
    return _board.getColor();
  } else if (_count == 0) {
    return 0.0f;
  } else {
    return _minimax;
  }
}

/**
 * Get the lower bound of the confidence interval for the evaluation value of this node.
 * @return Lower bound of confidence interval
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
 * Get the priority of this node based on UCB.
 * @param totalVisits Total number of searches
 * @return Priority
 */
float Node::getPriorityByUCB(int32_t totalVisits) {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  if (_count == 0) {
    return -99.0f;
  } else {
    float value = (_value / _count) * OPPOSITE_COLOR(_board.getColor());
    float ucb = std::sqrt(std::log(totalVisits) / (_visits + 1));
    return value + _ucbConstant * ucb;
  }
}

/**
 * Get the priority of this node based on PUCB.
 * @param totalVisits Total number of searches
 * @return Priority
 */
float Node::getPriorityByPUCB(int32_t totalVisits) {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  if (_count == 0) {
    return -99.0f;
  } else {
    float c_pucb_inc = std::log((1 + totalVisits + _pucbConstantBase) / _pucbConstantBase);
    float c_pucb = _pucbConstantInit * (1.0f + c_pucb_inc);
    float value = (_value / _count) * OPPOSITE_COLOR(_board.getColor());
    float ucb = _policy * std::sqrt(totalVisits) / (1 + _visits);
    return value + c_pucb * ucb;
  }
}

/**
 * Get the predicted sequence of this node.
 * @return Predicted sequence
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
 * Copy the state of the board to the specified board object.
 * @param board Board object
 */
void Node::copyBoardTo(Board* board) {
  std::shared_lock<std::shared_mutex> lock(_valueMutex);
  board->copyFrom(&_board);
}

/**
 * Execute board evaluation for this node.
 */
void Node::_evaluateBoard() {
  // If a checkmate move has not been found and not yet searched, execute 5-move checkmate search
  // Use a duplicated board object because the board object is changed during search
  if (!_checkmateMoveShallowSearched && _checkmateMoves.empty()) {
    int32_t remain_turn = _board.getDrawTurn() - _board.getTurn() + 1;
    int32_t search_depth = std::min(5, remain_turn);

    _checkmateMoveShallowSearched = true;
    _checkmateMoves = _board.getCheckmateMoves(search_depth);
  }

  // If a checkmate move has been found, do nothing
  if (!_checkmateMoves.empty()) {
    return;
  }

  // If already evaluated, do nothing
  if (_evaluator.isEvaluated()) {
    return;
  }

  // Execute evaluation and create the list of candidate moves to evaluate next
  _evaluator.evaluate(&_board);

  for (Policy policy : _evaluator.getPolicies()) {
    _childPolicies.push_back(policy);
  }
}

/**
 * Evaluate the state of this node and return the evaluation result.
 * @param equally If true, equalize the number of searches
 * @param width Search width (if 0, adjust automatically)
 * @param algorithm Search algorithm
 * @param temperature Temperature parameter for search
 * @param noise Strength of Gumbel noise for search
 * @return Evaluation result
 */
NodeResult Node::_evaluateNode(
    bool equally, int32_t width, int32_t algorithm, float temperature, float noise) {
  // If entering king declaration is possible,
  // return the evaluation result as a win for the side to move
  if (_board.isNyugyoku(_board.getColor())) {
    return NodeResult(nullptr, _board.getColor(), 1);
  }

  // If a checkmate move has been found, return the evaluation result as a win for the side to move
  if (!_checkmateMoves.empty()) {
    return NodeResult(nullptr, _board.getColor(), 1);
  }

  // If this is the first visit, return the evaluation result of this node
  if (_visits == 1) {
    return NodeResult(nullptr, _evaluator.getValue(), 1);
  }

  // If there are no candidate moves, return the evaluation value of this node
  if (_childPolicies.empty()) {
    return NodeResult(nullptr, _evaluator.getValue(), 1);
  }

  // Get the candidate move to add to the evaluation from the queue
  int32_t children_size = _children.size() + _waitingSet.size();

  if (children_size < _childPolicies.size() && (width < 1 || children_size < width)) {
    int32_t max_index = 0;
    int max_priority_type = 0;
    float max_priority = 0.0f;

    // Calculate the temperature parameter
    float win_chance = getValue() * _board.getColor() * 0.5 + 0.5;
    float temperature_power = win_chance + (1.0 / temperature) * (1 - win_chance);

    // Create the Gumbel noise generator object
    // If the number of child nodes is 4 or less, do not add noise
    float noise_scale = (children_size <= 4) ? 0.0f : noise;
    std::extreme_value_distribution<float> noise_dist(0.0f, noise_scale);

    // Find the candidate move with the highest priority
    for (int i = 0; i < _childPolicies.size(); i++) {
      Policy& policy = _childPolicies[i];
      float probability = policy.policy;
      float probability_org = probability;

      // Reflect the temperature parameter
      probability = std::pow(probability, temperature_power);

      // Add Gumbel noise
      // The target for adding noise is the logit, so multiply the probability by e^noise
      probability *= std::exp(noise_dist(random_engine));

      // Calculate the priority
      int32_t priority_type = 1;
      float priority = probability / (policy.visits + 1);

      // If the setting is to equalize the number of searches,
      // lower the priority of already registered candidate moves
      if (equally) {
        int32_t policy_index = policy.move.getValue();

        if (_children.find(policy_index) != _children.end() ||
            _waitingSet.find(policy_index) != _waitingSet.end()) {
          priority_type = 0;
        }
      }

      // Keep the candidate move with the highest priority
      if (priority_type > max_priority_type ||
          (priority_type == max_priority_type && priority > max_priority)) {
        max_index = i;
        max_priority_type = priority_type;
        max_priority = priority;
      }
    }

    // If the candidate move to add to the evaluation is not registered, add it to the waiting list
    Policy& max_policy = _childPolicies[max_index];
    int32_t max_policy_index = max_policy.move.getValue();

    if (_children.find(max_policy_index) == _children.end() &&
        _waitingSet.find(max_policy_index) == _waitingSet.end()) {
      _waitingQueue.push(max_policy);
      _waitingSet.insert(max_policy_index);
    }

    // Increase the number of visits
    _childPolicies[max_index].visits += 1;
  }

  // If the search width is not specified
  // or the number of child nodes has not reached the specified search width,
  // and there are candidate moves in the waiting list,
  // create a new child node and return it as the next search target
  if (_waitingQueue.size() > 0 && (width <= 0 || _children.size() < width)) {
    // Get the first candidate move in the waiting list
    Policy policy = _waitingQueue.front();
    int32_t policy_index = policy.move.getValue();

    _waitingQueue.pop();
    _waitingSet.erase(policy_index);

    // If the candidate move is not registered,
    // create a new child node and return it as the next search target
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

  // Create the list of child nodes to be searched
  std::vector<std::pair<Node*, float>> children;

  for (std::pair<int32_t, Node*> child : _children) {
    children.push_back(std::make_pair(
        child.second, child.second->getValueLCB() * getColor()));
  }

  // If the search width is specified, limit the number of child nodes to be searched
  if (width > 0 && children.size() > width) {
    std::sort(children.begin(), children.end(), [](auto a, auto b) {
      return a.second > b.second;
    });

    children.resize(width);
  }

  // Return the node with the highest priority as the next search target
  Node* max_node = children[0].first;
  float max_priority = -1.0;

  for (std::pair<Node*, float> child : children) {
    float priority;

    // Calculate the priority
    if (equally) {
      float visits = child.first->getVisits();
      float value = child.first->getValue() * getColor();
      priority = 1.0 / (visits + 1 - value * 0.5);
    } else if (algorithm == SEARCH_UCB) {
      priority = child.first->getPriorityByUCB(_visits);
    } else {
      priority = child.first->getPriorityByPUCB(_visits);
    }

    // If there is a node where deep checkmate search has not been executed,
    // prioritize searching that node
    if (!child.first->_checkmateMoveDeepSearched &&
        child.first->_checkmateMoves.empty()) {
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
 * Initialize the evaluation information of the node.
 */
void Node::_reset() {
  _evaluator.clear();
  _children.clear();
  _childPolicies.clear();
  _waitingQueue = std::queue<Policy>();
  _waitingSet.clear();
  _checkmateMoves.clear();
  _checkmateMoveShallowSearched = false;
  _checkmateMoveDeepSearched = false;
  _visits = 0;
  _playouts = 0;
  _value = 0.0f;
  _count = 0;
  _minimax = 0.0f;
}

/**
 * Set the value as a continuation node of the specified node.
 * @param prevNode Previous node
 * @param move Move information
 * @param policy Predicted move probability
 */
void Node::_setAsNextNode(Node* prevNode, const Move& move, float policy) {
  _board.copyFrom(&prevNode->_board);
  _board.play(move);

  _move = move;
  _policy = policy;
  _reset();
}

}  // namespace deepshogi

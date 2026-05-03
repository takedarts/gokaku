#pragma once

#include <cstdint>
#include <queue>
#include <shared_mutex>

#include "Board.h"
#include "Config.h"
#include "Evaluation.h"
#include "Evaluator.h"
#include "Move.h"
#include "NodeParameter.h"
#include "NodeResult.h"
#include "PnSearchEngine.h"
#include "Policy.h"

namespace deepshogi {
class NodeManager;

/**
 * Search node class.
 */
class Node {
 public:
  /**
   * Create a search node object.
   * @param manager Node manager object
   * @param evaluator Object that performs board evaluation
   * @param parameter Node creation parameters
   */
  Node(NodeManager* manager, Evaluator* evaluator, const NodeParameter& parameter);

  /**
   * Set as the initial board node specified in SFEN format.
   * @param sfen Board in SFEN format
   */
  void initialize(const std::string sfen);

  /**
   * Evaluate the search node and get the next node object to evaluate.
   * If the next node object to evaluate does not exist, return nullptr.
   * @param equally If true, equalize the number of searches
   * @param width Search width (if 0, adjust automatically)
   * @param algorithm Search algorithm
   * @param pnSearchEngine Mate search engine object (nullptr if not searching for mate)
   * @param checkSearchDepth Search depth for checkmate moves
   * @param temperature Temperature parameter for search
   * @param noise Strength of Gumbel noise for search
   * @return Evaluation result
   */
  NodeResult evaluate(
      bool equally, int32_t width, int32_t algorithm,
      PnSearchEngine* pnSearchEngine, int32_t checkSearchDepth,
      float temperature, float noise);

  /**
   * Update the evaluation value of the search node.
   * @param value Evaluation value
   * @param minimax Minimax value
   */
  void updateValue(float value, float minimax);

  /**
   * Cancel the evaluation value of the search node.
   * @param value Evaluation value
   */
  void cancelValue(float value);

  /**
   * Get the candidate move with the highest PolicyNetwork evaluation value.
   * @return Candidate move
   */
  Move getPolicyMove();

  /**
   * Get the move.
   * @return Move
   */
  Move getMove();

  /**
   * Get the next side to move.
   * @return Side to move
   */
  int32_t getColor();

  /**
   * Get the predicted move probability of this node.
   * @return Predicted move probability
   */
  float getPolicy();

  /**
   * Get the list of child nodes.
   * @return List of node objects
   */
  std::vector<Node*> getChildren();

  /**
   * Get the node object when the specified move is made.
   * If the node object does not exist, return a newly created object.
   * The created node object is not registered as a child node of this node object.
   * @param move Move
   * @return Pointer to node object
   */
  Node* getChild(const Move& move);

  /**
   * Delete the node object when the specified move is made.
   * @param move Move
   */
  void removeChild(const Move& move);

  /**
   * Get the checkmate moves of this node.
   * If no checkmate moves are found, return an empty array.
   * @return Checkmate moves
   */
  std::vector<Move> getCheckmateMoves();

  /**
   * Get the number of searches for this node.
   * @return Number of searches
   */
  int32_t getVisits();

  /**
   * Get the number of playouts.
   * @return Number of playouts
   */
  int32_t getPlayouts();

  /**
   * Set the number of playouts.
   * @param playouts Number of playouts
   */
  void setPlayouts(int32_t playouts);

  /**
   * Get the evaluation value of this node.
   * @return Evaluation value
   */
  float getValue();

  /**
   * Get the minimax evaluation value of this node.
   * @return Minimax evaluation value
   */
  float getMinimax();

  /**
   * Get the lower bound of the confidence interval for the evaluation value of this node.
   * @return Lower bound of confidence interval
   */
  float getValueLCB();

  /**
   * Get the priority of this node based on UCB.
   * @param totalVisits Total number of searches
   * @return Priority
   */
  float getPriorityByUCB(int32_t totalVisits);

  /**
   * Get the priority of this node based on PUCB.
   * @param totalVisits Total number of searches
   * @return Priority
   */
  float getPriorityByPUCB(int32_t totalVisits);

  /**
   * Get the predicted sequence of this node.
   * @return Predicted sequence
   */
  std::vector<Move> getVariations();

  /**
   * Copy the state of the board to the specified board object.
   * @param board Board object
   */
  void copyBoardTo(Board* board);

  /**
   * Check if the board of this node is a lesser board of the specified node.
   * @param other Node to compare
   * @param color Color to evaluate
   * @return True if the board of this node is a lesser board of the specified node
   */
  inline bool isLesserThan(const Node* other, int8_t color) {
    return _board.isLesserThan(other->_board, color);
  }

 private:
  /**
   * Mutex for evaluation synchronization.
   */
  std::shared_mutex _evalMutex;

  /**
   * Mutex for value synchronization.
   */
  std::shared_mutex _valueMutex;

  /**
   * Node manager object.
   */
  NodeManager* _manager;

  /**
   * Board to be evaluated in this node.
   */
  Board _board;

  /**
   * Move.
   */
  Move _move;

  /**
   * Predicted move probability.
   */
  float _policy;

  /**
   * Object to evaluate the board.
   */
  Evaluator* _evaluator;

  /**
   * Evaluation result.
   */
  Evaluation _evaluation;

  /**
   * True if evaluation results from the model are set.
   */
  bool _evaluated;

  /**
   * Constant multiplied to UCB upper confidence bound.
   */
  float _ucbConstant;

  /**
   * Initial value applied to PUCB upper confidence bound.
   */
  float _pucbConstantInit;

  /**
   * Base value applied to PUCB upper confidence bound.
   */
  float _pucbConstantBase;

  /**
   * List of child nodes.
   */
  std::unordered_map<int32_t, Node*> _children;

  /**
   * List of next move probabilities.
   */
  std::vector<Policy> _childPolicies;

  /**
   * List of candidate moves waiting to be registered as child nodes.
   */
  std::queue<Policy> _waitingQueue;

  /**
   * Set of candidate moves waiting to be registered as child nodes.
   */
  std::set<int32_t> _waitingSet;

  /**
   * Checkmate moves found in this node.
   */
  std::vector<Move> _checkmateMoves;

  /**
   * True if shallow (5-move) checkmate search has been executed.
   */
  bool _checkmateMoveShallowSearched;

  /**
   * True if deep (DfPn) checkmate search has been executed.
   */
  bool _checkmateMoveDeepSearched;

  /**
   * Number of searches.
   */
  int32_t _visits;

  /**
   * Number of playouts.
   */
  int32_t _playouts;

  /**
   * Predicted win rate.
   */
  float _value;

  /**
   * Number of times predicted win rate and predicted points have been added.
   */
  int32_t _count;

  /**
   * Minimax evaluation value.
   */
  float _minimax;

  /**
   * Execute board evaluation for this node.
   */
  void _evaluateBoard();

  /**
   * Evaluate the state of this node and return the evaluation result.
   * @param equally If true, equalize the number of searches
   * @param width Search width (if 0, adjust automatically)
   * @param algorithm Search algorithm
   * @param temperature Temperature parameter for search
   * @param noise Strength of Gumbel noise for search
   * @return Evaluation result
   */
  NodeResult _evaluateNode(
      bool equally, int32_t width, int32_t algorithm, float temperature, float noise);

  /**
   * Initialize the evaluation information of the node.
   */
  void _reset();

  /**
   * Set the value as a continuation node of the specified node.
   * @param prevNode Previous node
   * @param move Move
   * @param policy Predicted move probability
   */
  void _setAsNextNode(Node* prevNode, const Move& move, float policy);
};

}  // namespace deepshogi

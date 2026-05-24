#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <queue>
#include <set>
#include <shared_mutex>
#include <string>
#include <vector>

#include "Board.h"
#include "MctsManager.h"
#include "MctsParameter.h"
#include "MctsPolicy.h"
#include "MctsValue.h"
#include "Move.h"
#include "PnSearchEngine.h"

namespace deepshogi {

/**
 * A class for managing the state of MCTS search nodes.
 */
class MctsNode {
 public:
  /**
   * Creates an MCTS search node object.
   * @param manager Node management object
   */
  MctsNode(MctsManager* manager);

  /**
   * Sets this node as the initial board node specified in SFEN format.
   * @param sfen Board in SFEN format
   */
  void initialize(const std::string sfen);

  /**
   * Applies the specified inference result to the evaluation value and predicted move probability list of this node.
   * @param value Board evaluation value
   * @param policies List of predicted probabilities for the next move
   */
  void applyInferenceResult(
      float value, const std::vector<std::pair<Move, float>>& policies);

  /**
   * Updates the MCTS evaluation value of this node.
   * @param mctsValue MCTS evaluation value
   */
  void updateMctsValue(float mctsValue);

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
   * @return Next node object to evaluate
   */
  MctsNode* pickupNextNode(bool equally, int32_t width, float temperature, float noise);

  /**
   * Performs checkmate search.
   * @param engine Checkmate search engine
   * @param depth Search depth for checkmate search
   */
  void searchCheckmateMoves(PnSearchEngine* engine, int32_t depth);

  /**
   * Sets this node as the root node.
   * This function performs the following:
   * - Removes the parent node
   * - If evaluated and legal moves exist but no moves are registered in the policy:
   *   - Deletes all child nodes of this node
   *   - Resets the evaluation and statistics of this node to an unevaluated state
   */
  void setAsRootNode();

  /**
   * Returns true if this node's board has been evaluated.
   * @return true if the board has been evaluated
   */
  bool isEvaluated();

  /**
   * Returns true if checkmate search has been performed on this node.
   * @return true if checkmate search has been performed
   */
  bool isCheckmateSearched();

  /**
   * Returns the board evaluation value of this node.
   * @return Board evaluation value
   */
  float getNodeValue();

  /**
   * Returns the list of predicted probabilities for the next move of this node.
   * @return List of predicted probabilities for the next move
   */
  std::vector<MctsPolicy> getPolicies();

  /**
   * Returns the parent node.
   * @return Parent node
   */
  MctsNode* getParent();

  /**
   * Returns the list of child nodes.
   * @return List of child nodes
   */
  std::vector<MctsNode*> getChildren();

  /**
   * Gets the node object for when the specified move is made.
   * If no node object exists, returns a newly created object.
   * The created node object is not registered as a child node of this node object.
   * @param move Move
   * @return Pointer to the node object
   */
  MctsNode* getChild(const Move& move);

  /**
   * Removes the node object for when the specified move is made from the child node list.
   * @param move Move
   */
  void removeChild(const Move& move);

  /**
   * Gets the visit count of this node.
   * @return Visit count
   */
  int32_t getVisits();

  /**
   * Gets the playout count.
   * @return Playout count
   */
  int32_t getPlayouts();

  /**
   * Gets the MCTS evaluation value of this node.
   * @return MCTS evaluation value
   */
  float getMctsValue();

  /**
   * Gets the lower confidence bound of the evaluation value of this node.
   * @return Lower confidence bound
   */
  float getMctsValueLCB();

  /**
   * Gets the priority of this node based on PUCB.
   * @param totalVisits Total visit count
   * @return Priority
   */
  float getPriorityByPUCB(int32_t totalVisits);

  /**
   * Gets the checkmate move sequence of this node.
   * Returns an empty array if no checkmate sequence has been found.
   * @return Checkmate move sequence
   */
  std::vector<Move> getCheckmateMoves();

  /**
   * Gets the expected line of play of this node.
   * @return Expected line of play
   */
  std::vector<Move> getVariations();

  /**
   * Gets the candidate move with the highest PolicyNetwork evaluation value.
   * @return Candidate move
   */
  Move getPolicyMove();

  /**
   * Returns the board object of this node.
   * @return Board object of this node
   */
  inline const Board& getBoard() const {
    return _board;
  }

  /**
   * Returns the immediately preceding move.
   * Returns an invalid move object if this node is the initial board node.
   * @return Immediately preceding move
   */
  inline Move getMove() const {
    return _move;
  }

  /**
   * Returns the predicted move probability of the immediately preceding move of this node.
   * @return Predicted move probability
   */
  inline float getProbability() const {
    return _probability;
  }

 private:
  /**
   * Mutex for synchronization.
   */
  std::shared_mutex _mutex;

  /**
   * Condition variable for waiting for evaluation completion.
   */
  std::condition_variable_any _condition;

  /**
   * Node management object.
   */
  MctsManager* _manager;

  /**
   * Board to evaluate at this node.
   */
  Board _board;

  /**
   * Move.
   */
  Move _move;

  /**
   * Predicted move probability.
   */
  float _probability;

  /**
   * true if this node is the first child of its parent node.
   */
  bool _firstChild;

  /**
   * true if evaluation of this node is in progress.
   */
  bool _evaluating;

  /**
   * true if this node has been evaluated.
   */
  bool _evaluated;

  /**
   * Board evaluation value of this node.
   */
  float _nodeValue;

  /**
   * List of predicted probabilities for the next move of this node.
   * This list is updated when the board evaluation value of this node is updated.
   * However, when this node is in a terminal state, this list is empty.
   * This node is in a terminal state under any of the following conditions:
   * - No legal moves exist (loss)
   * - An entering-king declaration is possible (win)
   * - The maximum move count has been reached (draw)
   * - A checkmate move sequence has been found (win)
   */
  std::vector<MctsPolicy> _policies;

  /**
   * Parent node.
   */
  MctsNode* _parent;

  /**
   * List of child nodes.
   */
  std::map<int32_t, MctsNode*> _children;

  /**
   * Visit count.
   */
  int32_t _visits;

  /**
   * Playout count.
   */
  std::atomic<int32_t> _playouts;

  /**
   * MCTS evaluation value.
   */
  MctsValue _mctsValue;

  /**
   * Checkmate move sequence.
   */
  std::vector<Move> _checkmateMoves;

  /**
   * true if deep checkmate search has been performed.
   */
  bool _checkmateMoveSearched;

  /**
   * List of candidate moves waiting to be registered as child nodes.
   */
  std::queue<MctsPolicy> _waitingPolicies;

  /**
   * Set of candidate moves waiting to be registered as child nodes.
   */
  std::set<int32_t> _waitingMoves;

  /**
   * Initializes the state of this node except for the board object.
   */
  void _resetNode();

  /**
   * Gets the next node object to evaluate.
   * This function assumes that this node has already been evaluated.
   * @param equally true to equalize the search visit count
   * @param width Search width (0 means automatic adjustment)
   * @param temperature Temperature parameter for search
   * @param noise Strength of Gumbel noise for search
   * @return Next node object to evaluate
   */
  MctsNode* _pickupNextNode(bool equally, int32_t width, float temperature, float noise);
};

}  // namespace deepshogi

#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

#include "Board.h"
#include "BoardHash.h"
#include "MctsManager.h"
#include "MctsParameter.h"
#include "MctsPolicy.h"
#include "MctsValue.h"
#include "Move.h"
#include "PnSearchEngine.h"

namespace deepshogi {

/**
 * Manage the state of an MCTS search node.
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
   * Apply inference results to this node's evaluation and move probabilities.
   * @param value Position evaluation.
   * @param remainingTurns Predicted moves remaining until the game ends.
   * @param policies Predicted probabilities of the next moves.
   */
  void applyInferenceResult(
      float value, float remainingTurns,
      const std::vector<std::pair<Move, float>>& policies);

  /**
   * Update this node's MCTS evaluation.
   * @param mctsValue MCTS evaluation.
   */
  void updateMctsValue(float mctsValue);

  /**
   * Gets the next node object to evaluate.
   * Returns nullptr if no next node to evaluate exists.
   * Conditions under which no next node exists:
   * - The board has not been evaluated
   * - No legal moves exist
   * - A checkmate move sequence has been found by checkmate search
   * - This is not the root node, and an entering-king declaration is possible or the draw move
   * count has been reached
   * Returns nullptr if search is canceled.
   * @param equally true to equalize the search visit count
   * @param width Search width (0 means automatic adjustment)
   * @param temperature Temperature parameter for search
   * @param noise Strength of Gumbel noise for search
   * @param isCanceled Function to check if the search is canceled
   * @return Next node object to evaluate
   */
  MctsNode* pickupNextNode(
      bool equally, int32_t width, float temperature, float noise,
      std::function<bool()> isCanceled);

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
   * Carry the position history forward when changing the root.
   * @param oldRootNode Previous root node.
   */
  void copyAppearedBoardHashes(const MctsNode* oldRootNode);

  /**
   * Return true if this node's position has been evaluated.
   * @return True if the position has been evaluated.
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
   * Return the predicted moves remaining from this node until the game ends.
   * @return Predicted moves remaining until the game ends.
   */
  float getRemainingTurns();

  /**
   * Return the predicted probabilities of this node's next moves.
   * @return Predicted next-move probabilities.
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
   * Return the largest visit count among candidate moves.
   * @return Largest child visit count.
   */
  int32_t getPvVisits();

  /**
   * Return this node's MCTS evaluation.
   * @return MCTS evaluation.
   */
  float getMctsValue();

  /**
   * Gets the lower confidence bound of the evaluation value of this node.
   * @return Lower confidence bound
   */
  float getMctsValueLCB();

  /**
   * Return this node's PUCB priority.
   * @param totalVisits Total visit count.
   * @param childrenSize Number of children of the parent node.
   * @return Whether visits are below the minimum, paired with the PUCB priority.
   */
  std::pair<bool, float> getPriorityByPUCB(int32_t totalVisits, int32_t childrenSize);

  /**
   * Return this node's checkmating move sequence.
   * Return an empty vector if no checkmating sequence is known.
   * @return Checkmating move sequence.
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
   * True while this node is being evaluated.
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
   * Predicted moves remaining from this node until the game ends.
   */
  float _remainingTurns;

  /**
   * Predicted probabilities of this node's next moves.
   * Update this list when the position evaluation changes.
   * The list is empty for terminal nodes.
   * A node is terminal under any of the following conditions.
   * - No legal moves exist (loss).
   * - An entering-king victory can be declared (win).
   * - The maximum move count is reached (draw).
   * - A checkmating move sequence has been found (win).
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
  std::atomic<int32_t> _visits;

  /**
   * Largest child visit count.
   */
  std::atomic<int32_t> _pvVisits;

  /**
   * MCTS evaluation.
   */
  MctsValue _mctsValue;

  /**
   * Number of times this node has been selected in MCTS.
   * This variable is updated by the parent node when this node is selected by the parent.
   */
  std::atomic<int32_t> _mctsSelects;

  /**
   * Number of times exploration has been executed at this node in MCTS.
   * This variable is updated by this node when expanding the search tree.
   */
  std::atomic<int32_t> _mctsProceeds;

  /**
   * Checkmate move sequence.
   */
  std::vector<Move> _checkmateMoves;

  /**
   * true if deep checkmate search has been performed.
   */
  bool _checkmateMoveSearched;

  /**
   * Position hashes encountered before reaching the root.
   */
  std::set<BoardHash> _appearedBoardHashes;

  /**
   * Candidate moves waiting to be registered as children.
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
   * Update visit counts for this node and its parent.
   */
  void _incrementVisits();

  /**
   * Get the next node to evaluate.
   * This method requires the current node to have been evaluated.
   * @param equally True to distribute visits equally.
   * @param width Search width; zero adjusts it automatically.
   * @param temperature Search temperature.
   * @param noise Strength of Gumbel noise during search.
   * @return Next node to evaluate.
   */
  MctsNode* _pickupNextNode(bool equally, int32_t width, float temperature, float noise);

  /**
   * Return true if this position occurs in the history or current search path.
   * @return True if the position has appeared before.
   */
  bool _isSennichite() const;
};

}  // namespace deepshogi

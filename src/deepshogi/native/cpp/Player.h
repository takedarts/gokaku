#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "Board.h"
#include "Candidate.h"
#include "InferenceProcessor.h"
#include "MctsManager.h"
#include "MctsNode.h"
#include "PnSearchManager.h"
#include "ThreadPool.h"

namespace deepshogi {

/**
 * Class that selects the next move as a player.
 */
class Player {
 public:
  /**
   * Create the player object.
   * @param processor Inference processor.
   * @param threads Number of threads.
   * @param searchMaxVisits Maximum node visit count.
   * @param nyugyokuScoreBlack Points required for Black's entering-king declaration.
   * @param nyugyokuScoreWhite Points required for White's entering-king declaration.
   * @param drawTurn Move count at which the game is drawn.
   * @param sennichitePenalty Penalty assigned to repetition evaluations.
   * @param checkSearchDepth Checkmate search depth.
   * @param checkSearchNode Checkmate search node capacity.
   * @param checkNodeDepth Maximum node depth at which to search for checkmate.
   * @param pucbConstantInit Initial PUCB exploration coefficient.
   * @param pucbConstantBase Base controlling the PUCB exploration coefficient.
   * @param pucbMinVisitsRate Minimum visit ratio for prioritizing PUCB children.
   */
  Player(
      InferenceProcessor* processor, int32_t threads, int32_t searchMaxVisits,
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
      float sennichitePenalty, int32_t checkSearchDepth, int32_t checkSearchNode,
      int32_t checkNodeDepth, float pucbConstantInit, float pucbConstantBase,
      float pucbMinVisitsRate);

  /**
   * Destroy the player object.
   */
  virtual ~Player();

  /**
   * Initializes the state of the player object.
   * @param sfen SFEN of the initial position
   */
  void initialize(const std::string& sfen);

  /**
   * Gets the next turn color.
   * @return Turn color
   */
  int32_t getColor();

  /**
   * Moves a piece according to the specified move.
   * @param move Information about the piece to move
   */
  void play(const Move& move);

  /**
   * Starts board evaluation.
   * Search processing is executed on a separate thread.
   * @param equally true to equalize search visit count, false to use UCB or PUCB
   * @param candidateWidth Search width for candidate moves (0 means automatic adjustment)
   * @param temperature Temperature parameter for search
   * @param noise Strength of Gumbel noise for search
   */
  void startEvaluation(
      bool equally, int32_t candidateWidth, float temperature, float noise);

  /**
   * Wait for the search to finish.
   * @param visits Target search visit count.
   * @param timelimit Maximum wait time in seconds.
   * @param stop True to stop the search.
   */
  void waitEvaluation(int32_t visits, float timelimit, bool stop);

  /**
   * Get the candidate moves.
   * @return Candidate move list.
   */
  std::vector<Candidate> getCandidates();

  /**
   * Gets the visit count of the root node.
   * @return Visit count of the root node
   */
  int32_t getVisits();

  /**
   * Copies the board state to the specified board object.
   * @param board Board object
   */
  void copyBoardTo(Board* board);

  /**
   * Gets a string representing the state of the player object.
   * @return String representing the state of the player object
   */
  std::string toString();

  /**
   * Writes the state of the player object to an output stream.
   * @param os Output stream
   * @param player Player object
   * @return Output stream
   */
  friend std::ostream& operator<<(std::ostream& os, Player& player) {
    os << player.toString();
    return os;
  }

 private:
  /**
   * Synchronization object.
   */
  std::mutex _mutex;

  /**
   * Condition variable for triggering search.
   */
  std::condition_variable _searchCondition;

  /**
   * Condition variable for triggering node update processing.
   */
  std::condition_variable _updateCondition;

  /**
   * Condition variable for waiting for termination.
   */
  std::condition_variable _stopCondition;

  /**
   * Condition variable for waiting until search count and playout count are satisfied.
   */
  std::condition_variable _waitCondition;

  /**
   * Object that performs inference.
   */
  InferenceProcessor* _processor;

  /**
   * Object that manages the checkmate search engine.
   */
  PnSearchManager _pnsearch;

  /**
   * Thread management object.
   */
  ThreadPool _threadPool;

  /**
   * Thread that manages search state.
   */
  std::thread _searchThread;

  /**
   * Thread that updates node state.
   */
  std::thread _updateThread;

  /**
   * Object that manages search nodes.
   */
  MctsManager _manager;

  /**
   * Root node.
   */
  MctsNode* _root;

  /**
   * Maximum visit count for a node.
   */
  int32_t _searchMaxVisits;

  /**
   * Depth for long-sequence checkmate search.
   */
  int32_t _checkSearchDepth;

  /**
   * Maximum depth of nodes to perform checkmate search.
   */
  int32_t _checkNodeDepth;

  /**
   * true to equalize search visit count.
   */
  bool _searchEqually;

  /**
   * Search width for candidate moves.
   */
  int32_t _searchCandidateWidth;

  /**
   * Temperature parameter for search.
   */
  float _searchTemperature;

  /**
   * Strength of Gumbel noise for search.
   */
  float _searchNoise;

  /**
   * Number of running threads.
   */
  int32_t _runnings;

  /**
   * Number of nodes currently being updated.
   */
  int32_t _updatingNodes;

  /**
   * True while the search is paused.
   */
  bool _paused;

  /**
   * true if the search is stopped.
   */
  bool _stopped;

  /**
   * true if the search is terminated.
   */
  bool _terminated;

  /**
   * true if the search is canceled.
   */
  std::atomic<bool> _canceled;

  /**
   * List of node objects being evaluated.
   */
  std::queue<MctsNode*> _evaluatingNodes;

  /**
   * List of node objects to perform checkmate search.
   */
  std::queue<MctsNode*> _checkingNodes;

  /**
   * Executes search.
   */
  void _runSearch();

  /**
   * Return true if all search work is idle.
   * Call this method while holding the synchronization mutex.
   * @return True if all search work is idle.
   */
  bool _isSearchIdle() const;

  /**
   * Expand the search tree.
   */
  void _runExpand();

  /**
   * Executes checkmate search.
   * @param node Node object to perform checkmate search
   */
  void _runCheckmateSearch(MctsNode* node);

  /**
   * Updates node state.
   */
  void _runUpdate();
};

}  // namespace deepshogi

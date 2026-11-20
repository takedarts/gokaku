#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <utility>
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
  Player(
      InferenceProcessor* processor, int32_t threads, int32_t searchMaxVisits,
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, float sennichitePenalty,
      int32_t checkSearchDepth, int32_t checkSearchNode, int32_t checkNodeDepth,
      float pucbConstantInit, float pucbConstantBase, float pucbMinVisitsRate);

  /**
   * Destroy the player object.
   */
  virtual ~Player();

  /**
   * Initializes the state of the player object with the nyugyoku scores given at construction.
   * @param sfen SFEN of the initial position
   * @param drawTurn Total move count at which the game is drawn.
   */
  void initialize(const std::string& sfen, int32_t drawTurn);

  /**
   * Initializes the state of the player object with the specified nyugyoku scores.
   * @param sfen SFEN of the initial position
   * @param drawTurn Total move count at which the game is drawn.
   * @param nyugyokuScoreBlack Points Black needs for nyugyoku (from -128 to 127).
   * @param nyugyokuScoreWhite Points White needs for nyugyoku (from -128 to 127).
   */
  void initialize(const std::string& sfen, int32_t drawTurn,
                  int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite);

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
   * Request stopping and wake threads waiting for search results.
   */
  void stopEvaluation();

  /**
   * Get the candidate moves.
   * @return Candidate move list.
   */
  std::vector<Candidate> getCandidates();

  /**
   * Return the policy and search evaluation of the current position.
   * @param value Output for the current position's search evaluation from Black's perspective.
   * @param remainingTurns Output for the predicted moves remaining from the current position.
   * @return Moves and their predicted probabilities.
   */
  std::vector<std::pair<Move, float>> getPolicyMoves(float& value, float& remainingTurns);

  /**
   * Gets the visit count of the root node.
   * @return Visit count of the root node
   */
  int32_t getVisits();

  /**
   * Copies the board state to the specified board object.
   * @param board Destination for the current position
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
   * @param player Player whose state is written
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
   * Points Black needs for nyugyoku, specified at construction.
   */
  const int32_t _defaultNyugyokuScoreBlack;

  /**
   * Points White needs for nyugyoku, specified at construction.
   */
  const int32_t _defaultNyugyokuScoreWhite;

  /**
   * Object that manages search nodes.
   */
  MctsManager _manager;

  /**
   * Root node.
   */
  MctsNode* _root;

  /**
   * Maximum node visit count.
   */
  int32_t _searchMaxVisits;

  /**
   * Evaluation queue limit, validated for size overflow at construction.
   */
  int32_t _maxEvaluatingSize;

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
   * Number of nodes being updated.
   */
  int32_t _updatingNodes;

  /**
   * Inference requests awaiting callback completion; protected by _mutex.
   */
  std::size_t _pendingInferences;

  /**
   * True while the search is paused.
   */
  bool _paused;

  /**
   * true if the search is stopped.
   */
  bool _stopped;

  /**
   * True after an explicit external stop request.
   */
  bool _stopRequested;

  /**
   * Sequence number of normal or explicit stops; protected by _mutex.
   */
  uint64_t _stopSequence;

  /**
   * True if the search has terminated.
   */
  bool _terminated;

  /**
   * true if the search is canceled.
   */
  std::atomic<bool> _canceled;

  /**
   * True to re-register unstarted checkmate searches on resume (protected by _mutex).
   */
  bool _checkmateRescan;

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
   * Abort running searches and defer unstarted PN searches to re-registration on the next resume.
   * Call this function while holding _mutex.
   */
  void _cancelSearch();

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
   * Submit an inference request while tracking callback completion.
   * @param node Node to evaluate.
   */
  void _submitInference(MctsNode* node);

  /**
   * Record inference completion or submission failure and notify waiting threads.
   */
  void _finishInference();

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

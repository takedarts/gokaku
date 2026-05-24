#pragma once

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
  Player(
      InferenceProcessor* processor, int32_t threads, int32_t searchMaxVisits,
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
      int32_t checkSearchDepth, int32_t checkSearchNode, int32_t checkNodeDepth,
      float pucbConstantInit, float pucbConstantBase);

  /**
   * Destroys the player object.
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
   * Waits until the search completes.
   * @param visits Search visit count
   * @param playouts Search playout count
   * @param timelimit Time to wait (seconds)
   * @param stop true to stop the search
   */
  void waitEvaluation(int32_t visits, int32_t playouts, float timelimit, bool stop);

  /**
   * Gets the list of candidate moves.
   * @return List of candidate moves
   */
  std::vector<Candidate> getCandidates();

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
   * true if the search is paused.
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
   * Expands the search tree.
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

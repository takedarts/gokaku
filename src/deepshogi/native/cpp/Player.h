#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <vector>

#include "Board.h"
#include "Candidate.h"
#include "Config.h"
#include "Node.h"
#include "NodeManager.h"
#include "Processor.h"
#include "ThreadPool.h"

namespace deepshogi {

/**
 * Class representing a player who progresses the game.
 */
class Player {
 public:
  /**
   * Create a player object.
   * @param processor Object that performs inference
   * @param threads Number of threads
   * @param nyugyokuScoreBlack Points required for black's entering king declaration
   * @param nyugyokuScoreWhite Points required for white's entering king declaration
   * @param drawSteps Number of moves until a draw
   * @param checkSearchDepth Depth for mate search
   * @param checkSearchNode Number of nodes for mate search
   * @param evalLeafOnly True if only leaf nodes are evaluated
   * @param maxVisits Maximum number of visits for search
   */
  Player(
      Processor* processor, int32_t threads,
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps,
      int32_t checkSearchDepth, int32_t checkSearchNode,
      bool evalLeafOnly, int32_t maxVisits);

  /**
   * Destroy the player object.
   */
  virtual ~Player();

  /**
   * Initialize the state of the player object.
   * @param sfen Initial position SFEN
   */
  void initialize(const std::string& sfen);

  /**
   * Get the next turn.
   * @return Turn
   */
  int32_t getColor();

  /**
   * Move a piece.
   * @param move Information of the piece to move
   */
  void play(const Move& move);

  /**
   * Start board evaluation.
   * The search process is executed in a separate thread.
   * @param equally True to make the number of searches equal, false to use UCB1 or PUCB
   * @param useUcb1 True to use UCB1 as the search criterion, false to use PUCB
   * @param candidateWidth Search width for candidate moves (if 0, the width is automatically adjusted)
   * @param checkNodeDepth Maximum depth of nodes for mate search
   * @param temperature Temperature parameter for search
   * @param noise Strength of Gumbel noise for search
   */
  void startEvaluation(
      bool equally, bool useUcb1, int32_t candidateWidth, int32_t checkNodeDepth,
      float temperature, float noise);

  /**
   * Wait until the search is finished.
   * @param visits Number of search visits
   * @param playouts Number of search playouts
   * @param timelimit Time to wait (seconds)
   * @param stop True to stop the search
   */
  void waitEvaluation(int32_t visits, int32_t playouts, float timelimit, bool stop);

  /**
   * Get the list of candidate moves.
   * @return List of candidate moves
   */
  std::vector<Candidate> getCandidates();

  /**
   * Copy the board state to the specified board object.
   * @param board Board object
   */
  void copyBoardTo(Board* board);

 private:
  /**
   * Synchronization object.
   */
  std::mutex _mutex;

  /**
   * Condition variable for synchronization.
   */
  std::condition_variable _condition;

  /**
   * Object that manages search nodes.
   */
  NodeManager _nodeManager;

  /**
   * Thread management object.
   */
  ThreadPool _threadPool;

  /**
   * Thread that executes the search.
   */
  std::unique_ptr<std::thread> _thread;

  /**
   * Root node.
   */
  Node* _root;

  /**
   * True if only leaf nodes are evaluated.
   */
  bool _evalLeafOnly;

  /**
   * Maximum number of visits for search.
   */
  int32_t _maxVisits;

  /**
   * Number of search visits.
   */
  int32_t _searchVisits;

  /**
   * Number of search playouts.
   */
  int32_t _searchPlayouts;

  /**
   * True if the number of searches is made equal.
   */
  bool _searchEqually;

  /**
   * True if UCB1 is used as the search criterion.
   */
  bool _searchUseUcb1;

  /**
   * Search width for candidate moves.
   */
  int32_t _searchCandidateWidth;

  /**
   * Maximum depth of nodes for mate search.
   */
  int32_t _searchCheckNodeDepth;

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
   * True if the search is paused.
   */
  bool _paused;

  /**
   * True if the search is stopped.
   */
  bool _stopped;

  /**
   * True if the search is terminated.
   */
  bool _terminated;

  /**
   * Start the search process.
   */
  void _run();

  /**
   * Execute the search.
   * @return Number of playouts
   */
  int32_t _evaluate();

  /**
   * Release node objects other than the root node.
   * @param node Node object to release
   */
  void _releaseNode(Node* node);
};

}  // namespace deepshogi

#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

#include "Board.h"
#include "BoardHash.h"
#include "Move.h"
#include "PnSearchCache.h"
#include "PnSearchNode.h"

namespace deepshogi {

/**
 * Search for checkmate using the DF-PN algorithm.
 */
class PnSearchEngine {
 private:
  // Allow only the manager class to prepare engines when lending and change state on return
  friend class PnSearchManager;

  /**
   * State representing preparation, execution, and completion of a search and abort requests.
   */
  enum class SearchState {
    IDLE,
    READY,
    RUNNING,
    ABORT_REQUESTED,
    COMPLETED,
    ABORTED
  };

  /**
   * Allow PnSearchNode::expand() to obtain search nodes.
   * @param engine Search engine.
   * @param board Working search position.
   * @return False when node capacity is exhausted or an abort is requested.
   */
  friend bool PnSearchNode::expand(
      PnSearchEngine* engine, Board* board,
      const std::function<bool(const BoardHash&)>& isRepeated);

  /**
   * Store the requested depth and continuation conditions for a search path.
   */
  struct SearchFrame {
    /** Node searched by this frame. */
    PnSearchNode* node;

    /** Remaining search depth requested by this frame. */
    int32_t depth;

    /** PN threshold for this frame. */
    double pnLimit;

    /** DN threshold for this frame. */
    double dnLimit;

    /** Search progress counter on entry to this frame. */
    uint64_t enteredProgress;

    /** Whether to raise effective thresholds until the search makes progress. */
    bool forceProgress = false;

    /** Last child that returned without progress. */
    PnSearchNode* retryChild = nullptr;

    /** Number of child returns without progress. */
    size_t emptyReturns = 0;
  };

  /**
   * Store the selected child and aggregates used to calculate thresholds.
   */
  struct SearchChoice {
    /** Move leading to the selected child. */
    Move move;

    /** Selected child, or nullptr if none is eligible. */
    PnSearchNode* node = nullptr;

    /** Search priority of the runner-up child. */
    double secondPriority = std::numeric_limits<double>::infinity();

    /** Unsaturated PN aggregate from the children. */
    uint64_t pn = 0;

    /** Unsaturated DN aggregate from the children. */
    uint64_t dn = 0;

    /** PN value of the selected child. */
    int32_t childPn = 0;

    /** DN value of the selected child. */
    int32_t childDn = 0;

    /** PN value of the drop group containing the selected move. */
    int32_t groupPn = 0;
  };

  /**
   * Store the PN and DN thresholds for the current frame.
   */
  struct SearchLimits {
    /** PN threshold for the current frame. */
    double pn;

    /** DN threshold for the current frame. */
    double dn;
  };

 public:
  /**
   * Construct a DF-PN search engine.
   * @param nodeSize Maximum number of search nodes.
   */
  PnSearchEngine(int32_t nodeSize);

  /**
   * Deleted copy constructor.
   * @param engine Source object; copying is disabled.
   */
  PnSearchEngine(const PnSearchEngine& engine) = delete;

  /**
   * Destroy the DF-PN search engine.
   */
  virtual ~PnSearchEngine() = default;

  /**
   * Search for and return a checkmating move sequence.
   * Return an empty vector if no checkmate is found or the search is aborted.
   * Searches using the same engine must be called serially.
   * @param board Object holding the board position.
   * @param depth Initial depth, rounded to odd; may increase during search.
   * @param isPreviousPosition Function that identifies positions before the search started
   *     (empty for the conventional search).
   * @return Checkmating move sequence (use isSearchAborted() to detect an abort).
   */
  std::vector<Move> getCheckmateMoves(
      const Board* board, int32_t depth,
      const std::function<bool(const BoardHash&)>& isPreviousPosition = {});

  /**
   * Request an abort of a prepared or running search. Do not wait for it to finish.
   */
  void abortSearch();

  /**
   * Return whether the previous search was aborted. Check it after the search returns and
   * before the engine is returned or the next search starts.
   * @return True if the search ended because of an abort.
   */
  bool isSearchAborted() const;

  /**
   * Return the number of search nodes in use.
   * @return Number of search nodes in use.
   */
  inline int32_t getNodeCount() const {
    return _nodeCount;
  }

  /**
   * Return the number of nodes cloned to handle loops.
   * @return Number of search nodes cloned to handle loops.
   */
  inline int32_t getCloneCount() const {
    return _cloneCount;
  }

 private:
  /** State synchronizing abort requests from other threads with search completion. */
  std::atomic<SearchState> _searchState;

  /**
   * Invalidate the previous result before lending and accept aborts before the start.
   */
  void _prepareSearch();

  /**
   * Return whether an abort has been requested.
   * @return True if an abort request has been received.
   */
  bool _isAbortRequested() const;

  /**
   * Resolve a race between completion and abort and finalize the search result.
   * @return True if completion was finalized before the abort.
   */
  bool _finishSearch();

  /**
   * Initialize the search state and run the DF-PN search synchronously.
   * @param board Input position.
   * @param depth Initial search depth.
   * @param isPreviousPosition Function that identifies positions before the search started
   *     (empty for the conventional search).
   * @return Checkmating sequence, or an empty vector if not found, out of capacity, or aborted.
   */
  std::vector<Move> _searchCheckmateMoves(
      const Board* board, int32_t depth,
      const std::function<bool(const BoardHash&)>& isPreviousPosition);

  /**
   * Follow all replies of the proof and check that checkmate holds before any repetition on
   * that path.
   * @param node Node that proved checkmate.
   * @param board Corresponding position (the same position on return).
   * @param path Positions that have appeared so far in the proof.
   * @param isPreviousPosition Function that identifies positions before the search started.
   * @return True if checkmate holds before any repetition for all replies.
   */
  bool _validateMateProof(
      PnSearchNode* node, Board& board, std::vector<BoardHash>& path,
      const std::function<bool(const BoardHash&)>& isPreviousPosition) const;

  /**
   * Whether to adjust PN thresholds using drop groups.
   */
  bool _useGroups;

  /**
   * Number of nodes cloned to handle loops.
   */
  int32_t _cloneCount;

  /**
   * Progress counter for node allocation, leaf expansion, and loop replacement.
   */
  uint64_t _progress;

  /**
   * Array of search nodes.
   */
  std::vector<PnSearchNode> _nodes;

  /**
   * Number of search nodes.
   */
  int32_t _nodeSize;

  /**
   * Number of nodes currently in use.
   */
  int32_t _nodeCount;

  /**
   * Search node cache keyed only by position.
   */
  PnSearchCache _cacheNodes;

  /**
   * Return effective frame thresholds from the current aggregates.
   * @param frame Search frame.
   * @param choice Latest aggregates.
   * @return Effective frame thresholds.
   */
  SearchLimits _getLimits(
      const SearchFrame& frame, const SearchChoice& choice) const;

  /**
   * Return the selected child's frame from parent thresholds and sibling values.
   * @param frame Parent frame.
   * @param choice Selection result.
   * @param limits Effective parent thresholds.
   * @return Child search frame.
   */
  SearchFrame _makeChildFrame(
      const SearchFrame& frame, const SearchChoice& choice,
      const SearchLimits& limits) const;

  /**
   * Return the preferred child, runner-up value, and unsaturated PN and DN.
   * @param node Node to aggregate.
   * @return Preferred child and aggregate search values.
   */
  SearchChoice _selectDfpn(const PnSearchNode* node) const;

  /**
   * Obtain a search node.
   * Obtain a regular node even when the remaining depth is zero.
   * Return nullptr when the node capacity is exhausted or an abort is requested.
   * @param board Position to search.
   * @param depth Remaining search depth in moves.
   * @param noCache True to skip both cache lookup and insertion.
   * @param isPreviousPosition Function that identifies positions before the search started
   *     (empty for the conventional search).
   * @return Pointer to the search node.
   */
  PnSearchNode* _getNode(
      const Board* board, int32_t depth, bool noCache,
      const std::function<bool(const BoardHash&)>& isPreviousPosition = {});

  /**
   * Replace the detected path's defensive node with the shared terminal and align the path.
   * @param board Detected position; replacement position on success.
   * @param parents Ancestor list.
   * @param history Move history.
   * @param pathHashes Ancestor positions.
   * @param loopIndex Index of the loop ancestor.
   * @param node Regular node where the loop was detected.
   * @param attacker True if the current position is attacking.
   * @return False on capacity exhaustion or an abort request, without changing connections.
   */
  bool _replaceLoop(
      Board* board, std::vector<PnSearchNode*>& parents,
      std::vector<MoveResult>& history, std::vector<BoardHash>& pathHashes,
      size_t loopIndex, PnSearchNode* node, bool attacker);

  /**
   * Clone nodes after the loop ancestor through the current node's parent outside the cache.
   * @param board Current position, preserved on return.
   * @param parents Reference to the ancestor array; updated over the cloned segment.
   * @param history Move history from the root.
   * @param loopIndex Index of the loop ancestor.
   * @return False on capacity exhaustion or an abort request, without changing connections.
   */
  bool _detachLoop(
      Board* board, std::vector<PnSearchNode*>& parents,
      const std::vector<MoveResult>& history, size_t loopIndex);
};

}  // namespace deepshogi

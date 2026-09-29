#pragma once

#include <cstdint>
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
  /**
   * Allow PnSearchNode::expand() to obtain search nodes.
   * @param engine PnSearchEngine* search engine.
   * @param board Board* working search position.
   * @return bool; false only when node capacity is exhausted.
   */
  friend bool PnSearchNode::expand(PnSearchEngine* engine, Board* board);

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
   */
  PnSearchEngine(const PnSearchEngine& engine) = delete;

  /**
   * Destroy the DF-PN search engine.
   */
  virtual ~PnSearchEngine();

  /**
   * Search for and return a checkmating move sequence.
   * Return an empty vector if no checkmate is found.
   * @param board Object holding the board position.
   * @param depth int32_t initial depth, rounded to odd; may increase during search.
   * @return Checkmating move sequence.
   */
  std::vector<Move> getCheckmateMoves(const Board* board, int32_t depth);

  /**
   * Return the number of search nodes in use.
   * @return int32_t number of search nodes in use.
   */
  inline int32_t getNodeCount() const {
    return _nodeCount;
  }

  /**
   * Return the number of nodes cloned to handle loops.
   * @return int32_t number of search nodes cloned to handle loops.
   */
  inline int32_t getCloneCount() const {
    return _cloneCount;
  }

 private:
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
   * @param frame const SearchFrame& search frame.
   * @param choice const SearchChoice& latest aggregates.
   * @return SearchLimits effective thresholds.
   */
  SearchLimits _getLimits(
      const SearchFrame& frame, const SearchChoice& choice) const;

  /**
   * Return the selected child's frame from parent thresholds and sibling values.
   * @param frame const SearchFrame& parent frame.
   * @param choice const SearchChoice& selection result.
   * @param limits const SearchLimits& effective parent thresholds.
   * @return SearchFrame child frame.
   */
  SearchFrame _makeChildFrame(
      const SearchFrame& frame, const SearchChoice& choice,
      const SearchLimits& limits) const;

  /**
   * Return the preferred child, runner-up value, and unsaturated PN and DN.
   * @param node PnSearchNode* node to aggregate.
   * @return SearchChoice selection result.
   */
  SearchChoice _selectDfpn(const PnSearchNode* node) const;

  /**
   * Obtain a search node.
   * Obtain a regular node even when the remaining depth is zero.
   * Return nullptr when the node capacity is exhausted.
   * @param board Position to search.
   * @param depth int32_t remaining search depth in moves.
   * @param noCache bool; true skips both cache lookup and insertion.
   * @return Pointer to the search node.
   */
  PnSearchNode* _getNode(
      const Board* board, int32_t depth, bool noCache);

  /**
   * Replace the detected path's defensive node with the shared terminal and align the path.
   * @param board Board* detected position; replacement position on success.
   * @param parents std::vector<PnSearchNode*>& ancestor list.
   * @param history std::vector<MoveResult>& move history.
   * @param pathHashes std::vector<BoardHash>& ancestor positions.
   * @param loopIndex size_t index of the loop ancestor.
   * @param node PnSearchNode* regular node where the loop was detected.
   * @param attacker bool indicating whether the current position is attacking.
   * @return bool; false on capacity exhaustion, without changing connections.
   */
  bool _replaceLoop(
      Board* board, std::vector<PnSearchNode*>& parents,
      std::vector<MoveResult>& history, std::vector<BoardHash>& pathHashes,
      size_t loopIndex, PnSearchNode* node, bool attacker);

  /**
   * Clone nodes after the loop ancestor through the current node's parent outside the cache.
   * @param board Board* current position, preserved on return.
   * @param parents Reference to the ancestor array; updated over the cloned segment.
   * @param history const std::vector<MoveResult>& move history from the root.
   * @param loopIndex size_t index of the loop ancestor.
   * @return bool; false on capacity exhaustion, without changing connections.
   */
  bool _detachLoop(
      Board* board, std::vector<PnSearchNode*>& parents,
      const std::vector<MoveResult>& history, size_t loopIndex);
};

}  // namespace deepshogi

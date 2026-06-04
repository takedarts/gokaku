#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "Board.h"
#include "BoardHash.h"
#include "Move.h"
#include "PnSearchNode.h"

namespace deepshogi {

/**
 * Engine class that searches for checkmate sequences using the PN search algorithm.
 */
class PnSearchEngine {
 public:
  /**
   * Constructs a PN search engine object.
   * @param nodeSize Maximum number of nodes for the search
   */
  PnSearchEngine(int32_t nodeSize);

  /**
   * Deleted copy constructor.
   */
  PnSearchEngine(const PnSearchEngine& engine) = delete;

  /**
   * Destroys the PN search engine object.
   */
  virtual ~PnSearchEngine() = default;

  /**
   * Searches for a checkmate sequence and returns the move sequence.
   * Returns an empty array if no checkmate sequence is found.
   * @param board Object holding the board state
   * @param depth Search depth
   * @return Move sequence for the checkmate
   */
  std::vector<Move> getCheckmateMoves(const Board* board, int32_t depth);

 private:
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
   * Cache of search nodes.
   */
  std::map<BoardHash, PnSearchNode*> _nodeCache;

  /**
   * Retrieves a new search node.
   * Returns the cached node if one with the same board state exists.
   * Returns nullptr if the maximum number of nodes has been reached.
   * @param board Board state to search
   * @param depth Current search depth
   * @return Pointer to the search node
   */
  PnSearchNode* _getNode(const Board* board, int32_t depth);

  // PnSearchNode::expand() calls PnSearchEngine::_getNode(), so
  // PnSearchNode::expand() is declared as a friend function.
  friend bool PnSearchNode::expand(PnSearchEngine* engine);
};

}  // namespace deepshogi

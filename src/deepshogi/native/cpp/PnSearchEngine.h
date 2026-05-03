#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "Board.h"
#include "BoardHash.h"
#include "Move.h"
#include "PnSearchNode.h"

namespace deepshogi {

/**
 * Engine class for searching checkmate sequences using the PN search algorithm.
 */
class PnSearchEngine {
 public:
  /**
   * Creates a PN search engine object.
   * @param nodes Maximum number of nodes for search
   */
  PnSearchEngine(int32_t nodes);

  /**
   * Deletes the copy constructor.
   */
  PnSearchEngine(const PnSearchEngine& engine) = delete;

  /**
   * Destroys the PN search engine object.
   */
  virtual ~PnSearchEngine() = default;

  /**
   * Searches for checkmate sequences and returns the move sequence.
   * Returns an empty array if no checkmate sequence is found.
   * @param board Board information
   * @param depth Depth to search
   * @return Move sequence for checkmate
   */
  std::vector<Move> getCheckmateMoves(const Board* board, int32_t depth);

 private:
  /**
   * Array of search nodes.
   */
  std::unique_ptr<PnSearchNode[]> _nodes;

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
   * If a node for the same board exists in the cache, it returns that node.
   * If the maximum number of nodes is reached, it returns nullptr.
   * @param board Board information for the search
   * @param depth Current search depth
   * @return Pointer to the search node
   */
  PnSearchNode* _getNode(const Board* board, int32_t depth);

  // The expand function of the PnSearchNode class calls the _getNode function of the PnSearchEngine class,
  // so the expand function of the PnSearchNode class is declared as a friend function.
  friend bool PnSearchNode::expand(PnSearchEngine* engine);
};

}  // namespace deepshogi

#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "Board.h"
#include "DfpnNode.h"
#include "DfpnNodeKey.h"
#include "Move.h"

namespace deepshogi {

/**
 * Engine class that searches for checkmate moves using the DFPN algorithm.
 */
class DfpnEngine {
 public:
  /**
   * Construct a DFPN engine instance.
   * @param nodes maximum number of search nodes
   */
  DfpnEngine(int32_t nodes);

  /**
   * Destroy the DFPN engine instance.
   */
  virtual ~DfpnEngine() = default;

  /**
   * Obtain a new search node.
   * Returns nullptr if all nodes are in use.
   * @param board board information to search
   * @param depth current search depth
   * @return pointer to the search node
   */
  DfpnNode* getNode(const Board* board, int32_t depth);

  /**
   * Search for checkmate moves and return the sequence of moves.
   * Returns an empty vector if no checkmate moves are found.
   * @param board object that holds board information
   * @param depth depth to search
   * @return sequence of moves representing the checkmate moves
   */
  std::vector<Move> getCheckmateMoves(const Board* board, int32_t depth);

 private:
  /**
   * Array of search nodes.
   */
  std::unique_ptr<DfpnNode[]> _nodes;

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
  std::map<DfpnNodeKey, DfpnNode*> _nodeCache;
};

}  // namespace deepshogi

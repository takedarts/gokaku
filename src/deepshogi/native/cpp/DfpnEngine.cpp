#include "DfpnEngine.h"

#include <set>

namespace deepshogi {

/**
 * Construct a DFPN engine instance.
 * @param nodes maximum number of search nodes
 */
DfpnEngine::DfpnEngine(int32_t nodes)
    : _nodes(std::make_unique<DfpnNode[]>(nodes)),
      _nodeSize(nodes),
      _nodeCount(0),
      _nodeCache() {
}

/**
 * Obtain a new search node.
 * Returns nullptr if all nodes are in use.
 * @param board board information to search
 * @param depth current search depth
 * @return pointer to the search node
 */
DfpnNode* DfpnEngine::getNode(const Board* board, int32_t depth) {
  if (_nodeCount >= _nodeSize) {
    return nullptr;
  }

  // Acquire a new node from the node pool
  DfpnNode* node = &_nodes[_nodeCount];

  _nodeCount += 1;
  node->initialize(board, depth);

  // Check the node cache
  DfpnNodeKey node_key(node);
  auto it = _nodeCache.find(node_key);

  if (it != _nodeCache.end()) {
    // If a node exists in the cache, use the cached node
    node = it->second;
    // Decrease the node count because the node obtained from the pool will not be used
    _nodeCount -= 1;
  } else {
    // If no node exists in the cache, register the new node in the cache
    _nodeCache[node_key] = node;
  }

  return node;
}

/**
 * Search for checkmate moves and return the sequence of moves.
 * Returns an empty vector if no checkmate moves are found.
 * @param board object that holds board information
 * @param depth depth to search
 * @return sequence of moves representing the checkmate moves
 */
std::vector<Move> DfpnEngine::getCheckmateMoves(const Board* board, int32_t depth) {
  // Search depth must be odd
  if (depth % 2 == 0) {
    depth -= 1;
  }

  depth = std::max(depth, 1);

  // Reset the node pool
  _nodeCount = 0;
  _nodeCache.clear();

  // Create the root node
  DfpnNode* root = getNode(board, 0);

  // Execute the search
  std::vector<DfpnNode*> parents;
  std::set<DfpnNode*> visited;

  while (true) {
    parents.clear();
    visited.clear();

    // Get the next node to explore
    DfpnNode* node = root;
    DfpnNode* next_node = node->getNextNode();

    while (next_node != nullptr) {
      parents.push_back(node);
      visited.insert(node);

      node = next_node;
      next_node = node->getNextNode();

      if (visited.find(node) != visited.end()) {
        break;
      }
    }

    // If a loop is detected, remove the child node from its parent
    if (visited.find(node) != visited.end()) {
      DfpnNode* parent_node = parents.back();
      parent_node->removeChildNode(node);

      for (auto it = parents.rbegin(); it != parents.rend(); it++) {
        (*it)->update(depth);
      }

      continue;
    }

    // If the next node is a mate node, end the search
    if (node->getPn() == 0 || node->getDn() == 0) {
      break;
    }

    // Expand the node (if expansion fails, end the search)
    if (!node->expand(this)) {
      break;
    }

    // Update PN/DN values
    node->update(depth);

    for (auto it = parents.rbegin(); it != parents.rend(); it++) {
      (*it)->update(depth);
    }
  }

  // Retrieve the checkmate move sequence
  DfpnNode* node = root;
  std::vector<Move> checkmate_moves;

  while (true) {
    Move checkmate_move = node->getCheckmateMove();

    if (checkmate_move.getSrcX() < 0) {
      break;
    }

    checkmate_moves.push_back(checkmate_move);
    node = node->getChildNode(checkmate_move);
  }

  return checkmate_moves;
}

}  // namespace deepshogi

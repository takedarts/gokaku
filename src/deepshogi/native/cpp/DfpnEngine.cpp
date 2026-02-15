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
  int32_t color = board->getColor();

  // Execute the search
  std::vector<DfpnNode*> parents;
  DfpnNode end_node;

  while (root->getPn() != 0 && root->getDn() != 0) {
    parents.clear();

    // Get the next node to explore
    DfpnNode* node = root;
    DfpnNode* next_node = node->getNextNode();
    bool loop_found = false;

    while (next_node != nullptr) {
      parents.push_back(node);

      node = next_node;
      next_node = node->getNextNode();

      // Check if a loop is detected
      for (DfpnNode* parent : parents) {
        if (parent == node) {
          loop_found = true;
          break;
        }
      }

      // If a loop is detected, end the search
      if (loop_found) {
        break;
      }
    }

    // If a loop is detected, replace the child node with an end node
    if (loop_found) {
      parents.back()->replaceChildNode(node, &end_node);

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
    std::pair<Move, DfpnNode*> checkmate_node_pair = node->getCheckmateNode();

    if (checkmate_node_pair.second == nullptr) {
      break;
    }

    checkmate_moves.push_back(checkmate_node_pair.first);
    node = checkmate_node_pair.second;
  }

  return checkmate_moves;
}

}  // namespace deepshogi

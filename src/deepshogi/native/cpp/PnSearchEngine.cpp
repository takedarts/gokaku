#include "PnSearchEngine.h"

#include <algorithm>
#include <set>

namespace deepshogi {

// Constant representing a terminal node
static const PnSearchNode END_NODE = PnSearchNode();

/**
 * Creates a PN search engine object.
 * @param nodes Maximum number of nodes for search
 */
PnSearchEngine::PnSearchEngine(int32_t nodes)
    : _nodes(std::make_unique<PnSearchNode[]>(nodes)),
      _nodeSize(nodes),
      _nodeCount(0),
      _nodeCache() {
}

/**
 * Searches for checkmate sequences and returns the move sequence.
 * Returns an empty array if no checkmate sequence is found.
 * @param board Board information
 * @param depth Depth to search
 * @return Move sequence of the checkmate
 */
std::vector<Move> PnSearchEngine::getCheckmateMoves(const Board* board, int32_t depth) {
  // Search depth must be an odd number
  depth = std::max((depth % 2 == 0) ? depth + 1 : depth, 1);

  // Reset node pool
  _nodeCount = 0;
  _nodeCache.clear();

  // Create root node
  PnSearchNode* root = _getNode(board, 0);

  // Execute search
  std::vector<PnSearchNode*> parents;

  // Continue searching until checkmate or non-checkmate is found
  // However, terminate if maximum search count is reached
  while (root->getPn() != 0 && root->getDn() != 0) {
    // Search from root node to leaf node
    PnSearchNode* node = root;
    PnSearchNode* next_node = node->getNextNode();

    parents.clear();

    // Continue until reaching a leaf node
    while (next_node != nullptr) {
      // Add current node to parent list to detect loops
      parents.push_back(node);

      // If the next search node is the same as or a subordinate node of any parent node,
      // replace the next search node with a non-checkmate node
      if (std::any_of(parents.begin(), parents.end(), [next_node](PnSearchNode* parent) {
            return next_node->isLesserThanOrEqual(parent);
          })) {
        node->replaceChildNode(next_node, const_cast<PnSearchNode*>(&END_NODE));
        next_node = const_cast<PnSearchNode*>(&END_NODE);
      }

      // Move to next node
      node = next_node;
      next_node = node->getNextNode();
    }

    // Expand node if next search node is not checkmate/non-checkmate
    if (node->getPn() != 0 && node->getDn() != 0) {
      // Expand the node
      // Node expansion failure means maximum search nodes reached,
      // so terminate search if expansion fails
      if (!node->expand(this)) {
        break;
      }

      // Update PN/DN values of leaf node
      node->update(depth);
    }

    // Update PN/DN values of parent nodes
    // Parent PN/DN values must be updated even if leaf node is checkmate/non-checkmate.
    // Reflect the result of the leaf node in the root node's PN/DN values
    //  by updating the parent nodes' PN/DN values.
    for (auto it = parents.rbegin(); it != parents.rend(); it++) {
      (*it)->update(depth);
    }
  }

  // Retrieve checkmate move sequence
  std::vector<Move> checkmate_moves;
  PnSearchNode* node = root;

  while (true) {
    auto [next_move, next_node] = node->getCheckmateNode();

    if (next_node == nullptr) {
      break;
    }

    checkmate_moves.push_back(next_move);
    node = next_node;
  }

  return checkmate_moves;
}

/**
 * Retrieves a new search node.
 * If a node for the same board exists in the cache, it returns that node.
 * Returns nullptr if the maximum number of nodes is reached.
 * @param board Board information
 * @param depth Current search depth
 * @return Pointer to the search node
 */
PnSearchNode* PnSearchEngine::_getNode(const Board* board, int32_t depth) {
  // Return nullptr if the maximum number of nodes is reached
  if (_nodeCount >= _nodeSize) {
    return nullptr;
  }

  // Declare a variable for the node representing the board state
  PnSearchNode* node = nullptr;

  // Check the node cache
  BoardHash node_key(board);
  auto it = _nodeCache.find(node_key);

  // If the node exists in the cache
  if (it != _nodeCache.end()) {
    // Use the cached node
    node = it->second;
  }
  // If the node does not exist in the cache
  else {
    // Get a new node from the node pool
    node = &_nodes[_nodeCount];

    _nodeCount += 1;

    // Initialize the node
    node->initialize(board, depth);

    // Register the new node in the cache
    _nodeCache[node_key] = node;
  }

  return node;
}

}  // namespace deepshogi

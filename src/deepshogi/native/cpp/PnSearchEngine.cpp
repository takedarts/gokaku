#include "PnSearchEngine.h"

#include <algorithm>
#include <set>

namespace deepshogi {

// Constant representing a terminal node
static const PnSearchNode END_NODE = PnSearchNode();

/**
 * Constructs a PN search engine object.
 * @param nodeSize Maximum number of nodes for the search
 */
PnSearchEngine::PnSearchEngine(int32_t nodeSize)
    : _nodes(nodeSize),
      _nodeSize(nodeSize),
      _nodeCount(0),
      _nodeCache() {
}

/**
 * Searches for a checkmate sequence and returns the move sequence.
 * Returns an empty array if no checkmate sequence is found.
 * @param board Object holding the board state
 * @param depth Search depth
 * @return Move sequence for the checkmate
 */
std::vector<Move> PnSearchEngine::getCheckmateMoves(const Board* board, int32_t depth) {
  // Search depth must be odd
  depth = std::max((depth % 2 == 0) ? depth + 1 : depth, 1);

  // Reset the node pool
  _nodeCount = 0;
  _nodeCache.clear();

  // Create the root node
  PnSearchNode* root = _getNode(board, 0);

  // Run the search
  std::vector<PnSearchNode*> parents;

  // Continue searching until checkmate or non-checkmate is determined
  // Stop if the maximum number of search iterations is reached
  while (root->getPn() != 0 && root->getDn() != 0) {
    // Traverse from the root node down to a terminal node
    PnSearchNode* node = root;
    PnSearchNode* next_node = node->getNextNode();

    parents.clear();

    // Descend until a terminal node is reached
    while (next_node != nullptr) {
      // Add the current node to the parent list to detect loops
      parents.push_back(node);

      // If the next node is equal to or inferior to any ancestor node,
      // replace it with the terminal non-checkmate node
      if (std::any_of(parents.begin(), parents.end(), [next_node](PnSearchNode* parent) {
            return next_node->isLesserThanOrEqual(parent);
          })) {
        node->replaceChildNode(next_node, const_cast<PnSearchNode*>(&END_NODE));
        next_node = const_cast<PnSearchNode*>(&END_NODE);
      }

      // Move to the next node
      node = next_node;
      next_node = node->getNextNode();
    }

    // If the current node is not a checkmate/non-checkmate node, expand it
    if (node->getPn() != 0 && node->getDn() != 0) {
      // Expand the node
      // A failure to expand means the maximum node count has been reached,
      // so terminate the search
      if (!node->expand(this)) {
        break;
      }

      // Update the PN/DN values of the terminal node
      node->update(depth);
    }

    // Update the PN/DN values of ancestor nodes
    // Even if the terminal node is a checkmate/non-checkmate node, the ancestors must still be updated.
    // Updating ancestors propagates the terminal node result up to the root node.
    for (auto it = parents.rbegin(); it != parents.rend(); it++) {
      (*it)->update(depth);
    }
  }

  // Retrieve the checkmate move sequence
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
 * Returns the cached node if one with the same board state exists.
 * Returns nullptr if the maximum number of nodes has been reached.
 * @param board Board state to search
 * @param depth Current search depth
 * @return Pointer to the search node
 */
PnSearchNode* PnSearchEngine::_getNode(const Board* board, int32_t depth) {
  // Return nullptr if the maximum node count has been reached
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

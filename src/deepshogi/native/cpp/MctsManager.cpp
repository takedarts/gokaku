#include "MctsManager.h"

#include <sstream>

#include "MctsNode.h"

namespace deepshogi {

/**
 * Creates an MCTS search node manager object.
 * @param parameter MCTS search parameters
 */
MctsManager::MctsManager(const MctsParameter& parameter)
    : _mutex(),
      _parameter(parameter),
      _nodes(),
      _poolNodes(),
      _usedNodes() {
}

/**
 * Creates a node object.
 * Returns an unused node object if one exists, otherwise creates a new one.
 * @return Node object
 */
MctsNode* MctsManager::createNode() {
  std::lock_guard<std::mutex> lock(_mutex);

  // Create a node object
  // Use an unused node object if one exists
  MctsNode* node;

  if (_poolNodes.empty()) {
    _nodes.emplace_back(std::make_unique<MctsNode>(this));
    node = _nodes.back().get();
  } else {
    node = _poolNodes.back();
    _poolNodes.pop_back();
  }

  // Register as a node object currently in use
  _usedNodes.insert(node);

  // Return the node object
  return node;
}

/**
 * Marks a node object as unused.
 * @param node Node object
 */
void MctsManager::releaseNode(MctsNode* node) {
  std::lock_guard<std::mutex> lock(_mutex);

  // Do nothing if it is not a registered node object
  if (_usedNodes.find(node) == _usedNodes.end()) {
    return;
  }

  // Mark the node object as unused
  _usedNodes.erase(node);
  _poolNodes.push_back(node);
}

/**
 * Marks the specified node object and all its child node objects as unused.
 * @param root Node object
 */
void MctsManager::releaseTree(MctsNode* root) {
  std::vector<MctsNode*> stack = {root};

  // Search depth-first and mark nodes as unused
  while (!stack.empty()) {
    MctsNode* current = stack.back();
    stack.pop_back();

    for (MctsNode* child : current->getChildren()) {
      stack.push_back(child);
    }

    releaseNode(current);
  }
}

/**
 * Gets the state of the MCTS search node manager object as a string.
 * @return String representing the state
 */
std::string MctsManager::toString() {
  std::lock_guard<std::mutex> lock(_mutex);
  std::ostringstream ss;

  ss << "NodeManager: nodes=" << _nodes.size();
  ss << "(" << _usedNodes.size() << "/" << _poolNodes.size() << ")";
  ss << std::endl;

  return ss.str();
}

}  // namespace deepshogi

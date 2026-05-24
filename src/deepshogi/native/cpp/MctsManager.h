#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "MctsParameter.h"

namespace deepshogi {

// Class that manages MCTS search node objects.
// MctsNode class is defined in MctsNode.h.
// MctsNode class is forward-declared due to circular reference.
class MctsNode;

/**
 * Class that manages MCTS search node objects.
 */
class MctsManager {
 private:
  friend class Player;

 public:
  /**
   * Creates an MCTS search node manager object.
   * @param parameter MCTS search parameters
   */
  MctsManager(const MctsParameter& parameter);

  /**
   * Destroys the MCTS search node manager object.
   */
  virtual ~MctsManager() = default;

  /**
   * Creates a node object.
   * Returns an unused node object if one exists, otherwise creates a new one.
   * @return Node object
   */
  MctsNode* createNode();

  /**
   * Marks a node object as unused.
   * @param node Node object
   */
  void releaseNode(MctsNode* node);

  /**
   * Marks the specified node object and all its child node objects as unused.
   * @param root Node object
   */
  void releaseTree(MctsNode* root);

  /**
   * Gets the state of the MCTS search node manager object as a string.
   * @return String representing the state
   */
  std::string toString();

  /**
   * Gets the MCTS search parameters.
   * @return MCTS search parameters
   */
  inline const MctsParameter& getParameter() const {
    return _parameter;
  }

 private:
  /**
   * Synchronization object.
   */
  std::mutex _mutex;

  /**
   * MCTS search parameters.
   */
  MctsParameter _parameter;

  /**
   * List of node objects.
   */
  std::vector<std::unique_ptr<MctsNode>> _nodes;

  /**
   * List of unused node objects.
   */
  std::vector<MctsNode*> _poolNodes;

  /**
   * List of node objects currently in use.
   */
  std::set<MctsNode*> _usedNodes;
};

}  // namespace deepshogi

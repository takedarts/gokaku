#pragma once

#include <mutex>
#include <vector>

#include "Node.h"
#include "NodeParameter.h"

namespace deepshogi {

/**
 * Class for managing node objects.
 */
class NodeManager {
 public:
  /**
   * Create a management class for node objects.
   * @param parameter Configuration parameters for node objects
   */
  NodeManager(NodeParameter parameter);

  /**
   * Create a node object.
   * If there is an unused node object, return it; otherwise, create a new one.
   * @return Node object
   */
  Node* createNode();

  /**
   * Set the node object to unused state.
   * @param node Node object
   */
  void releaseNode(Node* node);

  /**
   * Output information about this node manager object.
   * @param os Output destination
   */
  void print(std::ostream& os = std::cout);

 private:
  /**
   * Synchronization object.
   */
  std::mutex _mutex;

  /**
   * Parameters used when generating nodes.
   */
  NodeParameter _parameter;

  /**
   * List of node objects.
   */
  std::vector<std::unique_ptr<Node>> _nodes;

  /**
   * List of unused node objects.
   */
  std::vector<Node*> _poolNodes;

  /**
   * List of node objects in use.
   */
  std::set<Node*> _usedNodes;
};

}  // namespace deepshogi

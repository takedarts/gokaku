#pragma once

#include <cstdint>

namespace deepshogi {
class Node;

/**
 * Class representing the evaluation result of a node.
 */
class NodeResult {
 public:
  /**
   * Create an object for the evaluation result of a node.
   * @param node Node object to be evaluated next
   * @param value Evaluation value
   * @param playouts Number of playouts
   */
  NodeResult(Node* node, float value, int32_t playouts);

  /**
   * Create an object for the evaluation result of a node.
   */
  NodeResult();

  /**
   * Get the node object to be evaluated next.
   * @return Node object to be evaluated next
   */
  Node* getNode();

  /**
   * Get the evaluation value.
   * @return Evaluation value
   */
  float getValue();

  /**
   * Get the number of playouts.
   * @return Number of playouts
   */
  int32_t getPlayouts();

 private:
  /**
   * Node object to be evaluated next.
   */
  Node* _node;

  /**
   * Evaluation value.
   */
  float _value;

  /**
   * Number of playouts.
   */
  int32_t _playouts;
};

}  // namespace deepshogi

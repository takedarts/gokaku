#include "NodeResult.h"

#include "Node.h"

namespace deepshogi {

/**
 * Create an object for the evaluation result of a node.
 * @param node Node object to be evaluated next
 * @param value Evaluation value
 * @param playouts Number of playouts
 */
NodeResult::NodeResult(Node* node, float value, int32_t playouts)
    : _node(node),
      _value(value),
      _playouts(playouts) {
}

/**
 * Create an object for the evaluation result of a node.
 */
NodeResult::NodeResult()
    : _node(nullptr),
      _value(0.0f) {
}

/**
 * Get the node object to be evaluated next.
 * @return Node object to be evaluated next
 */
Node* NodeResult::getNode() {
  return _node;
}

/**
 * Get the evaluation value.
 * @return Evaluation value
 */
float NodeResult::getValue() {
  return _value;
}

/**
 * Get the number of playouts.
 * @return Number of playouts
 */
int32_t NodeResult::getPlayouts() {
  return _playouts;
}

}  // namespace deepshogi

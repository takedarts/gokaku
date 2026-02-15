#include "DfpnNodeKey.h"

namespace deepshogi {

/**
 * Construct a DfpnNodeKey object.
 * @param node pointer to a DfpnNode object
 */
DfpnNodeKey::DfpnNodeKey(DfpnNode* node)
    : _node(node) {
}

/**
 * Return true if this node is equal to the specified node.
 */
bool DfpnNodeKey::operator==(const DfpnNodeKey& other) const {
  return *_node == *(other._node);
}

/**
 * Return true if this node is less than the specified node.
 */
bool DfpnNodeKey::operator<(const DfpnNodeKey& other) const {
  return *_node < *(other._node);
}

}  // namespace deepshogi

#pragma once

#include "DfpnNode.h"

namespace deepshogi {

/**
 * Class representing the key for the DfpnNode cache.
 */
class DfpnNodeKey {
 public:
  /**
   * Construct a DfpnNodeKey object.
   * @param node pointer to a DfpnNode object
   */
  DfpnNodeKey(DfpnNode* node);

  /**
   * Return true if this node is less than the specified node.
   */
  bool operator<(const DfpnNodeKey& other) const;

 private:
  /**
   * Pointer to the node.
   */
  DfpnNode* _node;
};

}  // namespace deepshogi

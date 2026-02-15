#pragma once

#include <cstdint>

#include "Board.h"
#include "Move.h"

namespace deepshogi {
class DfpnEngine;

/**
 * Class representing a search node for the DFPN algorithm.
 */
class DfpnNode {
 public:
  /**
   * Construct a DFPN node object.
   */
  DfpnNode();

  /**
   * Destroy the DFPN node object.
   */
  virtual ~DfpnNode() = default;

  /**
   * Initialize this node as a root node with the specified board information.
   * @param board board object
   * @param depth node depth
   */
  void initialize(const Board* board, int32_t depth);

  /**
   * Expand the node to generate child nodes.
   * @param engine DFPN engine object
   * @return true if the node was expanded
   */
  bool expand(DfpnEngine* engine);

  /**
   * Update this node's PN/DN values.
   * @param depth_limit depth limit
   */
  void update(int32_t depth_limit);

  /**
   * Get the next child node to explore.
   * Returns nullptr if this node is a leaf.
   * @return pointer to the next child node to explore
   */
  DfpnNode* getNextNode();

  /**
   * Get the checkmate move and child node.
   * Returns nullptr if no checkmate move exists.
   * @return pair of checkmate move and child node
   */
  std::pair<Move, DfpnNode*> getCheckmateNode();

  /**
   * Replace a child node.
   * @param targetNode child node to replace
   * @param newNode new child node
   */
  void replaceChildNode(DfpnNode* targetNode, DfpnNode* newNode);

  /**
   * Get the child node for the specified move.
   * Returns nullptr if the child node does not exist.
   * @param move move
   * @return pointer to the child node
   */
  DfpnNode* getChildNode(const Move& move) const;

  /**
   * Get the node depth.
   * @return node depth
   */
  int32_t getDepth() const;

  /**
   * Get the PN value.
   * @return PN value
   */
  int32_t getPn() const;

  /**
   * Get the DN value.
   * @return DN value
   */
  int32_t getDn() const;

  /**
   * Get the number of moves until mate.
   * @return moves until mate
   */
  int32_t getStep() const;

  /**
   * Get the node size.
   * @return node size
   */
  int32_t getSize() const;

  /**
   * Get the node information as a string.
   * @return string with node information
   */
  std::string toString() const;

  /**
   * Return true if this node is less than the specified node.
   */
  bool operator<(const DfpnNode& other) const;

 private:
  /**
   * Board object.
   */
  Board _board;

  /**
   * Hash value of the board.
   */
  uint32_t _hashBoard;

  /**
   * Hash value of the hand.
   */
  uint32_t _hashHand;

  /**
   * Node depth.
   */
  int32_t _depth;

  /**
   * List of child nodes.
   */
  std::vector<std::pair<Move, DfpnNode*>> _children;

  /**
   * PN value.
   */
  int32_t _pn;

  /**
   * DN value.
   */
  int32_t _dn;

  /**
   * Number of moves until mate.
   */
  int32_t _step;

  /**
   * Node size.
   */
  int32_t _size;
};

}  // namespace deepshogi

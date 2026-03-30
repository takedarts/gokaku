#pragma once

#include <cstdint>

#include "Board.h"
#include "Move.h"

namespace deepshogi {
class PnSearchEngine;

/**
 * A class representing a search node in the PN search algorithm.
 */
class PnSearchNode {
 public:
  /**
   * Creates a PnSearchNode object.
   * Initializes it as a leaf node representing an unsolved position.
   */
  PnSearchNode();

  /**
   * Deletes the copy constructor.
   */
  PnSearchNode(const PnSearchNode& node) = delete;

  /**
   * Destroys the PnSearchNode object.
   */
  virtual ~PnSearchNode() = default;

  /**
   * Initializes this node as a leaf node with the specified board information.
   * @param board Board object
   * @param depth Depth of the node
   */
  void initialize(const Board* board, int32_t depth);

  /**
   * Expands the node and generates child nodes.
   * @param engine PN search engine object
   * @return true if the node was expanded successfully
   */
  bool expand(PnSearchEngine* engine);

  /**
   * Updates the PN/DN values of this node.
   * @param depth_limit the depth limit
   */
  void update(int32_t depth_limit);

  /**
   * Gets the next child node to search.
   * Returns nullptr if this node is a leaf node.
   * For attacking positions, returns the child node with minimum "PN value + log(search count)".
   * For defending positions, returns the child node with minimum "DN value + log(search count)".
   * Considering search count reduces search bias
   * and increases the likelihood of finding checkmates with fewer moves.
   * @return pointer to the next child node to search
   */
  PnSearchNode* getNextNode();

  /**
   * Gets the move and child node that lead to checkmate.
   * Returns nullptr if no child node leads to checkmate.
   * @return a pair of the move and child node
   */
  std::pair<Move, PnSearchNode*> getCheckmateNode();

  /**
   * Replaces the specified child node with a new child node.
   * @param targetNode the child node to replace
   * @param newNode the new child node
   */
  void replaceChildNode(PnSearchNode* targetNode, PnSearchNode* newNode);

  /**
   * Gets the node information as a string.
   * @return the string representation of the node information
   */
  std::string toString() const;

  /**
   * Gets the depth of the node.
   * @return the depth of the node
   */
  inline int32_t getDepth() const {
    return _depth;
  }

  /**
   * Gets the PN value.
   * @return the PN value
   */
  inline int32_t getPn() const {
    return _pn;
  }

  /**
   * Gets the DN value.
   * @return the DN value
   */
  inline int32_t getDn() const {
    return _dn;
  }

  /**
   * Gets the number of moves to checkmate.
   * @return the number of moves to checkmate
   */
  inline int32_t getStep() const {
    return _step;
  }

  /**
   * Gets the size of the node.
   * @return the size of the node
   */
  inline int32_t getSize() const {
    return _size;
  }

  /**
   * Returns true if this node is a subordinate node of the specified node.
   * A subordinate node means a node with the same piece placement on the board,
   * and the same or fewer pieces in hand for all types.
   * The turn of the side giving check becomes the evaluation target turn.
   * @param node the node to compare against
   * @return true if this is a subordinate node
   */
  inline bool isLesserThan(const PnSearchNode* node) const {
    // Turn of the attacking side: verify board piece placement is the same
    // and own hand pieces are the same or fewer
    if (_depth % 2 == 1) {
      return _board.isLesserThan(node->_board, _board.getColor());
    }
    // Turn of the defending side: verify board piece placement is the same
    // and opponent's hand pieces are the same or fewer
    else {
      return _board.isLesserThan(node->_board, OPPOSITE_COLOR(_board.getColor()));
    }
  }

 private:
  /**
   * The board object.
   */
  Board _board;

  /**
   * The depth of the node.
   */
  int32_t _depth;

  /**
   * The list of child nodes.
   */
  std::vector<std::pair<Move, PnSearchNode*>> _children;

  /**
   * The PN value.
   */
  int32_t _pn;

  /**
   * The DN value.
   */
  int32_t _dn;

  /**
   * The number of moves to checkmate.
   */
  int32_t _step;

  /**
   * The size of the node.
   */
  int32_t _size;
};

}  // namespace deepshogi

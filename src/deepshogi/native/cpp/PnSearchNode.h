#pragma once

#include <cstdint>

#include "Board.h"
#include "Move.h"

namespace deepshogi {
class PnSearchEngine;

/**
 * Class representing a search node in the PN search algorithm.
 */
class PnSearchNode {
 public:
  /**
   * Constructs a PN search node object.
   * Initializes the node as a terminal node representing a non-checkmate state.
   */
  PnSearchNode();

  /**
   * Deleted copy constructor.
   */
  PnSearchNode(const PnSearchNode& node) = delete;

  /**
   * Destroys the PN search node object.
   */
  virtual ~PnSearchNode() = default;

  /**
   * Initializes this node as a terminal node with the specified board state.
   * @param board Board object
   * @param depth Depth of the node
   */
  void initialize(const Board* board, int32_t depth);

  /**
   * Expands the node to generate child nodes.
   * @param engine PN search engine object
   * @return true if the node was successfully expanded
   */
  bool expand(PnSearchEngine* engine);

  /**
   * Updates the PN/DN values of this node.
   * @param depth_limit Depth limit
   */
  void update(int32_t depth_limit);

  /**
   * Returns the next child node to search.
   * Returns nullptr if this node is a terminal node.
   * For the checking side, returns the child node with the minimum "PN value + log(search count)".
   * For the evading side, returns the child node with the minimum "DN value + log(search count)".
   * Computing priority with the search count reduces search bias
   * and increases the chance of finding shorter checkmate sequences.
   * @return Pointer to the next child node to search
   */
  PnSearchNode* getNextNode();

  /**
   * Returns the move and child node for the checkmate sequence.
   * Returns nullptr if no child node forming a checkmate sequence exists.
   * @return Pair of the checkmate move and child node
   */
  std::pair<Move, PnSearchNode*> getCheckmateNode();

  /**
   * Replaces the specified child node with a new child node.
   * @param targetNode Child node to replace
   * @param newNode New child node
   */
  void replaceChildNode(PnSearchNode* targetNode, PnSearchNode* newNode);

  /**
   * Returns the node information as a string.
   * @return String representation of the node information
   */
  std::string toString() const;

  /**
   * Returns the depth of the node.
   * @return Depth of the node
   */
  inline int32_t getDepth() const {
    return _depth;
  }

  /**
   * Returns the PN value.
   * @return PN value
   */
  inline int32_t getPn() const {
    return _pn;
  }

  /**
   * Returns the DN value.
   * @return DN value
   */
  inline int32_t getDn() const {
    return _dn;
  }

  /**
   * Returns the number of moves to checkmate.
   * @return Number of moves to checkmate
   */
  inline int32_t getStep() const {
    return _step;
  }

  /**
   * Returns the size of the node.
   * @return Size of the node
   */
  inline int32_t getSize() const {
    return _size;
  }

  /**
   * Returns true if this node is the same as or inferior to the specified node.
   * An inferior node has the same board piece arrangement and the same or fewer
   * pieces in hand for every piece type.
   * The side delivering check is the turn evaluated.
   * @param node Node to compare against
   * @return true if this node is equal to or inferior to the specified node
   */
  inline bool isLesserThanOrEqual(const PnSearchNode* node) const {
    // Checking side's turn: verify that the board piece arrangement is the same and own pieces in hand are the same or fewer
    if (_depth % 2 == 1) {
      return _board.isLesserThanOrEqual(node->_board, _board.getColor());
    }
    // Evading side's turn: verify that the board piece arrangement is the same and opponent's pieces in hand are the same or fewer
    else {
      return _board.isLesserThanOrEqual(node->_board, OPPOSITE_COLOR(_board.getColor()));
    }
  }

 private:
  /**
   * Board object.
   */
  Board _board;

  /**
   * Depth of the node.
   */
  int32_t _depth;

  /**
   * List of child nodes.
   */
  std::vector<std::pair<Move, PnSearchNode*>> _children;

  /**
   * PN value.
   */
  int32_t _pn;

  /**
   * DN value.
   */
  int32_t _dn;

  /**
   * Number of moves to checkmate.
   */
  int32_t _step;

  /**
   * Size of the node.
   */
  int32_t _size;
};

}  // namespace deepshogi

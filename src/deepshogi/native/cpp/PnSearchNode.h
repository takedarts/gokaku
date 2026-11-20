#pragma once

#include <cstdint>
#include <functional>

#include "Board.h"
#include "Move.h"

namespace deepshogi {
class PnSearchEngine;

/**
 * Represent a node in the PN search algorithm.
 */
class PnSearchNode {
 private:
  /**
   * Allow DF-PN search to access node values and child references.
   */
  friend class PnSearchEngine;

 public:
  /**
   * Construct a PN search node.
   * Initialize it as a terminal node representing no checkmate.
   */
  PnSearchNode();

  /**
   * Deleted copy constructor.
   * @param node Source object; copying is disabled.
   */
  PnSearchNode(const PnSearchNode& node) = delete;

  /**
   * Destroy the PN search node.
   */
  virtual ~PnSearchNode();

  /**
   * Initialize this node as a leaf for the specified position.
   * @param board Target position.
   * @param depth Remaining search depth in moves.
   */
  void initialize(const Board* board, int32_t depth);

  /**
   * Copy child references and search values without cloning the children themselves.
   * @param node Source node.
   */
  void copyFrom(const PnSearchNode* node);

  /**
   * Expand the node into children, retaining already expanded children.
   * @param engine Search engine.
   * @param board Working position, restored on return.
   * @param isPreviousPosition Function that identifies positions before the search started
   *     (empty for the conventional search).
   * @return False when node capacity is exhausted or an abort is requested, otherwise true.
   */
  bool expand(
      PnSearchEngine* engine, Board* board,
      const std::function<bool(const BoardHash&)>& isPreviousPosition = {});

  /**
   * Update this node's PN/DN values, preserving terminal values when there are no children.
   */
  void update();

  /**
   * Get the next child to search.
   * Return nullptr as the child when this node is terminal.
   * Minimize PN for attackers and DN for defenders, breaking ties by subtree size.
   * @param depth Current requested remaining depth.
   * @return Pair of the next move and child node.
   */
  std::pair<Move, PnSearchNode*> getNextNode(int32_t depth);

  /**
   * Get the move and child node on the checkmating line.
   * Return nullptr if no child lies on a checkmating line.
   * @return Pair of a checkmating move and its child node.
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
   * Return the PN value without a move limit.
   * @return PN value without a move limit.
   */
  inline int32_t getContPn() const {
    return _contPn;
  }

  /**
   * Return the DN value without a move limit.
   * @return DN value without a move limit.
   */
  inline int32_t getContDn() const {
    return _contDn;
  }

  /**
   * Return the PN value with a move limit.
   * @return PN value with a move limit.
   */
  inline int32_t getTermPn() const {
    return _termPn;
  }

  /**
   * Return the DN value with a move limit.
   * @return DN value with a move limit.
   */
  inline int32_t getTermDn() const {
    return _termDn;
  }

  /**
   * Return the depth associated with term values.
   * @return Depth associated with term values.
   */
  inline int32_t getTermDepth() const {
    return _termDepth;
  }

  /**
   * Return the internal value used to compare checkmate sequence lengths.
   * @return Internal value for comparing checkmate sequence lengths.
   */
  inline int32_t getStep() const {
    return _step;
  }

  /**
   * Return the aggregate subtree size used for search priority.
   * @return Aggregate subtree size.
   */
  inline int32_t getSize() const {
    return _size;
  }

 private:
  /**
   * List of child nodes.
   */
  std::vector<std::pair<Move, PnSearchNode*>> _children;

  /**
   * Legal moves generated during initialization.
   * Reuse their generation order when expanding again from depth zero.
   */
  std::vector<Move> _legalMoves;

  /**
   * Current requested remaining depth: odd for attackers, even for defenders.
   */
  int32_t _depth;

  /**
   * PN value without a move limit.
   */
  int32_t _contPn;

  /**
   * PN value with a move limit.
   */
  int32_t _termPn;

  /**
   * DN value without a move limit.
   */
  int32_t _contDn;

  /**
   * DN value with a move limit.
   */
  int32_t _termDn;

  /**
   * Aggregate depth updated together with term values.
   */
  int32_t _termDepth;

  /**
   * Depth at which the term result was solved; not used to increase search depth.
   */
  int32_t _termResultDepth;

  /**
   * Internal value used to compare checkmate sequence lengths.
   */
  int32_t _step;

  /**
   * Size aggregated from children for search priority calculation.
   */
  int32_t _size;

  /**
   * Return this child's PN and DN for the requested depth.
   * @param depth Requested remaining depth.
   * @return PN and DN values.
   */
  std::pair<int32_t, int32_t> _getValues(int32_t depth) const;
};

}  // namespace deepshogi

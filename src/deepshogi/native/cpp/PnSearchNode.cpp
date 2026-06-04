#include "PnSearchNode.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

#include "PnSearchEngine.h"

namespace deepshogi {

// Maximum value to set in PN search nodes
static constexpr int32_t MAX_VALUE = 0xffffff;

/**
 * Constructs a PN search node object.
 * Initializes the node as a terminal node representing a non-checkmate state.
 */
PnSearchNode::PnSearchNode()
    : _board(),
      _depth(MAX_VALUE),
      _children(),
      _pn(MAX_VALUE),
      _dn(0),
      _step(MAX_VALUE),
      _size(1) {
}

/**
 * Initializes this node as a terminal node with the specified board state.
 * @param board Board object
 * @param depth Depth of the node
 */
void PnSearchNode::initialize(const Board* board, int32_t depth) {
  // Copy the board and set values for a terminal node representing non-checkmate
  _board.copyFrom(board);
  _depth = depth;
  _children.clear();
  _pn = MAX_VALUE;
  _dn = 0;
  _step = MAX_VALUE;
  _size = 1;

  // Treat as non-checkmate if the maximum number of moves has been reached
  if (_board.getTurn() >= _board.getDrawTurn()) {
    return;
  }

  // Set PN/DN values
  if (_depth % 2 == 0) {
    // Checking side's turn
    // Can deliver check: PN=1, DN=number of legal moves, moves to checkmate=max
    // Cannot deliver check: PN=max, DN=0, moves to checkmate=max
    std::vector<Move> legal_moves = _board.getLegalMoves(false, true);

    if (!legal_moves.empty()) {
      _pn = 1;
      _dn = (int32_t)legal_moves.size();
      _step = MAX_VALUE;
    } else {
      _pn = MAX_VALUE;
      _dn = 0;
      _step = MAX_VALUE;
    }
  } else {
    // Evading side's turn
    // Can evade check: PN=number of legal moves, DN=1, moves to checkmate=max
    // Cannot evade check: PN=0, DN=max, moves to checkmate=1
    // Moves that drop a piece in hand to the same square are counted as one move even if the piece type differs
    std::vector<Move> legal_moves = _board.getLegalMoves(false, false);

    if (!legal_moves.empty()) {
      std::set<Position> unique_hand_moves;
      int32_t board_move_count = 0;

      for (Move& move : legal_moves) {
        if (move.getSrc().getX() == BOARD_SIZE) {
          unique_hand_moves.insert(move.getDst());
        } else {
          board_move_count++;
        }
      }

      _pn = board_move_count + (int32_t)unique_hand_moves.size();
      _dn = 1;
      _step = MAX_VALUE;
    } else {
      _pn = 0;
      _dn = MAX_VALUE;
      _step = 1;
    }
  }
}

/**
 * Expands the node to generate child nodes.
 * @param engine PN search engine object
 * @return true if the node was successfully expanded
 */
bool PnSearchNode::expand(PnSearchEngine* engine) {
  // Clear child nodes
  _children.clear();

  // Determine the rule for generating legal moves
  // Even depth: checking side's turn (generate only checking moves)
  // Odd depth: evading side's turn (also generate non-checking moves)
  bool checkmate = (_depth % 2 == 0) ? true : false;

  // Register child nodes
  Board board;

  for (Move& move : _board.getLegalMoves(false, checkmate)) {
    // Create a board state
    board.copyFrom(&_board);
    board.play(move);

    // Retrieve the node object
    // If a node with the same board state exists in the cache, return that node
    // If the maximum number of nodes has been reached, return nullptr
    PnSearchNode* child_node = engine->_getNode(&board, _depth + 1);

    if (child_node == nullptr) {
      return false;
    }

    // Set the depth of the child node
    // If a node with the same board state exists in the node cache,
    // and its depth is greater than depth + 1, correct the depth to depth + 1
    if (child_node->_depth > _depth + 1) {
      child_node->_depth = _depth + 1;
    }

    // Add to the child node list
    _children.push_back(std::make_pair(move, child_node));
  }

  return true;
}

/**
 * Updates the PN/DN values of this node.
 * @param depth_limit Depth limit
 */
void PnSearchNode::update(int32_t depth_limit) {
  // Treat as non-checkmate if the depth limit is reached
  if (_depth >= depth_limit) {
    _pn = MAX_VALUE;
    _dn = 0;
    _step = MAX_VALUE;
    _size = 1;
    return;
  }

  // Checking side's turn
  // PN value is the minimum of children's PN values, DN value is the sum of children's DN values,
  // and moves to checkmate is the minimum of children's moves to checkmate + 1
  if (_depth % 2 == 0) {
    _pn = MAX_VALUE;
    _dn = 0;
    _step = MAX_VALUE;
    _size = 1;

    for (auto& child_pair : _children) {
      PnSearchNode* child = child_pair.second;

      // PN value is the minimum of children's PN values
      if (child->_pn < _pn) {
        _pn = child->_pn;
      }

      // DN value is the sum of children's DN values
      _dn = std::min(_dn + child->_dn, MAX_VALUE);

      // Moves to checkmate is the minimum of children's moves to checkmate + 1
      if (_step > child->_step + 1) {
        _step = child->_step + 1;
      }

      // Node size is the sum of children's sizes
      _size = std::min(_size + child->_size, MAX_VALUE);
    }
  }
  // Evading side's turn
  // PN value is the sum of children's PN values, DN value is the minimum of children's DN values,
  // and moves to checkmate is the maximum of children's moves to checkmate + 1.
  // When computing the sum of children's PN values, if multiple moves drop a piece to the same square,
  // only the maximum PN value among those child nodes is added to the sum.
  else {
    std::map<Position, int32_t> hand_move_pns;
    _pn = 0;
    _dn = MAX_VALUE;
    _step = 1;
    _size = 1;

    for (auto& [move, child] : _children) {
      // When multiple moves drop a piece to the same square, add only the maximum PN value to the sum
      if (move.getSrc().getX() == BOARD_SIZE) {
        Position dst = move.getDst();

        if (hand_move_pns.find(dst) == hand_move_pns.end()) {
          _pn = std::min(_pn + child->_pn, MAX_VALUE);
          hand_move_pns[dst] = child->_pn;
        } else if (child->_pn > hand_move_pns[dst]) {
          _pn = std::min(_pn + child->_pn - hand_move_pns[dst], MAX_VALUE);
          hand_move_pns[dst] = child->_pn;
        }
      }
      // For moves that move a piece on the board, add the PN value directly to the sum
      else {
        _pn = std::min(_pn + child->_pn, MAX_VALUE);
      }

      // DN value is the minimum of children's DN values
      if (child->_dn < _dn) {
        _dn = child->_dn;
      }

      // Moves to checkmate is the maximum of children's moves to checkmate + 1
      if (_step < child->_step + 1) {
        _step = child->_step + 1;
      }

      // Node size is the sum of children's sizes
      _size = std::min(_size + child->_size, MAX_VALUE);
    }
  }
}

/**
 * Returns the next child node to search.
 * Returns nullptr if this node is a terminal node.
 * For the checking side, returns the child node with the minimum "PN value + log(search count)".
 * For the evading side, returns the child node with the minimum "DN value + log(search count)".
 * Computing priority with the search count reduces search bias
 * and increases the chance of finding shorter checkmate sequences.
 * @return Pointer to the next child node to search
 */
PnSearchNode* PnSearchNode::getNextNode() {
  PnSearchNode* next_node = nullptr;

  if (_depth % 2 == 0) {
    // Find the child node with the minimum PN value among undecided nodes
    // Use PN value plus log of search count as priority to account for search frequency
    float max_priority = 0.0f;

    for (auto& [move, child] : _children) {
      if (child->_pn == 0 || child->_dn == 0) {
        continue;
      }

      float priority = 1.0f / (child->_pn + std::log((float)child->_size));

      if (priority > max_priority) {
        max_priority = priority;
        next_node = child;
      }
    }
  } else {
    // Find the child node with the minimum DN value among undecided nodes
    // Use DN value plus log of search count as priority to account for search frequency
    float max_priority = 0.0f;

    for (auto& [move, child] : _children) {
      if (child->_pn == 0 || child->_dn == 0) {
        continue;
      }

      float priority = 1.0f / (child->_dn + std::log((float)child->_size));

      if (priority > max_priority) {
        max_priority = priority;
        next_node = child;
      }
    }
  }

  return next_node;
}

/**
 * Returns the move and child node for the checkmate sequence.
 * Returns nullptr if no child node forming a checkmate sequence exists.
 * @return Pair of the checkmate move and child node
 */
std::pair<Move, PnSearchNode*> PnSearchNode::getCheckmateNode() {
  PnSearchNode* checkmate_node = nullptr;
  Move checkmate_move(MOVE_INVALID);

  if (_depth % 2 == 0) {
    // For the checking side's turn, find the child node with PN=0 and minimum moves
    int32_t min_step = MAX_VALUE;

    for (auto& child_pair : _children) {
      PnSearchNode* child = child_pair.second;

      if (child->_pn == 0 && child->_step < min_step) {
        checkmate_node = child;
        checkmate_move = child_pair.first;
        min_step = child->_step;
      }
    }
  } else {
    // For the evading side's turn, find the child node with PN=0 and maximum moves
    int32_t max_step = 0;

    for (auto& child_pair : _children) {
      PnSearchNode* child = child_pair.second;

      if (child->_pn == 0 && child->_step > max_step) {
        checkmate_node = child;
        checkmate_move = child_pair.first;
        max_step = child->_step;
      }
    }
  }

  return std::make_pair(checkmate_move, checkmate_node);
}

/**
 * Replaces the specified child node with a new child node.
 * @param targetNode Child node to replace
 * @param newNode New child node
 */
void PnSearchNode::replaceChildNode(PnSearchNode* targetNode, PnSearchNode* newNode) {
  for (auto& [move, child] : _children) {
    if (child == targetNode) {
      child = newNode;
      return;
    }
  }
}

/**
 * Returns the node information as a string.
 * @return String representation of the node information
 */
std::string PnSearchNode::toString() const {
  std::stringstream ss;

  ss << "DFPN Node: depth=" << _depth << " pn=" << _pn << " dn=" << _dn
     << " step=" << _step << " size=" << _size << "\n"
     << _board << "\n";

  for (auto& child_pair : _children) {
    Move move = child_pair.first;
    PnSearchNode* child = child_pair.second;

    ss << "  Child Move: " << move
       << " pn=" << child->_pn
       << " dn=" << child->_dn
       << " step=" << child->_step
       << " size=" << child->_size
       << "\n";
  }

  return ss.str();
}

}  // namespace deepshogi

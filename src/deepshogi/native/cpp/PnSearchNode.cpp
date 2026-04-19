#include "PnSearchNode.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

#include "PnSearchEngine.h"

namespace deepshogi {

// Maximum value to set for PN search nodes
static constexpr int32_t MAX_VALUE = 0xffffff;

/**
 * Creates a PN search node object.
 * Initializes it as a node representing an unsolved leaf node.
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
 * Initializes this node as a leaf node with the specified board information.
 * @param board Board object
 * @param depth Depth of the node
 */
void PnSearchNode::initialize(const Board* board, int32_t depth) {
  // Copy the board and set values for an unsolved leaf node
  _board.copyFrom(board);
  _depth = depth;
  _children.clear();
  _pn = MAX_VALUE;
  _dn = 0;
  _step = MAX_VALUE;
  _size = 1;

  // If the maximum number of moves is reached, mark as unsolved
  if (_board.getTurn() >= _board.getDrawTurn()) {
    return;
  }

  // Set PN/DN values
  if (_depth % 2 == 0) {
    // For the turn to give check
    // If check can be given: PN=1, DN=number of legal moves, step=max value
    // If check cannot be given: PN=max value, DN=0, step=max value
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
    // For the turn to escape from check
    // If escape is possible: PN=number of legal moves, DN=1, step=max value
    // If escape is impossible: PN=0, DN=max value, step=1
    // Count drops to the same square as a single move even if the piece types differ.
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
 * Expands the node and generates child nodes.
 * @param engine PN search engine object
 * @return true if the node was expanded successfully
 */
bool PnSearchNode::expand(PnSearchEngine* engine) {
  // Clear child nodes
  _children.clear();

  // Determine the rule for generating legal moves
  // If depth is even: turn to give check (generate only moves that give check)
  // If depth is odd: turn to escape from check (generate moves other than check)
  bool checkmate = (_depth % 2 == 0) ? true : false;

  // Register child nodes for each legal move
  Board board;

  for (Move& move : _board.getLegalMoves(false, checkmate)) {
    // Create a new board
    board.copyFrom(&_board);
    board.play(move);

    // Get the node object
    // If a node with the same board exists in the cache, return that node
    // If the maximum number of nodes is reached, return nullptr
    PnSearchNode* child_node = engine->_getNode(&board, _depth + 1);

    if (child_node == nullptr) {
      return false;
    }

    // Set the depth of the child node
    // If a node with the same board exists in the cache,
    // and its depth is greater than `depth + 1`, adjust the depth to `depth + 1`
    if (child_node->_depth > _depth + 1) {
      child_node->_depth = _depth + 1;
    }

    // Add to the list of child nodes
    _children.push_back(std::make_pair(move, child_node));
  }

  return true;
}

/**
 * Updates the PN/DN values of this node.
 * @param depth_limit the depth limit
 */
void PnSearchNode::update(int32_t depth_limit) {
  // If the depth limit is reached, mark as unsolved
  if (_depth >= depth_limit) {
    _pn = MAX_VALUE;
    _dn = 0;
    _step = MAX_VALUE;
    _size = 1;
    return;
  }

  // If it's the turn to give check
  // PN value is the minimum of the PN values of the child nodes,
  // DN value is the sum of the DN values of the child nodes,
  // The number of moves to checkmate is the minimum of the number of moves
  // to checkmate of the child nodes + 1
  if (_depth % 2 == 0) {
    _pn = MAX_VALUE;
    _dn = 0;
    _step = MAX_VALUE;
    _size = 1;

    for (auto& child_pair : _children) {
      PnSearchNode* child = child_pair.second;

      if (child->_pn < _pn) {
        _pn = child->_pn;
      }

      _dn = std::min(_dn + child->_dn, MAX_VALUE);

      if (_step > child->_step + 1) {
        _step = child->_step + 1;
      }

      _size = std::min(_size + child->_size, MAX_VALUE);
    }
  }
  // If it's the turn to escape from check
  // PN value is the sum of the PN values of the child nodes,
  // DN value is the minimum of the DN values of the child nodes,
  // The number of moves to checkmate is the maximum of the number of moves
  // to checkmate of the child nodes + 1
  // When summing child PN values, if multiple drop moves target the same square,
  // add only the maximum PN value among those child nodes to the total.
  else {
    std::map<Position, int32_t> hand_move_pns;
    _pn = 0;
    _dn = MAX_VALUE;
    _step = 1;
    _size = 1;

    for (auto& [move, child] : _children) {
      // If multiple drop moves target the same square, add only the largest PN value to the total.
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
      // For board moves, add the PN value directly to the total.
      else {
        _pn = std::min(_pn + child->_pn, MAX_VALUE);
      }

      // Set DN to the minimum DN value among child nodes.
      if (child->_dn < _dn) {
        _dn = child->_dn;
      }

      // Set the mate distance to max(child mate distance) + 1.
      if (_step < child->_step + 1) {
        _step = child->_step + 1;
      }

      // Set node size to the sum of child node sizes.
      _size = std::min(_size + child->_size, MAX_VALUE);
    }
  }
}

/**
 * Gets the next child node to search.
 * Returns nullptr if this node is a leaf node.
 * For attacking positions, returns the child node with minimum "PN value + log(search count)".
 * For defending positions, returns the child node with minimum "DN value + log(search count)".
 * Considering search count reduces search bias
 * and increases the likelihood of finding checkmates with fewer moves.
 * @return pointer to the next child node to search
 */
PnSearchNode* PnSearchNode::getNextNode() {
  PnSearchNode* next_node = nullptr;

  if (_depth % 2 == 0) {
    // Find the child node with the minimum PN value among the nodes whose checkmate/escape status is not yet determined
    // To consider the number of explorations, add the logarithm of the number of explorations to the PN value as the priority
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
    // Find the child node with the minimum DN value among the nodes
    // whose checkmate/escape status is not yet determined
    // To consider the number of explorations,
    // add the logarithm of the number of explorations to the DN value as the priority
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
 * Gets the move and child node that lead to checkmate.
 * Returns nullptr if no child node leads to checkmate.
 * @return a pair of the move and child node
 */
std::pair<Move, PnSearchNode*> PnSearchNode::getCheckmateNode() {
  PnSearchNode* checkmate_node = nullptr;
  Move checkmate_move(MOVE_INVALID);

  if (_depth % 2 == 0) {
    // If it's the turn to give check,
    // find the child node with PN value 0 and the minimum number of moves to checkmate
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
    // If it's the turn to escape from check,
    // find the child node with PN value 0 and the maximum number of moves to checkmate
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
 * @param targetNode the child node to replace
 * @param newNode the new child node
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
 * Gets the node information as a string.
 * @return the string representation of the node information
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

#include "DfpnNode.h"

#include <algorithm>
#include <sstream>

#include "DfpnEngine.h"

namespace deepshogi {

/**
 * Construct a DFPN node object.
 */
DfpnNode::DfpnNode()
    : _board(),
      _hashBoard(0),
      _hashHand(0),
      _depth(0xffff),
      _children(),
      _pn(0xffff),
      _dn(0),
      _step(0xffff),
      _size(1) {
}

/**
 * Initialize this node as a root node with the specified board information.
 * @param board board object
 * @param depth node depth
 */
void DfpnNode::initialize(const Board* board, int32_t depth) {
  _board.copyFrom(board);
  _children.clear();
  _depth = depth;
  _size = 1;

  // Calculate the hash value of the board
  _hashBoard = 0;
  _hashHand = 0;

  for (int32_t y = 0; y < BOARD_SIZE; ++y) {
    for (int32_t x = 0; x < BOARD_SIZE; ++x) {
      int32_t piece = _board.getPiece(x, y);

      _hashBoard ^= (static_cast<uint32_t>(piece) << ((x + y * BOARD_SIZE) % 24));
    }
  }

  for (int32_t c : {COLOR_BLACK, COLOR_WHITE}) {
    int32_t off = (c == COLOR_BLACK) ? 0 : (PIECE_HAND_END - PIECE_HAND_BEGIN);

    for (int32_t p = PIECE_HAND_BEGIN; p < PIECE_HAND_END; ++p) {
      int32_t num = _board.getHandPieceNum(c, p);
      _hashHand ^= (static_cast<uint32_t>(num) << ((off + (p - PIECE_HAND_BEGIN)) % 24));
    }
  }

  // Set PN/DN values
  if (_depth % 2 == 0) {
    std::vector<Move> legal_moves = _board.getLegalMoves(false, true);

    if (legal_moves.empty()) {
      _pn = 0xffff;
      _dn = 0;
      _step = 0xffff;
    } else {
      _pn = 1;
      _dn = legal_moves.size();
      _step = 0xffff;
    }
  } else {
    std::vector<Move> legal_moves = _board.getLegalMoves(false, false);

    if (legal_moves.empty()) {
      _pn = 0;
      _dn = 0xffff;
      _step = 1;
    } else {
      _pn = legal_moves.size();
      _dn = 1;
      _step = 0xffff;
    }
  }
}

/**
 * Expand the node to generate child nodes.
 * @param engine DFPN engine object
 * @return true if the node was expanded
 */
bool DfpnNode::expand(DfpnEngine* engine) {
  // Clear child nodes
  _children.clear();

  // Register child nodes
  bool checkmate = (_depth % 2 == 0) ? true : false;
  Board board;

  for (Move& move : _board.getLegalMoves(false, checkmate)) {
    // Create the board
    board.copyFrom(&_board);
    board.play(move);

    // Obtain the node object for the child node from the engine
    DfpnNode* child_node = engine->getNode(&board, _depth + 1);

    if (child_node == nullptr) {
      return false;
    }

    // Set the depth of the child node
    if (child_node->_depth > _depth + 1) {
      child_node->_depth = _depth + 1;
    }

    // Add to the child node list
    _children.push_back(std::make_pair(move, child_node));
  }

  return true;
}

/**
 * Update this node's PN/DN values.
 * @param depth_limit depth limit
 */
void DfpnNode::update(int32_t depth_limit) {
  // If the depth limit is reached, consider it as a non-mate node
  if (_depth >= depth_limit) {
    _pn = 0xffff;
    _dn = 0;
    _step = 0xffff;
    _size = 1;
    return;
  }

  // If it's the turn to give check
  if (_depth % 2 == 0) {
    _pn = 0xffff;
    _dn = 0;
    _step = 0xffff;
    _size = 1;

    for (auto& child_pair : _children) {
      DfpnNode* child = child_pair.second;

      if (child->getPn() < _pn) {
        _pn = child->getPn();
      }

      _dn += child->getDn();

      if (_step > child->getStep() + 1) {
        _step = child->getStep() + 1;
      }

      _size += child->getSize();
    }
  }
  // If it's the turn to escape from check
  else {
    _pn = 0;
    _dn = 0xffff;
    _step = 1;
    _size = 1;

    for (auto& child_pair : _children) {
      DfpnNode* child = child_pair.second;

      _pn += child->getPn();

      if (child->getDn() < _dn) {
        _dn = child->getDn();
      }

      if (_step < child->getStep() + 1) {
        _step = child->getStep() + 1;
      }

      _size += child->getSize();
    }
  }
}

/**
 * Get the next child node to explore.
 * Returns nullptr if this node is a leaf.
 * @return pointer to the next child node to explore
 */
DfpnNode* DfpnNode::getNextNode() {
  DfpnNode* next_node = nullptr;

  if (_depth % 2 == 0) {
    // Find the child node with the minimum PN value among the nodes
    // whose mate/non-mate status is not yet determined
    float min_priority = 0xffff;

    for (auto& child_pair : _children) {
      DfpnNode* child = child_pair.second;

      if (child->getPn() == 0 || child->getDn() == 0) {
        continue;
      }

      float priority = child->getPn() + std::log(child->getSize());

      if (priority < min_priority) {
        min_priority = priority;
        next_node = child;
      }
    }
  } else {
    // Find the child node with the minimum DN value among the nodes
    // whose mate/non-mate status is not yet determined
    float min_priority = 0xffff;

    for (auto& child_pair : _children) {
      DfpnNode* child = child_pair.second;

      if (child->getPn() == 0 || child->getDn() == 0) {
        continue;
      }

      float priority = child->getDn() + std::log(child->getSize());

      if (priority < min_priority) {
        min_priority = priority;
        next_node = child;
      }
    }
  }

  return next_node;
}

/**
 * Get the checkmate move and child node.
 * Returns nullptr if no checkmate move exists.
 * @return pair of checkmate move and child node
 */
std::pair<Move, DfpnNode*> DfpnNode::getCheckmateNode() {
  DfpnNode* checkmate_node = nullptr;
  Move checkmate_move(-1, -1, -1, -1, false);

  if (_depth % 2 == 0) {
    // If it's the turn to give check,
    // find the child node with PN value 0 and the minimum step value
    int32_t min_step = 0xffff;

    for (auto& child_pair : _children) {
      DfpnNode* child = child_pair.second;

      if (child->getPn() == 0 && child->getStep() < min_step) {
        checkmate_node = child;
        checkmate_move = child_pair.first;
        min_step = child->getStep();
      }
    }
  } else {
    // If it's the turn to escape from check,
    // find the child node with PN value 0 and the maximum step value
    int32_t max_step = 0;

    for (auto& child_pair : _children) {
      DfpnNode* child = child_pair.second;

      if (child->getPn() == 0 && child->getStep() > max_step) {
        checkmate_node = child;
        checkmate_move = child_pair.first;
        max_step = child->getStep();
      }
    }
  }

  return std::make_pair(checkmate_move, checkmate_node);
}

/**
 * Replace a child node.
 * @param targetNode child node to replace
 * @param newNode new child node
 */
void DfpnNode::replaceChildNode(DfpnNode* targetNode, DfpnNode* newNode) {
  for (auto& child_pair : _children) {
    if (child_pair.second == targetNode) {
      child_pair.second = newNode;
      return;
    }
  }
}

/**
 * Get the child node for the specified move.
 * Returns nullptr if the child node does not exist.
 * @param move move
 * @return pointer to the child node
 */
DfpnNode* DfpnNode::getChildNode(const Move& move) const {
  for (const auto& child_pair : _children) {
    if (child_pair.first == move) {
      return child_pair.second;
    }
  }

  return nullptr;
}

/**
 * Get the node depth.
 * @return node depth
 */
int32_t DfpnNode::getDepth() const {
  return _depth;
}

/**
 * Get the PN value.
 * @return PN value
 */
int32_t DfpnNode::getPn() const {
  return _pn;
}

/**
 * Get the DN value.
 * @return DN value
 */
int32_t DfpnNode::getDn() const {
  return _dn;
}

/**
 * Get the number of moves until mate.
 * @return moves until mate
 */
int32_t DfpnNode::getStep() const {
  return _step;
}

/**
 * Get the node size.
 * @return node size
 */
int32_t DfpnNode::getSize() const {
  return _size;
}

/**
 * Get the node information as a string.
 * @return string with node information
 */
std::string DfpnNode::toString() const {
  std::stringstream ss;

  ss << "DFPN Node: depth=" << _depth << " pn=" << _pn << " dn=" << _dn
     << " step=" << _step << " size=" << _size << "\n";
  ss << _board.toString() << "\n";

  for (auto& child_pair : _children) {
    Move move = child_pair.first;
    DfpnNode* child = child_pair.second;

    ss << "  Child Move: src(" << move.getSrcX() << ", " << move.getSrcY() << ")"
       << " -> dst(" << move.getDstX() << ", " << move.getDstY() << "),"
       << " promote=" << move.isPromote()
       << " pn=" << child->getPn()
       << " dn=" << child->getDn()
       << " step=" << child->getStep()
       << " size=" << child->getSize()
       << "\n";
  }

  return ss.str();
}

/**
 * Return true if this node is less than the specified node.
 */
bool DfpnNode::operator<(const DfpnNode& other) const {
  if (_hashBoard != other._hashBoard) {
    return _hashBoard < other._hashBoard;
  } else if (_hashHand != other._hashHand) {
    return _hashHand < other._hashHand;
  } else if (_board.getColor() != other._board.getColor()) {
    return _board.getColor() < other._board.getColor();
  }

  for (int32_t y = 0; y < BOARD_SIZE; ++y) {
    for (int32_t x = 0; x < BOARD_SIZE; ++x) {
      int32_t piece1 = _board.getPiece(x, y);
      int32_t piece2 = other._board.getPiece(x, y);

      if (piece1 != piece2) {
        return piece1 < piece2;
      }
    }
  }

  for (int32_t c : {COLOR_BLACK, COLOR_WHITE}) {
    for (int32_t p = PIECE_HAND_BEGIN; p < PIECE_HAND_END; ++p) {
      int32_t num1 = _board.getHandPieceNum(c, p);
      int32_t num2 = other._board.getHandPieceNum(c, p);

      if (num1 != num2) {
        return num1 < num2;
      }
    }
  }

  return false;
}

}  // namespace deepshogi

#include "PnSearchNode.h"

#include <algorithm>
#include <array>
#include <sstream>

#include "PnSearchEngine.h"

namespace deepshogi {

// Maximum value stored in a PN search node.
static constexpr int32_t MAX_VALUE = 0xffffff;

/**
 * Construct a PN search node.
 * Initialize it as a terminal node representing no checkmate.
 */
PnSearchNode::PnSearchNode()
    : _children(),
      _legalMoves(),
      _depth(MAX_VALUE),
      _contPn(MAX_VALUE),
      _termPn(MAX_VALUE),
      _contDn(0),
      _termDn(0),
      _termDepth(0),
      _termResultDepth(0),
      _step(MAX_VALUE),
      _size(1) {
}

/**
 * Destroy the PN search node.
 */
PnSearchNode::~PnSearchNode() = default;

/**
 * Initialize this node as a leaf for the specified position.
 * @param board const Board* target position.
 * @param depth int32_t remaining search depth in moves.
 * @return void
 */
void PnSearchNode::initialize(const Board* board, int32_t depth) {
  // Set terminal non-mate values without storing a board.
  _children.clear();
  _depth = depth;
  _contPn = MAX_VALUE;
  _contDn = 0;
  _step = MAX_VALUE;
  _size = 1;

  // Reuse legal moves for evaluation and expansion instead of generating them twice.
  board->getLegalMoves(_legalMoves, false, _depth % 2 == 1);

  // Set ordinary PN/DN values without treating depth limits as non-mate results.
  if (_depth % 2 == 1) {
    // On the attacking side,
    // with checks available: PN=1, DN=legal move count, mate length=maximum.
    // Without checks: PN=maximum, DN=0, mate length=maximum.
    if (!_legalMoves.empty()) {
      _contPn = 1;
      _contDn = (int32_t)_legalMoves.size();
      _step = MAX_VALUE;
    } else {
      _contPn = MAX_VALUE;
      _contDn = 0;
      _step = MAX_VALUE;
    }
  } else {
    // On the defending side,
    // with replies: cont PN=aggregated reply count, DN=1, mate length=maximum.
    // Without replies: PN=0, DN=maximum, mate length=1.
    // Count drops to the same square as one move, regardless of piece type.
    if (!_legalMoves.empty()) {
      std::array<bool, BOARD_SIZE * BOARD_SIZE> unique_hand_moves{};
      int32_t hand_move_count = 0;
      int32_t board_move_count = 0;

      for (const Move& move : _legalMoves) {
        if (move.getSrc().getX() == BOARD_SIZE) {
          // Count each drop destination only once.
          bool& seen = unique_hand_moves[move.getDst().getIndex()];
          if (!seen) {
            seen = true;
            ++hand_move_count;
          }
        } else {
          board_move_count++;
        }
      }

      _contPn = board_move_count + hand_move_count;
      _contDn = 1;
      _step = MAX_VALUE;
    } else {
      _contPn = 0;
      _contDn = MAX_VALUE;
      _step = 1;
    }
  }

  // Share the ordinary evaluation; set term to non-mate only at depth zero with replies.
  _termPn = (_depth == 0 && _contPn != 0) ? MAX_VALUE : _contPn;
  _termDn = (_depth == 0 && _contPn != 0) ? 0 : _contDn;
  _termDepth = depth;
  _termResultDepth = depth;
}

/**
 * Copy child references and search values without cloning the children themselves.
 * @param node const PnSearchNode* source node.
 * @return void
 */
void PnSearchNode::copyFrom(const PnSearchNode* node) {
  // Keep vectors independent so child references can be changed safely along a path.
  _children = node->_children;
  _legalMoves = node->_legalMoves;
  _depth = node->_depth;
  _contPn = node->_contPn;
  _contDn = node->_contDn;
  _termPn = node->_termPn;
  _termDn = node->_termDn;
  _termDepth = node->_termDepth;
  _termResultDepth = node->_termResultDepth;
  _step = node->_step;
  _size = node->_size;
}

/**
 * Expand the node into children, retaining already expanded children.
 * @param engine PnSearchEngine* search engine.
 * @param board Board* working position, restored on return.
 * @return bool; false only when node capacity is exhausted.
 */
bool PnSearchNode::expand(PnSearchEngine* engine, Board* board) {
  // Retain connections without generating children for solved, depth-zero, or expanded nodes.
  if (_contPn == 0 || _contDn == 0 || _depth == 0 || !_children.empty()) {
    return true;
  }

  // Initialization stores checking moves for attackers and all legal moves for defenders.
  int32_t child_depth = _depth - 1;

  // Try each move on the working board and always undo it after obtaining a node.
  for (const Move& move : _legalMoves) {
    MoveResult result = board->play(move);

    // Obtain regular children at depth zero and detect checkmate during initialization.
    PnSearchNode* child_node = engine->_getNode(board, child_depth, false);

    board->undo(result);

    // nullptr signals capacity exhaustion; return false only in this case,
    // so the caller restores the root position and aborts the entire search.
    if (child_node == nullptr) {
      return false;
    }

    // Append the child node.
    _children.push_back(std::make_pair(move, child_node));
  }

  return true;
}

/**
 * Update this node's PN/DN values, preserving terminal values when there are no children.
 * @return void
 */
void PnSearchNode::update() {
  // Do not overwrite childless terminal evaluations with an empty aggregation.
  if (_children.empty()) {
    return;
  }

  // Compare using the old depth and defer member changes until aggregation finishes.
  int32_t child_depth = std::max(_depth, _termDepth) - 1;
  bool attacker = (_depth % 2 == 1);
  int32_t cont_pn = attacker ? MAX_VALUE : 0;
  int32_t cont_dn = attacker ? 0 : MAX_VALUE;
  int32_t term_pn = cont_pn;
  int32_t term_dn = cont_dn;
  int32_t term_depth = 0;
  bool has_unresolved = false;
  int32_t step = attacker ? MAX_VALUE : 1;
  int32_t size = 1;
  std::array<int32_t, BOARD_SIZE * BOARD_SIZE> cont_hand_pns{};
  std::array<int32_t, BOARD_SIZE * BOARD_SIZE> term_hand_pns{};

  for (const auto& [move, child] : _children) {
    auto [selected_pn, selected_dn] = child->_getValues(child_depth);

    // Derive the stored depth only from children whose cont values are unresolved.
    if (child->_contPn != 0 && child->_contDn != 0) {
      int32_t candidate_depth = child->_termDepth + 1;
      term_depth = has_unresolved ? std::min(term_depth, candidate_depth) : candidate_depth;
      has_unresolved = true;
    }

    if (attacker) {
      // For attackers, compute minimum PN and summed DN for both value systems.
      cont_pn = std::min(cont_pn, child->_contPn);
      term_pn = std::min(term_pn, selected_pn);
      cont_dn = std::min(cont_dn + child->_contDn, MAX_VALUE);
      term_dn = std::min(term_dn + selected_dn, MAX_VALUE);
      step = std::min(step, child->_step + 1);
    } else {
      // For defensive drops, aggregate maximum PN per destination separately in both systems.
      int32_t cont_delta = child->_contPn;
      int32_t term_delta = selected_pn;
      if (move.getSrc().getX() == BOARD_SIZE) {
        int32_t& cont_max = cont_hand_pns[move.getDst().getIndex()];
        int32_t& term_max = term_hand_pns[move.getDst().getIndex()];
        cont_delta = std::max(0, child->_contPn - cont_max);
        term_delta = std::max(0, selected_pn - term_max);
        cont_max = std::max(cont_max, child->_contPn);
        term_max = std::max(term_max, selected_pn);
      }
      cont_pn = std::min(cont_pn + cont_delta, MAX_VALUE);
      term_pn = std::min(term_pn + term_delta, MAX_VALUE);
      cont_dn = std::min(cont_dn, child->_contDn);
      term_dn = std::min(term_dn, selected_dn);
      step = std::max(step, child->_step + 1);
    }

    // Share subtree size between both systems and add it once per child reference.
    size = std::min(size + child->_size, MAX_VALUE);
  }

  // Update term PN/DN and the stored depth together.
  _contPn = cont_pn;
  _contDn = cont_dn;
  _termPn = term_pn;
  _termDn = term_dn;
  // Solved term values remain valid to this depth; unrelated shallow children must not shrink it.
  bool term_resolved = term_pn == 0 || term_dn == 0;
  bool cont_resolved = cont_pn == 0 || cont_dn == 0;
  _termDepth = term_depth;
  _termResultDepth = term_resolved && !cont_resolved
      ? std::max(term_depth, child_depth + 1) : term_depth;
  _step = step;
  _size = size;
}

/**
 * Select this child's PN/DN values for the requested depth.
 * @param depth int32_t requested remaining depth.
 * @return std::pair<int32_t, int32_t> PN/DN values.
 */
std::pair<int32_t, int32_t> PnSearchNode::_getValues(int32_t depth) const {
  // Prefer solved cont values; also use cont when the stored term depth is insufficient.
  int32_t valid_depth = (_termPn == 0 || _termDn == 0) ? _termResultDepth : _termDepth;
  if (_contPn == 0 || _contDn == 0 || depth > valid_depth) {
    return {_contPn, _contDn};
  }
  return {_termPn, _termDn};
}

/**
 * Get the next child to search.
 * Return nullptr as the child when this node is terminal.
 * Minimize PN for attackers and DN for defenders, breaking ties by subtree size.
 * @param depth int32_t current requested remaining depth.
 * @return Pair of the next move and child node.
 */
std::pair<Move, PnSearchNode*> PnSearchNode::getNextNode(int32_t depth) {
  // Raise shallow requests to the stored term depth and continue searching.
  _depth = std::max(depth, _termDepth);
  PnSearchNode* next_node = nullptr;
  Move next_move(MOVE_INVALID);
  if (_contPn == 0 || _contDn == 0 || _depth == 0) {
    return {next_move, next_node};
  }

  // Select values with the aggregation rules and exclude solved children.
  int32_t child_depth = std::max(_depth, _termDepth) - 1;
  int32_t min_value = MAX_VALUE;
  for (const auto& [move, child] : _children) {
    auto [pn, dn] = child->_getValues(child_depth);
    if (pn == 0 || dn == 0) {
      continue;
    }

    // Prefer smaller subtrees when PN/DN values tie.
    int32_t value = (_depth % 2 == 1) ? pn : dn;
    if (next_node == nullptr || value < min_value
        || (value == min_value && child->_size < next_node->_size)) {
      min_value = value;
      next_node = child;
      next_move = move;
    }
  }
  return {next_move, next_node};
}

/**
 * Get the move and child node on the checkmating line.
 * Return nullptr if no child lies on a checkmating line.
 * @return Pair of a checkmating move and its child node.
 */
std::pair<Move, PnSearchNode*> PnSearchNode::getCheckmateNode() {
  PnSearchNode* checkmate_node = nullptr;
  Move checkmate_move(MOVE_INVALID);

  if (_depth % 2 == 1) {
    // For attackers, choose the shortest-mate child with PN zero.
    int32_t min_step = MAX_VALUE;

    for (auto& child_pair : _children) {
      PnSearchNode* child = child_pair.second;

      if (child->_contPn == 0 && child->_step < min_step) {
        checkmate_node = child;
        checkmate_move = child_pair.first;
        min_step = child->_step;
      }
    }
  } else {
    // For defenders, choose the longest-mate child with PN zero.
    int32_t max_step = 0;

    for (auto& child_pair : _children) {
      PnSearchNode* child = child_pair.second;

      if (child->_contPn == 0 && child->_step > max_step) {
        checkmate_node = child;
        checkmate_move = child_pair.first;
        max_step = child->_step;
      }
    }
  }

  return std::make_pair(checkmate_move, checkmate_node);
}

/**
 * Replace the specified child with a new child node.
 * @param targetNode Child to replace.
 * @param newNode Replacement child.
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
 * Return the node information as a string.
 * @return String representation of the node information.
 */
std::string PnSearchNode::toString() const {
  std::stringstream ss;

  ss << "DFPN Node: depth=" << _depth << " contPn=" << _contPn << " contDn=" << _contDn
     << " termPn=" << _termPn << " termDn=" << _termDn << " termDepth=" << _termDepth
     << " step=" << _step << " size=" << _size << "\n";

  for (auto& child_pair : _children) {
    Move move = child_pair.first;
    PnSearchNode* child = child_pair.second;

    ss << "  Child Move: " << move
       << " contPn=" << child->_contPn
       << " contDn=" << child->_contDn
       << " termPn=" << child->_termPn
       << " termDn=" << child->_termDn
       << " termDepth=" << child->_termDepth
       << " step=" << child->_step
       << " size=" << child->_size
       << "\n";
  }

  return ss.str();
}

}  // namespace deepshogi

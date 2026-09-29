#include "PnSearchEngine.h"

#include <algorithm>
#include <array>
#include <limits>

#include "BoardHash.h"
#include "MoveResult.h"

namespace deepshogi {

// Shared terminal replacing defensive loop branches; its special depth does not determine the side.
static PnSearchNode LOOP_NODE;

/**
 * Construct a DF-PN search engine.
 * @param nodeSize Maximum number of search nodes.
 */
PnSearchEngine::PnSearchEngine(int32_t nodeSize)
    : _useGroups(true),
      _cloneCount(0),
      _progress(0),
      _nodes(nodeSize),
      _nodeSize(nodeSize),
      _nodeCount(0),
      _cacheNodes(static_cast<uint32_t>(nodeSize)) {
}

/**
 * Destroy the DF-PN search engine.
 */
PnSearchEngine::~PnSearchEngine() = default;

/**
 * Search for and return a checkmating move sequence.
 * Return an empty vector if no checkmate is found.
 * @param board Object holding the board position.
 * @param depth int32_t initial depth, rounded to odd; may increase during search.
 * @return Checkmating move sequence.
 */
std::vector<Move> PnSearchEngine::getCheckmateMoves(const Board* board, int32_t depth) {
  // Search depth must be odd
  depth = std::max((depth % 2 == 0) ? depth + 1 : depth, 1);

  // Invalidate previous results and initialize the search state.
  _cacheNodes.Clear();
  _nodeCount = 0;
  _cloneCount = 0;
  _progress = 0;

  // Create a working board to preserve the input position.
  Board search_board;
  search_board.copyFrom(board);

  // Identify the attacking and defending colors.
  const int8_t attack_color = search_board.getColor();
  const int8_t defense_color = OPPOSITE_COLOR(attack_color);

  // Register the root position in the node pool and cache.
  PnSearchNode* root = _getNode(&search_board, depth, false);

  // Abort if the root node cannot be allocated.
  if (root == nullptr) {
    return {};
  }

  // Set unlimited PN and DN thresholds for the root frame.
  const double infinity = std::numeric_limits<double>::infinity();

  // Maintain search frames and state for the current path.
  std::vector<SearchFrame> frames;
  std::vector<PnSearchNode*> parents;
  std::vector<BoardHash> path_hashes;
  std::vector<MoveResult> history;

  // Start DF-PN search at the root position.
  frames.push_back({root, depth, infinity, infinity, _progress});
  path_hashes.emplace_back(&search_board);

  while (!frames.empty()) {
    // Get the current search frame and node.
    SearchFrame& frame = frames.back();
    PnSearchNode* node = frame.node;

    // Resume at a requested depth no smaller than the stored term depth.
    frame.depth = std::max(frame.depth, node->_termDepth);
    node->_depth = frame.depth;

    // Check termination using PN and DN for the requested depth.
    auto [pn, dn] = node->_getValues(frame.depth);
    bool finished = pn == 0 || dn == 0 || frame.depth == 0;

    // Initialize selection and effective thresholds for unsearched nodes, too.
    SearchChoice choice;
    SearchLimits limits{frame.pnLimit, frame.dnLimit};

    if (!finished) {
      // Record whether this is the first expansion, then generate children.
      bool leaf = node->_children.empty();
      if (!node->expand(this, &search_board)) {
        break;
      }

      // Record progress when a new leaf is expanded.
      if (leaf) {
        ++_progress;
      }

      // Reaggregate the current node from the latest shared child values.
      node->update();

      // Select the next child and gather values for threshold calculation.
      choice = _selectDfpn(node);
      finished = choice.node == nullptr;

      // End the current frame when an effective threshold is reached.
      limits = _getLimits(frame, choice);
      finished = finished || double(choice.pn) >= limits.pn || double(choice.dn) >= limits.dn;
    }

    if (finished) {
      // Check whether this frame allocated or expanded any nodes.
      bool no_progress = frame.enteredProgress == _progress;
      frames.pop_back();

      // End the search when the root frame finishes.
      if (frames.empty()) {
        break;
      }

      // Undo the last move and restore the path to the parent frame.
      search_board.undo(history.back());
      history.pop_back();
      parents.pop_back();
      path_hashes.pop_back();

      // Record the child returning without progress and the consecutive return count.
      frames.back().retryChild = no_progress ? node : nullptr;
      frames.back().emptyReturns = no_progress ? frames.back().emptyReturns + 1 : 0;
      continue;
    }

    // Create the child frame using the parent's thresholds and aggregates.
    SearchFrame child_frame = _makeChildFrame(frame, choice, limits);

    // Play the selected move and append the parent and undo record to the path.
    parents.push_back(node);
    history.push_back(search_board.play(choice.move));

    // Determine whether the resulting position belongs to the attacker.
    bool attacker = search_board.getColor() == attack_color;
    BoardHash current_hash(&search_board);

    // Find an identical ancestor or one satisfying the hand-piece conditions.
    auto loop = std::find_if(
        path_hashes.begin(), path_hashes.end(), [&](const BoardHash& path_hash) {
          return attacker ? current_hash.isLesserThanOrEqual(path_hash, attack_color)
                          : path_hash.isLesserThanOrEqual(current_hash, defense_color);
        });

    if (loop != path_hashes.end()) {
      // Find the ancestor's index in the loop.
      size_t loop_index = static_cast<size_t>(loop - path_hashes.begin());

      // Clone the loop segment as needed and replace the target with the shared terminal.
      if (!_replaceLoop(
              &search_board, parents, history, path_hashes, loop_index, choice.node, attacker)) {
        break;
      }

      // Record loop replacement as search progress.
      _progress += 1;

      // Propagate the replacement toward the root.
      for (auto it = parents.rbegin(); it != parents.rend(); ++it) {
        (*it)->update();
      }

      // Undo the working board back to the root position.
      for (auto it = history.rbegin(); it != history.rend(); ++it) {
        search_board.undo(*it);
      }

      // Rebuild the path from the root after changing ancestor references.
      parents.clear();
      history.clear();
      path_hashes.clear();
      path_hashes.emplace_back(&search_board);
      frames.clear();
      frames.push_back({root, depth, infinity, infinity, _progress});

      continue;
    }

    // Without a loop, append the position and child frame to the search path.
    path_hashes.push_back(current_hash);
    frames.push_back(child_frame);
  }

  // Restore the working board on exit, including capacity exhaustion.
  for (auto it = history.rbegin(); it != history.rend(); ++it) {
    search_board.undo(*it);
  }

  // Do not return an incomplete sequence from an interrupted search as checkmate.
  if (root->getTermPn() != 0) {
    return {};
  }

  // Follow proven children to construct the checkmating move sequence.
  std::vector<Move> checkmate_moves;
  PnSearchNode* node = root;

  while (true) {
    // Get the next checkmating move and child node.
    auto [next_move, next_node] = node->getCheckmateNode();

    // Stop constructing the sequence when no checkmating child remains.
    if (next_node == nullptr) {
      break;
    }

    // Append the move and advance to the next child.
    checkmate_moves.push_back(next_move);
    node = next_node;
  }

  return checkmate_moves;
}

/**
 * Compute the frame's effective thresholds from the current aggregates.
 * @param frame const SearchFrame& search frame.
 * @param choice const SearchChoice& latest aggregates.
 * @return SearchLimits effective thresholds.
 */
PnSearchEngine::SearchLimits PnSearchEngine::_getLimits(
    const SearchFrame& frame, const SearchChoice& choice) const {
  SearchLimits limits{frame.pnLimit, frame.dnLimit};

  // After repeated empty returns from one child, continue until expansion or loop detection.
  if (frame.forceProgress && frame.enteredProgress == _progress) {
    limits.pn = std::max(limits.pn, double(choice.pn) + 1.0);
    limits.dn = std::max(limits.dn, double(choice.dn) + 1.0);
  }

  return limits;
}

/**
 * Create the selected child's frame from parent thresholds and sibling values.
 * @param frame const SearchFrame& parent frame.
 * @param choice const SearchChoice& selection result.
 * @param limits const SearchLimits& effective parent thresholds.
 * @return SearchFrame child frame.
 */
PnSearchEngine::SearchFrame PnSearchEngine::_makeChildFrame(
    const SearchFrame& frame, const SearchChoice& choice, const SearchLimits& limits) const {
  bool attacker = frame.node->_depth % 2 == 1;
  double pn_limit = limits.pn;
  double dn_limit = limits.dn;
  // Derive child PN/DN thresholds from the runner-up and unsaturated sibling contributions.
  if (attacker) {
    pn_limit = std::min(pn_limit, choice.secondPriority + 1.0);
    dn_limit -= double(choice.dn) - choice.childDn;
  } else {
    int32_t contribution = _useGroups ? choice.groupPn : choice.childPn;
    pn_limit -= double(choice.pn) - contribution;
    dn_limit = std::min(dn_limit, choice.secondPriority + 1.0);
  }
  bool force = (frame.forceProgress && frame.enteredProgress == _progress) ||
               frame.retryChild == choice.node ||
               (frame.emptyReturns > 0 && frame.emptyReturns >= frame.node->_children.size());
  return {choice.node, frame.node->_depth - 1, pn_limit, dn_limit,
          _progress, force};
}

/**
 * Get the preferred child, runner-up value, and unsaturated PN/DN aggregates.
 * @param node const PnSearchNode* node to aggregate.
 * @return SearchChoice selection result.
 */
PnSearchEngine::SearchChoice PnSearchEngine::_selectDfpn(const PnSearchNode* node) const {
  // Odd search depths represent the attacker; even depths represent the defender.
  bool attacker = node->_depth % 2 == 1;

  // Calculate the child search depth.
  int32_t child_depth = std::max(node->_depth, node->_termDepth) - 1;

  // Initialize the minimum PN/DN values.
  uint64_t minimum = std::numeric_limits<uint64_t>::max();
  double best = std::numeric_limits<double>::infinity();
  std::array<int32_t, BOARD_SIZE * BOARD_SIZE> hand_pns{};
  SearchChoice result;

  // Use values selected for the same requested depth for selection and unsaturated aggregation.
  for (const auto& [move, child] : node->_children) {
    auto [pn, dn] = child->_getValues(child_depth);

    if (attacker) {
      minimum = std::min(minimum, uint64_t(pn));
      result.dn += dn;
    } else {
      minimum = std::min(minimum, uint64_t(dn));
      if (move.getSrc().getX() == BOARD_SIZE) {
        int32_t& maximum = hand_pns[move.getDst().getIndex()];
        result.pn += std::max(0, pn - maximum);
        maximum = std::max(maximum, pn);
      } else {
        result.pn += pn;
      }
    }

    if (pn == 0 || dn == 0) {
      continue;
    }

    // Calculate priority.
    // Use PN for the attacker and DN for the defender.
    double priority = double(attacker ? pn : dn);

    // Include ties in the runner-up value and prefer smaller subtrees when PN/DN ties.
    if (priority < best ||
        (priority == best && result.node != nullptr && child->_size < result.node->_size)) {
      result.secondPriority = best;
      best = priority;
      result.node = child;
      result.move = move;
      result.childPn = pn;
      result.childDn = dn;
    } else {
      result.secondPriority = std::min(result.secondPriority, priority);
    }
  }

  if (attacker) {
    result.pn = minimum;
  } else {
    result.dn = minimum;
  }

  if (result.pn == 0 || result.dn == 0) {
    result.node = nullptr;
  }

  if (result.node != nullptr) {
    result.groupPn = result.move.getSrc().getX() == BOARD_SIZE
                         ? hand_pns[result.move.getDst().getIndex()]
                         : result.childPn;
  }

  return result;
}

/**
 * Obtain a search node.
 * Obtain a regular node even when the remaining depth is zero.
 * Return nullptr when the node capacity is exhausted.
 * @param board const Board* position to search.
 * @param depth int32_t remaining search depth in moves.
 * @param noCache bool; true skips both cache lookup and insertion.
 * @return PnSearchNode* search node, or nullptr if capacity is exhausted.
 */
PnSearchNode* PnSearchEngine::_getNode(
    const Board* board, int32_t depth, bool noCache) {
  // Existing cached nodes remain usable after capacity is exhausted.
  std::array<uint64_t, 6> key{};
  uint64_t hash = 0;
  size_t slot = 0;

  // Look up a slot when caching is enabled.
  if (!noCache) {
    // Compute the cache key and hash.
    key = BoardHash(board).getCacheKey();
    hash = PnSearchCache::Hash(key);
    slot = _cacheNodes.Find(key, hash);

    // Read the node index from the cache slot.
    int32_t node_index = _cacheNodes.GetNodeIndex(slot);

    if (node_index >= 0) {
      return &_nodes[node_index];
    }
  }

  // Signal capacity exhaustion regardless of the remaining depth.
  if (_nodeCount >= _nodeSize) {
    return nullptr;
  }

  // Initialize an unused node and cache it only for ordinary lookups.
  PnSearchNode* node = &_nodes[_nodeCount++];
  _cloneCount += noCache ? 1 : 0;
  ++_progress;

  node->initialize(board, depth);

  // Insert the slot, key, hash, and node index into the cache.
  if (!noCache) {
    _cacheNodes.Insert(slot, key, hash, static_cast<uint32_t>(_nodeCount - 1));
  }

  return node;
}

/**
 * Replace a defensive node reference on a detected loop with the shared terminal.
 * @param board Board* detected position; replacement position on success.
 * @param parents std::vector<PnSearchNode*>& ancestor list.
 * @param history std::vector<MoveResult>& move history.
 * @param pathHashes std::vector<BoardHash>& ancestor positions.
 * @param loopIndex size_t index of the loop ancestor.
 * @param node PnSearchNode* regular node where the loop was detected.
 * @param attacker bool indicating whether the current position is attacking.
 * @return bool; false on capacity exhaustion, without changing connections.
 */
bool PnSearchEngine::_replaceLoop(
    Board* board, std::vector<PnSearchNode*>& parents,
    std::vector<MoveResult>& history, std::vector<BoardHash>& pathHashes,
    size_t loopIndex, PnSearchNode* node, bool attacker) {
  // For an attacking position, replace the parent and clone one fewer node.
  size_t parent_count = parents.size() - (attacker ? 1 : 0);
  size_t clone_count = parent_count - loopIndex - 1;

  if (clone_count > static_cast<size_t>(_nodeSize - _nodeCount)) {
    return false;
  }

  // Align the board, history, and ancestors with the defensive position being replaced.
  PnSearchNode* target = node;

  if (attacker) {
    target = parents.back();
    board->undo(history.back());
    history.pop_back();
    parents.pop_back();
    pathHashes.pop_back();
  }

  // Clone through the target's parent while preserving original nodes and cache entries.
  if (!_detachLoop(board, parents, history, loopIndex)) {
    return false;
  }

  parents.back()->replaceChildNode(target, &LOOP_NODE);

  return true;
}

/**
 * Clone intermediate loop nodes outside the cache.
 * @param board Board* current position, preserved on return.
 * @param parents std::vector<PnSearchNode*>& ancestor list.
 * @param history const std::vector<MoveResult>& move history.
 * @param loopIndex size_t index of the loop ancestor.
 * @return bool; false on capacity exhaustion, without changing connections.
 */
bool PnSearchEngine::_detachLoop(
    Board* board, std::vector<PnSearchNode*>& parents,
    const std::vector<MoveResult>& history, size_t loopIndex) {
  // Check capacity for the entire segment before changing connections or the board.
  size_t clone_count = parents.size() - loopIndex - 1;

  if (clone_count > static_cast<size_t>(_nodeSize - _nodeCount)) {
    return false;
  }

  if (clone_count == 0) {
    return true;
  }

  // Rewind to the loop ancestor to reconstruct each position being cloned.
  for (size_t i = history.size(); i > loopIndex; --i) {
    board->undo(history[i - 1]);
  }

  // Prepare storage for cloned nodes.
  std::vector<PnSearchNode*> clones;

  clones.reserve(clone_count);

  // Process each node in the segment to clone.
  for (size_t i = loopIndex + 1; i < parents.size(); ++i) {
    // Advance the board to the position corresponding to the source node.
    board->play(history[i - 1].getMove());

    // Create the cloned node.
    PnSearchNode* clone = _getNode(board, parents[i]->getDepth(), true);

    // If node allocation fails, restore the current position and abort.
    if (clone == nullptr) {
      // Connections are unchanged, so restoring the current position safely aborts.
      for (size_t j = i; j < history.size(); ++j) {
        board->play(history[j].getMove());
      }

      return false;
    }

    // Copy the source node's contents into the clone.
    clone->copyFrom(parents[i]);

    // Append the cloned node to the array.
    clones.push_back(clone);
  }

  // Replay the last recorded move to restore the current position.
  board->play(history.back().getMove());

  // After all clones succeed, reconnect branches and replace ancestors used for reverse updates.
  for (size_t i = loopIndex + 1; i < parents.size(); ++i) {
    PnSearchNode* clone = clones[i - loopIndex - 1];

    parents[i - 1]->replaceChildNode(parents[i], clone);
    parents[i] = clone;
  }

  return true;
}

}  // namespace deepshogi

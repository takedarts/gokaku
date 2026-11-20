#include "PnSearchEngine.h"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

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
    : _searchState(SearchState::IDLE),
      _useGroups(true),
      _cloneCount(0),
      _progress(0),
      _nodes(std::max(nodeSize, 0)),
      _nodeSize(static_cast<int32_t>(_nodes.size())),
      _nodeCount(0),
      _cacheNodes(static_cast<uint32_t>(_nodeSize)) {
}

/**
 * Request an abort of a prepared or running search.
 * Do not wait for it to finish.
 */
void PnSearchEngine::abortSearch() {
  // Keep a request made after preparation until the start; do not carry one after completion
  SearchState state = _searchState.load(std::memory_order_acquire);

  while (state == SearchState::READY || state == SearchState::RUNNING) {
    if (_searchState.compare_exchange_weak(
            state, SearchState::ABORT_REQUESTED, std::memory_order_acq_rel)) {
      return;
    }
  }
}

/**
 * Return whether the previous search was aborted.
 * @return True if the search ended because of an abort.
 */
bool PnSearchEngine::isSearchAborted() const {
  return _searchState.load(std::memory_order_acquire) == SearchState::ABORTED;
}

/** Invalidate the previous result before lending and accept aborts before the start. */
void PnSearchEngine::_prepareSearch() {
  _searchState.store(SearchState::READY, std::memory_order_release);
}

/**
 * Return whether an abort has been requested.
 * @return True if an abort request has been received.
 */
bool PnSearchEngine::_isAbortRequested() const {
  return _searchState.load(std::memory_order_relaxed) == SearchState::ABORT_REQUESTED;
}

/**
 * Resolve a race between completion and abort and finalize the search result.
 * @return True if completion was finalized before the abort.
 */
bool PnSearchEngine::_finishSearch() {
  SearchState expected = SearchState::RUNNING;
  if (_searchState.compare_exchange_strong(
          expected, SearchState::COMPLETED, std::memory_order_acq_rel)) {
    return true;
  }

  // If the abort request was finalized first, record it so that the caller can retry
  _searchState.store(SearchState::ABORTED, std::memory_order_release);

  return false;
}

/**
 * Search for and return a checkmating move sequence.
 * Return an empty vector if no checkmate is found or the search is aborted.
 * @param board Object holding the board position.
 * @param depth Initial depth, rounded to odd; may increase during search.
 * @param isPreviousPosition Function that identifies positions before the search started
 *     (empty for the conventional search).
 * @return Checkmating move sequence.
 */
std::vector<Move> PnSearchEngine::getCheckmateMoves(
    const Board* board, int32_t depth,
    const std::function<bool(const BoardHash&)>& isPreviousPosition) {
  // Get the state before the start, including an abort received after the engine was lent
  SearchState state = _searchState.load(std::memory_order_acquire);

  // On a race with a state change, decide again whether to start with the latest state
  while (true) {
    // Reject concurrent searches on the same engine
    if (state == SearchState::RUNNING) {
      throw std::logic_error("PN search is already running.");
    }

    // Finalize an abort requested before the start and end with zero nodes used this time
    if (state == SearchState::ABORT_REQUESTED) {
      _nodeCount = 0;
      _cloneCount = 0;
      _searchState.store(SearchState::ABORTED, std::memory_order_release);
      return {};
    }

    // Move from the observed state to running; on failure retry with the updated state
    if (_searchState.compare_exchange_weak(
            state, SearchState::RUNNING, std::memory_order_acq_rel)) {
      break;
    }
  }

  try {
    // Run the DF-PN search on the caller's thread and get the checkmating sequence
    auto moves = _searchCheckmateMoves(board, depth, isPreviousPosition);

    // Resolve completion versus abort; discard the sequence if the abort came first
    return _finishSearch() ? std::move(moves) : std::vector<Move>{};
  } catch (...) {
    // Finalize the search state before rethrowing an exception to the caller
    _finishSearch();
    throw;
  }
}

/**
 * Initialize the search state and run the DF-PN search synchronously.
 * @param board Input position.
 * @param depth Initial search depth.
 * @param isPreviousPosition Function that identifies positions before the search started
 *     (empty for the conventional search).
 * @return Checkmating sequence, or an empty vector if not found, out of capacity, or aborted.
 */
std::vector<Move> PnSearchEngine::_searchCheckmateMoves(
    const Board* board, int32_t depth,
    const std::function<bool(const BoardHash&)>& isPreviousPosition) {
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

  while (!frames.empty() && !_isAbortRequested()) {
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
      if (!node->expand(this, &search_board, isPreviousPosition)) {
        break;
      }

      // Record progress when a new leaf is expanded.
      if (leaf) {
        _progress += 1;
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
      if (no_progress) {
        frames.back().emptyReturns += 1;
      } else {
        frames.back().emptyReturns = 0;
      }
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

    // Within the search path, detect loops by identical positions and piece superiority.
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
      for (auto it = parents.rbegin(); it != parents.rend(); it++) {
        (*it)->update();
      }

      // Undo the working board back to the root position.
      for (auto it = history.rbegin(); it != history.rend(); it++) {
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

  // Restore the working board on exit, including capacity exhaustion and aborts.
  for (auto it = history.rbegin(); it != history.rend(); it++) {
    search_board.undo(*it);
  }

  // Do not return an incomplete sequence from an interrupted search as checkmate.
  if (_isAbortRequested() || root->getTermPn() != 0) {
    return {};
  }

  // For MCTS, check every reply for repetitions on the path, even for proofs sharing positions
  if (isPreviousPosition) {
    path_hashes.clear();
    path_hashes.emplace_back(&search_board);
    if (!_validateMateProof(root, search_board, path_hashes, isPreviousPosition)) {
      return {};
    }
  }

  // Follow proven children to construct the checkmating move sequence.
  std::vector<Move> checkmate_moves;
  PnSearchNode* node = root;

  while (!_isAbortRequested()) {
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
 * Follow the proving child and all replies to check for branches that end by repetition.
 * @param node Node that proved checkmate.
 * @param board Corresponding position (the same position on return).
 * @param path Positions that have appeared so far in the proof.
 * @param isPreviousPosition Function that identifies positions before the search started.
 * @return True if checkmate holds before any repetition for all replies.
 */
bool PnSearchEngine::_validateMateProof(
    PnSearchNode* node, Board& board, std::vector<BoardHash>& path,
    const std::function<bool(const BoardHash&)>& isPreviousPosition) const {
  if (_isAbortRequested() || node->_contPn != 0) {
    return false;
  }

  // A finalized terminal with no legal reply is checkmate in that position
  if (node->_children.empty()) {
    return node->_depth % 2 == 0 && node->_legalMoves.empty();
  }

  // Compare identical positions on the actual path instead of relying on shared node values
  auto validate_child = [&](const Move& move, PnSearchNode* child) {
    MoveResult result = board.play(move);
    BoardHash hash(&board);

    bool repeats = isPreviousPosition(hash) ||
                   std::find(path.begin(), path.end(), hash) != path.end();

    path.push_back(hash);

    bool valid = (!repeats || child->_children.empty()) &&
                 _validateMateProof(child, board, path, isPreviousPosition);

    path.pop_back();
    board.undo(result);

    return valid;
  };

  // Validate the returned proof move for the attacker and every move for the defender
  if (node->_depth % 2 == 1) {
    auto [move, child] = node->getCheckmateNode();

    return child != nullptr && validate_child(move, child);
  }

  return std::all_of(node->_children.begin(), node->_children.end(), [&](const auto& child) {
    return validate_child(child.first, child.second);
  });
}

/**
 * Compute the frame's effective thresholds from the current aggregates.
 * @param frame Search frame.
 * @param choice Latest aggregates.
 * @return Effective frame thresholds.
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
 * @param frame Parent frame.
 * @param choice Selection result.
 * @param limits Effective parent thresholds.
 * @return Child search frame.
 */
PnSearchEngine::SearchFrame PnSearchEngine::_makeChildFrame(
    const SearchFrame& frame, const SearchChoice& choice, const SearchLimits& limits) const {
  // The parent is the attacker if its remaining depth is odd, otherwise the defender
  bool attacker = frame.node->_depth % 2 == 1;

  // Start from the parent's effective thresholds and compute those passed to the selected child
  double pn_limit = limits.pn;
  double dn_limit = limits.dn;

  if (attacker) {
    // PN is aggregated by minimum, so limit it by the parent limit and the runner-up PN plus one
    pn_limit = std::min(pn_limit, choice.secondPriority + 1.0);

    // DN is aggregated by sum, so subtract the other children's contributions from the limit
    dn_limit -= double(choice.dn) - choice.childDn;
  } else {
    // With correction enabled, drops contribute their group PN; otherwise the child's PN
    int32_t contribution = _useGroups ? choice.groupPn : choice.childPn;

    // Subtract the PN contributions other than the selected child or group from the limit
    pn_limit -= double(choice.pn) - contribution;

    // DN is aggregated by minimum, so limit it by the parent limit and the runner-up DN plus one
    dn_limit = std::min(dn_limit, choice.secondPriority + 1.0);
  }

  // Enable progress-forcing thresholds for the child when inheriting an unprogressed forced
  // search, revisiting the same child without progress, or after at least as many empty returns
  // as there are children
  bool force = (frame.forceProgress && frame.enteredProgress == _progress) ||
               frame.retryChild == choice.node ||
               (frame.emptyReturns > 0 && frame.emptyReturns >= frame.node->_children.size());

  // Decrease the remaining depth by one and return the child frame with the current progress
  return {choice.node, frame.node->_depth - 1, pn_limit, dn_limit,
          _progress, force};
}

/**
 * Get the preferred child, runner-up value, and unsaturated PN/DN aggregates.
 * @param node Node to aggregate.
 * @return Preferred child and aggregate search values.
 */
PnSearchEngine::SearchChoice PnSearchEngine::_selectDfpn(const PnSearchNode* node) const {
  // Odd search depths represent the attacker; even depths represent the defender.
  bool attacker = node->_depth % 2 == 1;

  // Calculate the child search depth.
  int32_t child_depth = std::max(node->_depth, node->_termDepth) - 1;

  // Initialize the minimum PN or DN aggregated over all children
  uint64_t minimum = std::numeric_limits<uint64_t>::max();

  // Initialize the best candidate priority and the maximum PN for each drop square
  double best = std::numeric_limits<double>::infinity();
  std::array<int32_t, BOARD_SIZE * BOARD_SIZE> hand_pns{};

  // Initialize the PN/DN aggregates and the best and runner-up selections
  SearchChoice result;

  // Use values selected for the same requested depth for selection and unsaturated aggregation.
  for (const auto& [move, child] : node->_children) {
    // Get the child's PN/DN from its cont or term values according to the requested depth
    auto [pn, dn] = child->_getValues(child_depth);

    if (attacker) {
      // For the attacker, take the minimum PN and the sum of DN without saturating the sum
      minimum = std::min(minimum, uint64_t(pn));
      result.dn += dn;
    } else {
      // For the defender, take the minimum DN and add PN per move and per drop group
      minimum = std::min(minimum, uint64_t(dn));

      // A move from off the board is a drop; merge PNs for the same square by their maximum
      if (move.getSrc().getX() == BOARD_SIZE) {
        int32_t& maximum = hand_pns[move.getDst().getIndex()];

        // Add only the increase of the maximum to avoid double-counting the same square
        result.pn += std::max(0, pn - maximum);
        maximum = std::max(maximum, pn);
      } else {
        // Add the PN of each move on the board as is
        result.pn += pn;
      }
    }

    // Include finalized children in the aggregates but exclude them from new search candidates
    if (pn == 0 || dn == 0) {
      continue;
    }

    // Calculate priority.
    // Use PN for the attacker and DN for the defender.
    double priority = double(attacker ? pn : dn);

    // Include ties in the runner-up value and prefer smaller subtrees when PN/DN ties.
    if (priority < best ||
        (priority == best && result.node != nullptr && child->_size < result.node->_size)) {
      // Move the previous best to the runner-up and save the selected child and its values
      result.secondPriority = best;
      best = priority;
      result.node = child;
      result.move = move;
      result.childPn = pn;
      result.childDn = dn;
    } else {
      // Update the runner-up minimum from the priority of an unselected child
      result.secondPriority = std::min(result.secondPriority, priority);
    }
  }

  // Set the minimum combined with the sum as PN for the attacker and DN for the defender
  if (attacker) {
    result.pn = minimum;
  } else {
    result.dn = minimum;
  }

  // Clear the search target when the aggregate is finalized as checkmate or no checkmate
  if (result.pn == 0 || result.dn == 0) {
    result.node = nullptr;
  }

  // Save the PN of the selected child or drop group for the defender's threshold calculation
  if (result.node != nullptr) {
    result.groupPn = result.move.getSrc().getX() == BOARD_SIZE
                         ? hand_pns[result.move.getDst().getIndex()]
                         : result.childPn;
  }

  // Return the unsaturated PN/DN and the selection needed to create the child frame
  return result;
}

/**
 * Obtain a search node.
 * Obtain a regular node even when the remaining depth is zero.
 * Return nullptr when the node capacity is exhausted or an abort is requested.
 * @param board Position to search.
 * @param depth Remaining search depth in moves.
 * @param noCache True to skip both cache lookup and insertion.
 * @param isPreviousPosition Function that identifies positions before the search started
 *     (empty for the conventional search).
 * @return Search node, or nullptr if capacity is exhausted or an abort is requested.
 */
PnSearchNode* PnSearchEngine::_getNode(
    const Board* board, int32_t depth, bool noCache,
    const std::function<bool(const BoardHash&)>& isPreviousPosition) {
  // Check before the cache lookup so that child generation and loop cloning can also abort
  if (_isAbortRequested()) {
    return nullptr;
  }

  // Do not use branches ending by repetition as grounds for checkmate; prefer existing mate
  if (isPreviousPosition && isPreviousPosition(BoardHash(board)) &&
      (depth % 2 != 0 || !board->getLegalMoves(false, false).empty())) {
    return &LOOP_NODE;
  }

  // History before the search is common to all paths, so nodes can be shared as usual
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
  PnSearchNode* node = &_nodes[_nodeCount];
  _nodeCount += 1;
  _cloneCount += noCache ? 1 : 0;
  _progress += 1;

  node->initialize(board, depth);

  // Insert the slot, key, hash, and node index into the cache.
  if (!noCache) {
    _cacheNodes.Insert(slot, key, hash, static_cast<uint32_t>(_nodeCount - 1));
  }

  return node;
}

/**
 * Replace a defensive node reference on a detected loop with the shared terminal.
 * @param board Detected position; replacement position on success.
 * @param parents Ancestor list.
 * @param history Move history.
 * @param pathHashes Ancestor positions.
 * @param loopIndex Index of the loop ancestor.
 * @param node Regular node where the loop was detected.
 * @param attacker True if the current position is attacking.
 * @return False on capacity exhaustion or an abort request, without changing connections.
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
 * @param board Current position, preserved on return.
 * @param parents Ancestor list.
 * @param history Move history.
 * @param loopIndex Index of the loop ancestor.
 * @return False on capacity exhaustion or an abort request, without changing connections.
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
  for (size_t i = loopIndex + 1; i < parents.size(); i++) {
    // Advance the board to the position corresponding to the source node.
    board->play(history[i - 1].getMove());

    // Create the cloned node.
    PnSearchNode* clone = _getNode(board, parents[i]->getDepth(), true);

    // If node allocation fails, restore the current position and abort.
    if (clone == nullptr) {
      // Connections are unchanged, so restoring the current position safely aborts.
      for (size_t j = i; j < history.size(); j++) {
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
  for (size_t i = loopIndex + 1; i < parents.size(); i++) {
    PnSearchNode* clone = clones[i - loopIndex - 1];

    parents[i - 1]->replaceChildNode(parents[i], clone);
    parents[i] = clone;
  }

  return true;
}

}  // namespace deepshogi

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "BoardHash.h"
// Check the internal state of the cache and path cloning directly in regressions.
#define private public
#include "PnSearchEngine.h"
#undef private

using namespace deepshogi;

/**
 * Return the number of PN search cache entries valid in the current generation.
 * @param cache Cache to inspect.
 * @return Number of valid entries.
 */
size_t GetCacheSize(const PnSearchCache& cache) {
  return static_cast<size_t>(std::count_if(
      cache._entries.begin(), cache._entries.end(),
      [&cache](const auto& entry) {
        return entry.generation == cache._generation;
      }));
}

/**
 * Return whether two PN search caches register the same positions and node indices.
 * @param cache Cache to inspect.
 * @param expected Expected cache.
 * @return True if the registered contents are the same.
 */
bool HasSameCacheEntries(
    const PnSearchCache& cache, const PnSearchCache& expected) {
  if (GetCacheSize(cache) != GetCacheSize(expected)) {
    return false;
  }
  for (const auto& entry : expected._entries) {
    if (entry.generation != expected._generation) {
      continue;
    }
    const auto& key = expected._keys[entry.nodeIndex];
    size_t slot = cache.Find(key, entry.hash);
    if (cache.GetNodeIndex(slot) != static_cast<int32_t>(entry.nodeIndex)) {
      return false;
    }
  }
  return true;
}

/**
 * Check the node's own values and child references match the source.
 * @param node Node to check.
 * @param expected Source node.
 */
void AssertSameNode(const PnSearchNode& node, const PnSearchNode& expected) {
  // Shared values of the child nodes themselves are not part of the parent's invariants.
  assert(node.getDepth() == expected.getDepth());
  assert(node.getContPn() == expected.getContPn());
  assert(node.getContDn() == expected.getContDn());
  assert(node.getTermPn() == expected.getTermPn());
  assert(node.getTermDn() == expected.getTermDn());
  assert(node.getTermDepth() == expected.getTermDepth());
  assert(node._termResultDepth == expected._termResultDepth);
  assert(node.getStep() == expected.getStep());
  assert(node.getSize() == expected.getSize());
  assert(node._children.size() == expected._children.size());
  for (size_t i = 0; i < node._children.size(); i++) {
    assert(node._children[i].first.getValue() == expected._children[i].first.getValue());
    assert(node._children[i].second == expected._children[i].second);
  }
}

/**
 * Check inferiority judgments by side to move, pieces in hand, and board placement.
 */
void TestBoardHash() {
  // Compare positions with the same placement that differ only in pieces in hand.
  Board fewer;
  Board more;
  Board other_turn;
  Board other_cells;
  fewer.initialize("4k4/9/9/9/9/9/9/9/4K4 b P2p 1", 0x7fff);
  more.initialize("4k4/9/9/9/9/9/9/9/4K4 b 2Pp 99", 0x7fff);
  other_turn.initialize("4k4/9/9/9/9/9/9/9/4K4 w P2p 1", 0x7fff);
  other_cells.initialize("3k5/9/9/9/9/9/9/9/4K4 b P2p 1", 0x7fff);
  BoardHash fewer_hash(&fewer);
  BoardHash more_hash(&more);
  assert(fewer_hash.isLesserThanOrEqual(fewer_hash, COLOR_BLACK));
  assert(!fewer_hash.isLesserThan(fewer_hash, COLOR_BLACK));
  assert(fewer_hash.isLesserThan(more_hash, COLOR_BLACK));
  assert(!more_hash.isLesserThanOrEqual(fewer_hash, COLOR_BLACK));
  assert(more_hash.isLesserThan(fewer_hash, COLOR_WHITE));
  assert(!fewer_hash.isLesserThanOrEqual(more_hash, COLOR_WHITE));
  assert(!fewer_hash.isLesserThanOrEqual(BoardHash(&other_turn), COLOR_BLACK));
  assert(!fewer_hash.isLesserThanOrEqual(BoardHash(&other_cells), COLOR_BLACK));

  // Attackers use current<=ancestor and defenders use ancestor<=current.
  assert(fewer_hash.isLesserThanOrEqual(more_hash, COLOR_BLACK));
  assert(more_hash.isLesserThanOrEqual(fewer_hash, COLOR_WHITE));

  // Positions that differ only in move number have the same key.
  Board later;
  later.initialize("4k4/9/9/9/9/9/9/9/4K4 b P2p 99", 0x7fff);
  BoardHash later_hash(&later);
  assert(!(fewer_hash < later_hash) && !(later_hash < fewer_hash));
}

/**
 * Check board restoration during expansion and depth decreasing by one per move.
 */
void TestExpansion() {
  // Prepare a position that can generate checks including captures, promotions, and drops.
  Board board;
  board.initialize("4k4/4p4/4R4/9/9/9/9/9/4K4 b G 1", 0x7fff);
  std::string before = board.toString();
  BoardHash before_hash(&board);
  PnSearchEngine engine(10000);
  PnSearchNode root;
  root.initialize(&board, 31);
  assert(root.expand(&engine, &board));
  assert(board.toString() == before);
  assert(!(BoardHash(&board) < before_hash) && !(before_hash < BoardHash(&board)));

  // Attacking children have 30 remaining depth and defending children have 29.
  auto [attack_move, defense] = root.getNextNode(root.getDepth());
  assert(defense != nullptr && defense->getDepth() == 30);
  MoveResult result = board.play(attack_move);
  std::string defense_before = board.toString();
  assert(defense->expand(&engine, &board));
  assert(board.toString() == defense_before);
  auto [defense_move, attack] = defense->getNextNode(30);
  assert(attack != nullptr && attack->getDepth() == 29);
  board.undo(result);
  assert(board.toString() == before);

  // Restore the board even when stopping midway after exhausting the node slots.
  PnSearchEngine limited(1);
  root.initialize(&board, 31);
  assert(!root.expand(&limited, &board));
  assert(board.toString() == before);

  // Reexpanding the same board and depth reuses the cached children.
  root.initialize(&board, 31);
  assert(root.expand(&engine, &board));
  auto [same_move, new_defense] = root.getNextNode(root.getDepth());
  assert(same_move.getValue() == attack_move.getValue());
  assert(new_defense != nullptr && new_defense == defense);
  assert(new_defense->getDepth() == 30);

  // Also restore the board on capacity exhaustion when getting a depth-zero child from depth one.
  root.initialize(&board, 1);
  PnSearchEngine empty(0);
  assert(!root.expand(&empty, &board));
  assert(board.toString() == before);

}

/**
 * Check cache keys, suppression of lookup and registration, and capacity checks.
 */
void TestCache() {
  Board board;
  board.initialize("4k4/9/9/9/9/9/9/9/4K4 b - 1", 0x7fff);
  PnSearchEngine engine(4);
  PnSearchNode* first = engine._getNode(&board, 29, false);
  assert(engine._getNode(&board, 29, false) == first);
  PnSearchNode* deeper = engine._getNode(&board, 31, false);
  assert(deeper == first && deeper->getDepth() == 29);
  deeper->getNextNode(31);
  assert(deeper->getDepth() == 31);

  // Lookups outside the cache neither replace existing entries nor register new ones.
  PnSearchNode* detached = engine._getNode(&board, 31, true);
  assert(detached != first);
  assert(engine._getNode(&board, 27, true) != nullptr);
  assert(GetCacheSize(engine._cacheNodes) == 1);
  assert(engine._getNode(&board, 31, true) != nullptr);
  assert(engine._getNode(&board, 31, false) == first);
  assert(engine._getNode(&board, 31, true) == nullptr);
  assert(engine._getNode(&board, 33, false) == first);
  assert(engine._getNode(&board, 0, true) == nullptr);

  // Reset the cache and the pool when a search starts.
  engine.getCheckmateMoves(&board, 1);
  assert(GetCacheSize(engine._cacheNodes) == 1);
  assert(engine._nodes[0].getDepth() == 1);
}

/**
 * Check mate judgment at depth zero, retention of finalized values, and cache capacity.
 */
void TestTerminal() {
  constexpr int32_t M = 0xffffff;
  // Prepare a position escaping from check and a position mated by a gold.
  for (bool mate : {false, true}) {
    Board board;
    board.initialize(mate ? "4k4/4G4/4K4/9/9/9/9/9 w - 1"
                          : "4k4/4R4/9/9/9/9/9/9/4K4 w - 1", 0x7fff);
    std::string before = board.toString();
    auto legal_moves = board.getLegalMoves(false, false);
    assert(legal_moves.empty() == mate);
    PnSearchEngine engine(2);
    PnSearchNode* node = engine._getNode(&board, 0, false);
    assert(node != nullptr && node == &engine._nodes[0]);
    assert(engine._nodeCount == 1 && GetCacheSize(engine._cacheNodes) == 1);
    assert(node->getDepth() == 0 && node->_children.empty());
    assert(node->getTermPn() == (mate ? 0 : M));
    assert(node->getTermDn() == (mate ? M : 0));
    assert(node->getStep() == (mate ? 1 : M));
    assert(node->getSize() == 1);

    // Keep leaf values and the board even when expansion and update are called directly.
    assert(node->expand(&engine, &board));
    node->update();
    assert(node->getDepth() == 0 && node->_children.empty());
    assert(node->getTermPn() == (mate ? 0 : M));
    assert(node->getTermDn() == (mate ? M : 0));
    assert(node->getStep() == (mate ? 1 : M));
    assert(node->getSize() == 1);
    assert(node->getNextNode(0).second == nullptr);
    assert(node->getCheckmateNode().second == nullptr);
    assert(board.toString() == before);
    assert(board.getLegalMoves(false, false).size() == legal_moves.size());
    assert(engine._nodeCount == 1);

    // Leaves outside the cache use separate slots, and the cache is usable after exhaustion.
    PnSearchNode* detached = engine._getNode(&board, 0, true);
    assert(detached != nullptr && detached != node);
    assert(detached->getDepth() == 0 && detached->getTermPn() == node->getTermPn());
    assert(engine._nodeCount == 2 && GetCacheSize(engine._cacheNodes) == 1);
    assert(engine._getNode(&board, 0, false) == node);
    assert(engine._getNode(&board, 0, true) == nullptr);
    Board other;
    other.initialize("3k5/9/9/9/9/9/9/9/4K4 w - 1", 0x7fff);
    assert(engine._getNode(&other, 0, false) == nullptr);

    // The same defensive position is unresolved with replies at a positive remaining depth.
    PnSearchNode positive;
    positive.initialize(&board, 2);
    assert(mate ? positive.getTermPn() == 0
                : positive.getTermPn() > 0 && positive.getTermDn() == 1);
  }
}

/**
 * Check the independence of intermediate loop nodes and no changes on capacity exhaustion.
 */
void TestDetachLoop() {
  for (int32_t capacity : {4, 7}) {
    // Build a four-move path where the kings move back and forth, returning to an ancestor.
    Board board;
    board.initialize("4k4/9/9/9/9/9/9/9/4K4 b - 1", 0x7fff);
    PnSearchEngine engine(capacity);
    std::vector<Move> moves = {
        Move(Position(4, 8), Position(3, 8), false),
        Move(Position(4, 0), Position(3, 0), false),
        Move(Position(3, 8), Position(4, 8), false),
        Move(Position(3, 0), Position(4, 0), false)};
    std::vector<PnSearchNode*> nodes;
    std::vector<MoveResult> history;
    nodes.push_back(engine._getNode(&board, 31, false));
    for (size_t i = 0; i < moves.size(); i++) {
      history.push_back(board.play(moves[i]));
      nodes.push_back(engine._getNode(&board, 31 - static_cast<int32_t>(i + 1), false));
      nodes[i]->_children.emplace_back(moves[i], nodes.back());
    }

    // Also prepare child references outside the path and check sharing is kept after cloning.
    nodes[1]->_children.emplace_back(Move(MOVE_INVALID), nodes.back());
    std::vector<PnSearchNode*> parents(nodes.begin(), nodes.end() - 1);
    std::string before = board.toString();
    bool detached = engine._detachLoop(&board, parents, history, 0);
    assert(board.toString() == before);
    assert(GetCacheSize(engine._cacheNodes) == 4);
    assert(nodes.back() == nodes.front());
    assert(detached == (capacity == 7));
    if (!detached) {
      assert(engine._nodeCount == 4);
      assert(parents[1] == nodes[1]);
      assert(nodes[0]->_children[0].second == nodes[1]);
      continue;
    }

    // Clone only intermediate nodes and keep attributes and references outside the path.
    assert(parents[0] == nodes[0]);
    for (size_t i = 1; i < parents.size(); i++) {
      assert(parents[i] != nodes[i]);
      assert(parents[i]->getDepth() == nodes[i]->getDepth());
      assert(parents[i]->getTermPn() == nodes[i]->getTermPn());
      assert(parents[i]->getTermDn() == nodes[i]->getTermDn());
      assert(parents[i]->getContPn() == nodes[i]->getContPn());
      assert(parents[i]->getContDn() == nodes[i]->getContDn());
      assert(parents[i]->getTermDepth() == nodes[i]->getTermDepth());
      assert(parents[i]->getStep() == nodes[i]->getStep());
      assert(parents[i]->getSize() == nodes[i]->getSize());
      assert(parents[i - 1]->_children[0].second == parents[i]);
    }
    assert(parents[1]->_children[1].second == nodes.back());
    PnSearchNode terminal;
    parents.back()->replaceChildNode(nodes.back(), &terminal);
    for (size_t i = 1; i + 1 < nodes.size(); i++) {
      assert(nodes[i]->_children[0].second == nodes[i + 1]);
    }
    assert(parents.back()->_children[0].second == &terminal);
  }
}

/**
 * Check loop replacement per side, path shortening, shared sources, and capacity boundaries.
 */
void TestReplaceLoop() {
  PnSearchNode* shared_loop = nullptr;
  // Prepare positions repeated after four attacking moves and after five defensive moves.
  for (size_t steps : {4, 5, 2}) {
    for (bool sufficient : {false, true}) {
      Board board;
      board.initialize("4k4/9/9/9/9/9/9/9/4K4 b - 1", 0x7fff);
      std::string root_board = board.toString();
      std::vector<Move> moves = {
          Move(Position(4, 8), Position(3, 8), false),
          Move(Position(4, 0), Position(3, 0), false),
          Move(Position(3, 8), Position(4, 8), false),
          Move(Position(3, 0), Position(4, 0), false),
          Move(Position(4, 8), Position(3, 8), false)};
      size_t target_index = (steps % 2 == 0) ? steps - 1 : steps;
      size_t loop_index = (steps == 5) ? 1 : 0;
      size_t clone_count = target_index - loop_index - 1;
      int32_t original_count = static_cast<int32_t>(std::min(steps + 1, size_t{4}));
      PnSearchEngine engine(original_count + (sufficient ? clone_count : 0));
      std::vector<PnSearchNode*> nodes;
      std::vector<MoveResult> history;
      std::vector<BoardHash> hashes;
      std::vector<std::string> boards;
      nodes.push_back(engine._getNode(&board, 31, false));
      for (size_t i = 0; i < steps; i++) {
        hashes.emplace_back(&board);
        boards.push_back(board.toString());
        history.push_back(board.play(moves[i]));
        nodes.push_back(engine._getNode(&board, 30 - static_cast<int32_t>(i), false));
        // The revisited ancestor already has the same branch, so do not add it again.
        if (nodes[i]->_children.empty()) {
          nodes[i]->_children.emplace_back(moves[i], nodes.back());
        }
      }
      boards.push_back(board.toString());
      // A two-move artificial path checks only the boundary where no cloning is needed.
      if (steps != 2) {
        BoardHash current(&board);
        assert(!(current < hashes[loop_index]) && !(hashes[loop_index] < current));
      }
      std::vector<PnSearchNode*> parents(nodes.begin(), nodes.end() - 1);
      std::vector<PnSearchNode> original_nodes(nodes.size());
      for (size_t i = 0; i < nodes.size(); i++) {
        original_nodes[i].copyFrom(nodes[i]);
      }
      auto cache = engine._cacheNodes;
      bool replaced = engine._replaceLoop(
          &board, parents, history, hashes, loop_index, nodes.back(), steps % 2 == 0);
      assert(replaced == (sufficient || clone_count == 0));
      assert(HasSameCacheEntries(engine._cacheNodes, cache));
      if (!replaced) {
        assert(board.toString() == boards.back());
        assert(history.size() == steps && hashes.size() == steps);
        assert(parents.size() == steps && engine._nodeCount == original_count);
        for (size_t i = 0; i < nodes.size(); i++) {
          AssertSameNode(*nodes[i], original_nodes[i]);
        }
      } else {
        // Update up to the parent being replaced, not the detached original node.
        assert(board.toString() == boards[target_index]);
        assert(history.size() == target_index && hashes.size() == target_index);
        assert(parents.size() == target_index);
        assert(engine._nodeCount
               == original_count + static_cast<int32_t>(clone_count));
        PnSearchNode* terminal = parents.back()->_children[0].second;
        if (shared_loop != nullptr) {
          assert(terminal == shared_loop);
        }
        shared_loop = terminal;
        std::string terminal_before = terminal->toString();
        assert(terminal->getDepth() == 0xffffff && terminal->getTermPn() == 0xffffff);
        assert(terminal->getTermDn() == 0 && terminal->getStep() == 0xffffff);
        assert(terminal->getSize() == 1 && terminal->_children.empty());
        assert(terminal->getContPn() == 0xffffff && terminal->getContDn() == 0);
        assert(terminal->getTermDepth() == 0);
        for (size_t i = loop_index + 1; i < target_index; i++) {
          assert(parents[i] != nodes[i]);
          assert(parents[i]->getDepth() == nodes[i]->getDepth());
        }
        for (auto it = parents.rbegin(); it != parents.rend(); it++) {
          (*it)->update();
        }
        assert(parents.back()->getTermDn() == 0);
        assert(terminal->toString() == terminal_before);
        for (size_t i = loop_index + 1; i < nodes.size(); i++) {
          if (std::find(nodes.begin(), nodes.begin() + loop_index + 1, nodes[i])
              == nodes.begin() + loop_index + 1) {
            AssertSameNode(*nodes[i], original_nodes[i]);
          }
        }
      }
      // Return to the root correctly with the remaining history on success or exhaustion.
      for (auto it = history.rbegin(); it != history.rend(); it++) {
        board.undo(*it);
      }
      assert(board.toString() == root_board);
    }
  }
}

/**
 * Check reexpanding the same depth-zero node and keeping expanded connections.
 */
void TestReopenTerminal() {
  Board board;
  board.initialize("4k4/4R4/9/9/9/9/9/9/4K4 w - 1", 0x7fff);
  std::string before = board.toString();
  PnSearchEngine engine(1000);
  PnSearchNode* node = engine._getNode(&board, 0, false);
  assert(node->getContPn() > 0 && node->getContDn() == 1);
  assert(node->getTermDn() == 0 && node->getTermDepth() == 0);

  // Keep initial values on cache lookups and change the depth on search requests.
  assert(engine._getNode(&board, 2, false) == node);
  assert(node->getDepth() == 0);
  assert(node->getNextNode(2).second == nullptr);
  assert(node->getDepth() == 2 && node->getTermDepth() == 0);
  assert(node->expand(&engine, &board));
  node->update();
  assert(!node->_children.empty() && node->getTermDepth() == 2);
  assert(board.toString() == before);

  // Replacing with a cloned child and expanding again changes neither references nor usage.
  PnSearchNode clone;
  clone.copyFrom(node->_children.front().second);
  node->replaceChildNode(node->_children.front().second, &clone);
  auto children = node->_children;
  int32_t count = engine._nodeCount;
  assert(node->expand(&engine, &board));
  assert(engine._nodeCount == count);
  for (size_t i = 0; i < children.size(); i++) {
    assert(node->_children[i].first.getValue() == children[i].first.getValue());
    assert(node->_children[i].second == children[i].second);
  }
  assert(board.toString() == before);
}

/**
 * Check value switching by depth, stored depths, reaggregation, and independent drop sums.
 */
void TestAggregation() {
  constexpr int32_t M = 0xffffff;
  PnSearchNode parent;
  PnSearchNode shallow;
  PnSearchNode resolved;
  Move move(Position(4, 8), Position(4, 7), false);

  // Do not aggregate a depth-zero non-mate for a requested depth of three.
  shallow._contPn = 2;
  shallow._contDn = 1;
  shallow._termPn = M;
  shallow._termDn = 0;
  shallow._termDepth = 0;
  parent._depth = 3;
  parent._termDepth = 3;
  parent._children.emplace_back(move, &shallow);
  parent.update();
  assert(parent.getTermPn() == 2 && parent.getTermDn() == 1);
  assert(parent.getTermDepth() == 1);
  parent.update();
  assert(parent.getTermPn() == 2 && parent.getTermDn() == 1);
  assert(parent.getNextNode(3).second == &shallow);

  // Use term at equal depths and exclude shallow cont-finalized children from depth sums.
  shallow._termDepth = 2;
  shallow._termResultDepth = 2;
  parent._children.emplace_back(move, &resolved);
  parent.update();
  assert(parent.getContDn() == 1 && parent.getTermDn() == 0);
  assert(parent.getTermDepth() == 3);
  assert(parent.getNextNode(3).second == nullptr);
  assert(parent.getNextNode(5).second == &shallow);

  // Prefer finalized cont values and reset the depth to zero when all children are final.
  shallow._contPn = M;
  shallow._contDn = 0;
  shallow._termPn = 1;
  shallow._termDn = 1;
  parent.update();
  assert(parent.getContDn() == 0 && parent.getTermDn() == 0);
  assert(parent.getTermDepth() == 0);
  assert(parent.getNextNode(5).second == nullptr);

  // A mated cont value also takes precedence over an older term value.
  shallow._contPn = 0;
  shallow._contDn = M;
  shallow._step = 1;
  parent.update();
  assert(parent.getContPn() == 0 && parent.getTermPn() == 0);
  assert(parent.getCheckmateNode().second == &shallow);

  // On the same defensive drop square, different children give the maximum in each system.
  PnSearchNode other;
  shallow._contPn = 5;
  shallow._contDn = 2;
  shallow._termPn = 6;
  shallow._termDn = 2;
  shallow._termDepth = 1;
  other._contPn = 3;
  other._contDn = 4;
  other._termPn = 9;
  other._termDn = 4;
  other._termDepth = 1;
  parent._depth = 2;
  parent._termDepth = 0;
  parent._children.clear();
  parent._children.emplace_back(Move(Position(BOARD_SIZE, 0), Position(4, 4), false),
                                &shallow);
  parent._children.emplace_back(Move(Position(BOARD_SIZE, 1), Position(4, 4), false),
                                &other);
  parent.update();
  assert(parent.getContPn() == 5 && parent.getTermPn() == 9);
  assert(parent.getContDn() == 2 && parent.getTermDn() == 2);
  assert(parent.getTermDepth() == 2);
  assert(parent.getSize() == 3);

  // Reflect stored term depths larger than the requested depth in the comparison.
  parent._termDepth = 4;
  parent.update();
  assert(parent.getTermPn() == 5);

  // Do not overwrite the values of an unexpanded leaf with an empty child set.
  std::string before = other.toString();
  other.update();
  assert(other.toString() == before);
}

/**
 * Check shallow rerequests rise to the term depth and children receive the corrected depth.
 */
void TestDepthClamp() {
  Board board;
  board.initialize("4k4/4R4/3G5/9/9/9/9/9/4K4 w - 1", 0x7fff);
  PnSearchEngine engine(1000);
  PnSearchNode* node = engine._getNode(&board, 2, false);

  // Reproduce the stalled old case requesting zero moves from an unexpanded depth-two node.
  assert(engine._getNode(&board, 0, false) == node);
  assert(node->getNextNode(0).second == nullptr);
  assert(node->getDepth() == 2 && node->getTermDepth() == 2);
  assert(node->expand(&engine, &board));
  node->update();
  auto [move, child] = node->getNextNode(0);
  assert(child != nullptr && node->getDepth() == 2);

  // Do not pass negative depths to children; use the corrected parent depth minus one.
  MoveResult result = board.play(move);
  child->getNextNode(node->getDepth() - 1);
  assert(child->getDepth() == 1);
  assert(child->expand(&engine, &board));
  child->update();
  board.undo(result);

  // Use requests that need no raise and larger requests as is.
  node->getNextNode(2);
  assert(node->getDepth() == 2);
  node->getNextNode(4);
  assert(node->getDepth() == 4);
}

/**
 * Run the native PN search regressions.
 * @return Exit code (0 on success).
 */
int main() {
  TestBoardHash();
  TestExpansion();
  TestCache();
  TestTerminal();
  TestReopenTerminal();
  TestAggregation();
  TestDepthClamp();
  TestDetachLoop();
  TestReplaceLoop();
  std::cout << "Board=" << sizeof(Board) << ", PnSearchNode="
            << sizeof(PnSearchNode) << ", BoardHash=" << sizeof(BoardHash) << '\n';
  return 0;
}

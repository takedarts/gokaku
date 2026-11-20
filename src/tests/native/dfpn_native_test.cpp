#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

#include "BoardHash.h"
#include "PnSearchCache.h"
// Check threshold and depth boundaries directly with small artificial trees.
#define private public
#include "PnSearchEngine.h"
#undef private

using namespace deepshogi;

/**
 * Set an unresolved artificial node.
 * @param node Target node.
 * @param pn PN
 * @param dn DN
 * @param depth Evaluation depth.
 */
void SetNode(PnSearchNode& node, int32_t pn, int32_t dn, int32_t depth) {
  node._contPn = node._termPn = pn;
  node._contDn = node._termDn = dn;
  node._depth = node._termDepth = node._termResultDepth = depth;
  node._size = 1;
}

/** Check child selection, ties, groups, and unsaturated sums. */
void TestSelection() {
  PnSearchEngine engine(0);
  PnSearchNode parent, first, second, third;
  SetNode(parent, 1, 1, 2);
  SetNode(first, 5, 1, 1);
  SetNode(second, 7, 1, 1);
  SetNode(third, 10, 2, 1);
  parent._children = {
      {Move(Position(9, 0), Position(4, 4), false), &first},
      {Move(Position(9, 1), Position(4, 4), false), &second},
      {Move(Position(3, 3), Position(4, 4), false), &third}};
  auto choice = engine._selectDfpn(&parent);
  assert(choice.pn == 17 && choice.dn == 1 && choice.groupPn == 7);
  assert(choice.node == &first && choice.secondPriority == 1);

  // Select the child with the smallest PN regardless of size.
  parent._depth = parent._termDepth = 3;
  SetNode(first, 1, 0xfffffe, 2);
  SetNode(second, 2, 0xfffffe, 2);
  first._size = 100;
  parent._children.resize(2);
  choice = engine._selectDfpn(&parent);
  assert(choice.node == &first && choice.dn == uint64_t(0xfffffe) * 2);
  // Compare sizes only when PNs are equal.
  second._contPn = second._termPn = 1;
  assert(engine._selectDfpn(&parent).node == &second);
  parent._children.resize(1);
  assert(std::isinf(engine._selectDfpn(&parent).secondPriority));
}

/** Check threshold arrival, group correction, and progress correction. */
void TestThresholds() {
  const double infinity = std::numeric_limits<double>::infinity();
  PnSearchEngine engine(0);
  PnSearchNode parent, child;
  SetNode(parent, 17, 1, 2);
  SetNode(child, 5, 1, 1);
  PnSearchEngine::SearchFrame frame{&parent, 2, 20, 10, 0};
  PnSearchEngine::SearchChoice choice;
  choice.node = &child;
  choice.pn = 17;
  choice.dn = choice.childDn = 1;
  choice.childPn = 5;
  choice.groupPn = 7;
  choice.secondPriority = 3;
  auto limits = engine._getLimits(frame, choice);
  engine._useGroups = false;
  auto next = engine._makeChildFrame(frame, choice, limits);
  assert(next.pnLimit == 8 && next.dnLimit == 4);
  assert(!next.forceProgress);
  engine._useGroups = true;
  assert(engine._makeChildFrame(frame, choice, limits).pnLimit == 10);

  // Enable progress correction after a finite number of reselections even for empty returns
  parent._children = {{Move(0), &child}, {Move(1), &child}};
  frame.emptyReturns = 1;
  assert(!engine._makeChildFrame(frame, choice, limits).forceProgress);
  frame.emptyReturns = 2;
  assert(engine._makeChildFrame(frame, choice, limits).forceProgress);
  frame.emptyReturns = 0;
  frame.retryChild = &child;
  assert(engine._makeChildFrame(frame, choice, limits).forceProgress);
  frame.retryChild = nullptr;

  // Return when a value equals the threshold, and continue while it is below.
  frame.pnLimit = 17;
  assert(choice.pn >= engine._getLimits(frame, choice).pn);
  frame.pnLimit = 18;
  assert(choice.pn < engine._getLimits(frame, choice).pn);
  // Clear progress correction once the search progresses.
  frame.pnLimit = 17;
  frame.forceProgress = true;
  assert(engine._getLimits(frame, choice).pn == 18);
  engine._progress += 1;
  assert(engine._getLimits(frame, choice).pn == 17);

  // OR nodes limit by the runner-up PN and subtract the DNs of the other children.
  parent._depth = 3;
  frame.pnLimit = 20;
  choice.dn = 8;
  choice.childDn = 3;
  limits = engine._getLimits(frame, choice);
  next = engine._makeChildFrame(frame, choice, limits);
  assert(next.pnLimit == 4 && next.dnLimit == 5);
  choice.secondPriority = infinity;
  assert(engine._makeChildFrame(frame, choice, limits).pnLimit == 20);

}

/** Store the valid depth of finalized results separately from the search depth. */
void TestResolvedDepth() {
  PnSearchNode parent, terminal, shallow;
  SetNode(parent, 1, 1, 4);
  SetNode(terminal, 1, 1, 3);
  terminal._termPn = 0xffffff;
  terminal._termDn = 0;
  SetNode(shallow, 1, 1, 1);
  parent._children = {{Move(0), &terminal}, {Move(1), &shallow}};
  parent.update();
  assert(parent._termDepth == 2 && parent._termResultDepth == 4);
  assert(parent._getValues(4).second == 0 && parent._getValues(6).second == 1);
  PnSearchNode clone;
  clone.copyFrom(&parent);
  assert(clone._getValues(4).second == 0);
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
    // Cloning does not replace the original nodes in the cache.
    Board cached_board;
    cached_board.initialize("4k4/9/9/9/9/9/9/9/4K4 b - 1", 0x7fff);
    for (size_t i = 0; i < moves.size(); i++) {
      assert(engine._getNode(&cached_board, 31, false) == nodes[i]);
      cached_board.play(moves[i]);
    }
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
      assert(parents[i]->_termResultDepth == nodes[i]->_termResultDepth);
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

/** Check both drop-square ends and independent maximum aggregation for cont and term. */
void TestHandAggregation() {
  PnSearchNode parent, first, second, third;
  SetNode(parent, 1, 1, 2);
  SetNode(first, 5, 1, 1);
  SetNode(second, 7, 1, 1);
  SetNode(third, 3, 1, 1);
  second._termPn = 11;
  parent._children = {
      {Move(Position(9, 0), Position(8, 8), false), &first},
      {Move(Position(9, 1), Position(8, 8), false), &second},
      {Move(Position(9, 0), Position(0, 0), false), &third}};
  parent.update();
  assert(parent.getContPn() == 10 && parent.getTermPn() == 14);

  // Do not keep the previous group maximum when aggregating again.
  SetNode(second, 2, 1, 1);
  parent.update();
  assert(parent.getContPn() == 8 && parent.getTermPn() == 8);
}

/** Run the DF-PN artificial tree regressions. */
int main() {
  TestHandAggregation();
  TestSelection();
  TestThresholds();
  TestResolvedDepth();
  TestDetachLoop();
  std::cout << "DF-PN thresholds and depth: OK\n";
  return 0;
}

#include <cassert>
#include <iostream>
#include <set>

#include "BoardHash.h"
// Access child nodes in the test to check expansion results.
#define private public
#include "PnSearchEngine.h"
#undef private

using namespace deepshogi;

/** Check reusing an output vector does not leave stale legal moves. */
void TestOutputReuse() {
  Board board;
  board.initialize("4k4/9/9/9/9/9/9/9/4K4 b - 1", 0x7fff);
  std::vector<Move> moves;
  board.getLegalMoves(moves, false, false);
  assert(!moves.empty());
  const auto expected = moves;
  const auto* storage = moves.data();
  const auto capacity = moves.capacity();

  // Restricting to a position without checks empties the contents and keeps the capacity.
  board.getLegalMoves(moves, false, true);
  assert(moves.empty() && moves.capacity() == capacity && moves.data() == storage);
  board.getLegalMoves(moves, false, false);
  assert(moves == expected && moves.data() == storage);
}

/**
 * Check a clone expands with the original legal moves even after the source is reinitialized.
 */
void TestCopiedUnexpandedNode() {
  // Clone both a depth-zero defensive leaf and an unexpanded attacking node.
  for (int32_t depth : {0, 3}) {
    Board board;
    board.initialize(depth == 0 ? "4k4/4R4/9/9/9/9/9/9/4K4 w - 1"
                                : "4k4/4p4/4R4/9/9/9/9/9/4K4 b G 1", 0x7fff);
    auto expected = board.getLegalMoves(false, depth % 2 == 1);
    assert(!expected.empty());
    PnSearchNode original;
    original.initialize(&board, depth);
    PnSearchNode clone;
    clone.copyFrom(&original);

    // Reinitialize the source to another position, simulating pool reuse.
    Board other;
    other.initialize("4k4/9/9/9/9/9/9/9/4K4 b - 1", 0x7fff);
    original.initialize(&other, 3);
    PnSearchEngine engine(10000);
    clone.getNextNode(depth == 0 ? 2 : depth);
    assert(clone.expand(&engine, &board));

    // The original legal moves and generation order match even after raising the depth.
    assert(clone._children.size() == expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
      assert(clone._children[i].first.getValue() == expected[i].getValue());
    }
  }
}

/**
 * Suppress mate confirmation in short and PN searches when a reply ends by repetition.
 */
void TestRepetitionBlocksMate() {
  Board board;
  const std::string sfen =
      "9/2n1g4/k1B2s2+P/l1p6/gp1NPPP2/1PPn3P1/+p1N2R2L/p2K5/L8 b RB2SL7P2gs 273";
  board.initialize(sfen, 512);
  PnSearchEngine engine(1000);
  assert(board.getCheckmateMoves(3).size() == 3);
  assert(engine.getCheckmateMoves(&board, 3).size() == 3);

  // Pass the positions after each reply to the check as earlier positions.
  std::set<BoardHash> appeared;
  for (const Move& check : board.getLegalMoves(false, true)) {
    auto check_result = board.play(check);
    for (const Move& reply : board.getLegalMoves(false, false)) {
      auto reply_result = board.play(reply);
      appeared.emplace(&board);
      board.undo(reply_result);
    }
    board.undo(check_result);
  }
  auto repeats = [&](const BoardHash& hash) { return appeared.contains(hash); };
  assert(board.getCheckmateMoves(3, repeats).empty());
  assert(engine.getCheckmateMoves(&board, 3, repeats).empty());
  assert(board.getSfen() == sfen);
  assert(engine.getCheckmateMoves(&board, 3).size() == 3);
}

/**
 * Run the legal move reuse regressions.
 * @return Exit code (0 on success).
 */
int main() {
  TestOutputReuse();
  TestCopiedUnexpandedNode();
  TestRepetitionBlocksMate();
  std::cout << "Cached legal moves: OK\n";
  return 0;
}

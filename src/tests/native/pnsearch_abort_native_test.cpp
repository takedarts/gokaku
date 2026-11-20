#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <new>
#include <thread>
#include <vector>

#include "Board.h"
#include "BoardHash.h"
#include "PnSearchCache.h"
// Check the interrupted processing positions and board restoration using the search state.
#define private public
#include "PnSearchNode.h"
#include "PnSearchEngine.h"
#undef private
#include "PnSearchManager.h"

using namespace deepshogi;
using namespace std::chrono_literals;

// Inject an abort request into the same search thread at the specified allocation.
thread_local PnSearchEngine* abort_on_allocation = nullptr;
thread_local int32_t allocations_until_abort = 0;

/**
 * Allocate test memory and request an abort at the specified allocation.
 * @param size Required byte count.
 * @return Allocated memory.
 */
void* operator new(std::size_t size) {
  if (abort_on_allocation != nullptr) {
    if (allocations_until_abort == 0) {
      PnSearchEngine* engine = abort_on_allocation;
      abort_on_allocation = nullptr;
      engine->abortSearch();
    } else {
      allocations_until_abort -= 1;
    }
  }
  void* pointer = std::malloc(size == 0 ? 1 : size);
  if (pointer == nullptr) {
    throw std::bad_alloc();
  }
  return pointer;
}

/**
 * Release memory allocated for the test.
 * @param pointer Memory to release.
 */
void operator delete(void* pointer) noexcept {
  std::free(pointer);
}

// An 11-ply mate that needs about 190,000 nodes in existing comparisons.
constexpr char HARD_SFEN[] =
    "l3+B4/1lk6/2ngsP3/2p1pp3/p6P1/1PP1P4/P1N2g3/1K5+p1/L3N3+l w RBG2SN4Prgs3p 114";
constexpr char EASY_SFEN[] = "4k4/9/4K4/9/9/9/9/9 b G 1";

/** Check aborts before the start, return and re-lending, and direct-call compatibility. */
static void TestPreparationAndReuse() {
  Board board;
  board.initialize(EASY_SFEN, 512);
  PnSearchManager manager(1000, 2);
  PnSearchEngine* first = manager.acquire();
  PnSearchEngine* second = manager.acquire();
  manager.abortSearch();
  manager.abortSearch();
  for (PnSearchEngine* engine : {first, second}) {
    assert(engine->getCheckmateMoves(&board, 7).empty());
    assert(engine->isSearchAborted());
    assert(engine->getNodeCount() == 0);
    manager.release(engine);
  }

  // Do not carry an abort after return to the next lending, nor change a completed result.
  manager.abortSearch();
  PnSearchEngine* engine = manager.acquire();
  assert(!engine->isSearchAborted());
  assert(engine->getCheckmateMoves(&board, 7).size() == 1);
  engine->abortSearch();
  assert(!engine->isSearchAborted());
  assert(engine->getCheckmateMoves(&board, 7).size() == 1);
  manager.release(engine);

  // Do not treat capacity exhaustion or no checkmate as an abort.
  PnSearchEngine empty(0);
  empty.abortSearch();
  assert(empty.getCheckmateMoves(&board, 7).empty());
  assert(!empty.isSearchAborted());
  board.initialize("4k4/9/9/9/9/9/9/9/4K4 b - 1", 512);
  assert(engine == first || engine == second);
  PnSearchEngine direct(1000);
  assert(direct.getCheckmateMoves(&board, 7).empty());
  assert(!direct.isSearchAborted());
}

/** Check aborting a running search, input invariance, and reuse of the same engine. */
static void TestRunningAbort() {
  Board board;
  Board easy;
  board.initialize(HARD_SFEN, 512);
  easy.initialize(EASY_SFEN, 512);
  const std::string before = board.toString();
  PnSearchEngine engine(200000);
  for (int32_t i = 0; i < 5; i++) {
    auto result = std::async(std::launch::async, [&]() {
      return engine.getCheckmateMoves(&board, 31);
    });
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (engine._searchState.load() != PnSearchEngine::SearchState::RUNNING &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    assert(engine._searchState.load() == PnSearchEngine::SearchState::RUNNING);
    std::this_thread::sleep_for(2ms);
    const auto stopped = std::chrono::steady_clock::now();
    engine.abortSearch();
    assert(result.wait_for(2s) == std::future_status::ready);
    const auto moves = result.get();
    const double latency = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - stopped).count();
    assert(moves.empty() && engine.isSearchAborted());
    assert(engine.getNodeCount() > 1);
    assert(board.toString() == before);
    std::cout << "Abort latency us: " << latency << '\n';
    assert(engine.getCheckmateMoves(&easy, 7).size() == 1);
    assert(!engine.isSearchAborted());
  }
}

/** Check the final state and returned sequence agree when mate completion races an abort. */
static void TestCompletionRace() {
  Board board;
  board.initialize(EASY_SFEN, 512);
  PnSearchManager manager(1000, 1);
  for (int32_t i = 0; i < 100; i++) {
    PnSearchEngine* engine = manager.acquire();
    auto result = std::async(std::launch::async, [&]() {
      return engine->getCheckmateMoves(&board, 7);
    });
    if (i % 2 == 0) {
      std::this_thread::yield();
    }
    manager.abortSearch();
    assert(result.wait_for(2s) == std::future_status::ready);
    const auto moves = result.get();
    const bool aborted = engine->isSearchAborted();
    assert(moves.empty() == aborted);
    assert(aborted || moves.size() == 1);
    manager.abortSearch();
    assert(engine->isSearchAborted() == aborted);
    manager.release(engine);
  }
}

/** Aborting during child generation does not aggregate partial children and restores the board. */
static void TestInterruptedExpansion() {
  Board board;
  board.initialize(HARD_SFEN, 512);
  PnSearchEngine engine(1000);
  PnSearchNode* root = engine._getNode(&board, 31, false);
  const std::string before = board.toString();
  const int32_t pn = root->getTermPn();
  engine._prepareSearch();
  abort_on_allocation = &engine;
  allocations_until_abort = 0;
  assert(!root->expand(&engine, &board));
  assert(abort_on_allocation == nullptr && engine._isAbortRequested());
  assert(!root->_children.empty() && root->_children.size() < root->_legalMoves.size());
  assert(root->getTermPn() == pn && board.toString() == before);
}

/** Aborting during loop cloning does not change connections and restores the board. */
static void TestInterruptedLoopClone() {
  Board board;
  board.initialize("4k4/9/9/9/9/9/9/9/4K4 b - 1", 512);
  PnSearchEngine engine(16);
  const std::vector<Move> moves = {
      Move(Position(4, 8), Position(3, 8), false),
      Move(Position(4, 0), Position(3, 0), false),
      Move(Position(3, 8), Position(4, 8), false),
      Move(Position(3, 0), Position(4, 0), false)};
  std::vector<MoveResult> history;
  std::vector<PnSearchNode*> nodes{engine._getNode(&board, 31, false)};
  for (size_t i = 0; i < moves.size(); i++) {
    history.push_back(board.play(moves[i]));
    nodes.push_back(engine._getNode(&board, 30 - static_cast<int32_t>(i), false));
    nodes[i]->_children.emplace_back(moves[i], nodes.back());
  }
  std::vector<PnSearchNode*> parents(nodes.begin(), nodes.end() - 1);
  const auto original_parents = parents;
  const std::string before = board.toString();
  engine._prepareSearch();
  abort_on_allocation = &engine;
  allocations_until_abort = 1;
  assert(!engine._detachLoop(&board, parents, history, 0));
  assert(abort_on_allocation == nullptr && engine._isAbortRequested());
  assert(engine.getCloneCount() > 0 && parents == original_parents);
  assert(board.toString() == before);
  for (size_t i = 0; i + 1 < nodes.size(); i++) {
    assert(nodes[i]->_children.front().second == nodes[i + 1]);
  }
}

/**
 * Run the PN search abort and reuse regressions.
 * @return Exit code.
 */
int main() {
  TestPreparationAndReuse();
  TestRunningAbort();
  TestCompletionRace();
  TestInterruptedExpansion();
  TestInterruptedLoopClone();
  std::cout << "PN abort native tests passed." << std::endl;
  return 0;
}

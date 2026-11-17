#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <queue>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Board.h"
#include "BoardHash.h"
#include "Candidate.h"
#include "MctsParameter.h"
#include "MctsPolicy.h"
#include "MctsValue.h"
#include "Move.h"
#include "PnSearchEngine.h"

// Check failed initialization returns temporary nodes without increasing active nodes.
#define private public
#include "MctsManager.h"
#undef private

// Use node locks to control the update thread between checking termination and waiting.
#define private public
#include "MctsNode.h"
#undef private

#include "InferenceCache.h"
#include "InferenceModel.h"

// Inspect values normalized at construction without adding validation to inference.
#define private public
#include "InferenceExecutor.h"
#undef private
#include "InferenceResult.h"

// Control submission timing to test the gap between evaluation and callback completion.
#define private public
#include "InferenceProcessor.h"
#undef private
#include "PnSearchManager.h"
#include "ThreadPool.h"

// Control pre-visit mate detection and stop/restart ordering without starting workers.
#define private public
#include "Player.h"
#undef private

using namespace deepshogi;
using namespace std::chrono_literals;

// Fail the test thread's next allocation only while testing exception paths.
thread_local bool fail_next_allocation = false;

// Track allocation failures and live allocations only during PN pool construction.
thread_local bool track_allocations = false;
thread_local int32_t allocations_until_failure = -1;
thread_local int32_t tracked_live_allocations = 0;

/**
 * Allocate test memory and inject a submission exception only when requested.
 * @param size Required byte count.
 * @return Allocated memory.
 */
void* operator new(std::size_t size) {
  if (fail_next_allocation) {
    fail_next_allocation = false;
    throw std::bad_alloc();
  }
  if (track_allocations && allocations_until_failure == 0) {
    throw std::bad_alloc();
  }
  if (track_allocations && allocations_until_failure > 0) {
    --allocations_until_failure;
  }
  // Return valid storage even for a zero-byte request.
  void* pointer = std::malloc(size == 0 ? 1 : size);
  if (pointer == nullptr) {
    throw std::bad_alloc();
  }
  if (track_allocations) {
    ++tracked_live_allocations;
  }
  return pointer;
}

/**
 * Release memory returned by the test allocator.
 * @param pointer Memory to release.
 */
void operator delete(void* pointer) noexcept {
  if (pointer != nullptr && track_allocations) {
    --tracked_live_allocations;
  }
  std::free(pointer);
}

/**
 * Route array allocations through the same failure injection and live allocation tracking.
 * @param size Required byte count.
 * @return Allocated memory.
 */
void* operator new[](std::size_t size) {
  return ::operator new(size);
}

/**
 * Route array deallocation through the same live allocation tracking.
 * @param pointer Memory to release.
 */
void operator delete[](void* pointer) noexcept {
  ::operator delete(pointer);
}

// A 31-ply mate that short checkmate search cannot find.
constexpr char MATE_SFEN[] =
    "l6+Rl/1p2k1n2/r2ppb3/pPps1g2p/Pn5N1/1GPS4P/3PP+l+p2/"
    "bK1G1+p3/+n1+s5L w GSP3p 112";

/**
 * Check a result wait finishes and explicitly stop it for cleanup on failure.
 * @param player Player to stop.
 * @param result Wait result.
 */
static void AssertReleased(Player& player, std::future<void>& result) {
  bool released = result.wait_for(2s) == std::future_status::ready;
  if (!released) {
    player.stopEvaluation();
  }
  result.get();
  assert(released);
}

/**
 * Check mate detection releases a zero-visit wait and preserves the mating line.
 * @param processor Inference processor.
 */
static void TestMateBeforeVisit(InferenceProcessor* processor) {
  Player player(processor, 1, 1000000, 28, 27, 0.0f, 31, 100000, 1,
                1.6f, 3200.0f, 0.0f);
  player.initialize(MATE_SFEN, 512);

  // Start a first-visit wait and confirm mate using PN search without starting MCTS.
  auto result = std::async(std::launch::async, [&player]() {
    player.waitEvaluation(1000, 60.0f, true);
  });
  assert(result.wait_for(20ms) == std::future_status::timeout);
  player._runCheckmateSearch(player._root);
  AssertReleased(player, result);
  assert(player.getVisits() == 0);

  // Check candidate visits are nonnegative and a mating line is returned.
  auto candidates = player.getCandidates();
  assert(candidates.size() == 1);
  assert(candidates.front().getVisits() == 0);
  auto checkmate_moves = player._root->getCheckmateMoves();
  assert(checkmate_moves.size() == 31);

  // Do not overwrite a long mate with short search when first-visit handling runs later.
  assert(player._root->pickupNextNode(false, 0, 1.0f, 0.0f, []() { return false; }) ==
         player._root);
  assert(player._root->getCheckmateMoves() == checkmate_moves);
  assert(player._root->isEvaluated());
}

/**
 * Check root and child nodes accept only PN mates within the draw move limit.
 */
static void TestMateDrawTurnLimits() {
  // Standalone PN search ignores the draw limit and returns a line beyond its initial depth of 29.
  Board board;
  board.initialize(MATE_SFEN, 139);
  const int32_t turn = board.getTurn();
  PnSearchEngine baseline_engine(100000);
  const auto expected_moves = baseline_engine.getCheckmateMoves(&board, 29);
  const int32_t mate_length = static_cast<int32_t>(expected_moves.size());
  assert(mate_length > 29);

  for (bool child_node : {false, true}) {
    for (int32_t remaining : {-1, 0, 28, 29, mate_length - 1, mate_length, mate_length + 1}) {
      MctsManager manager(MctsParameter(28, 27, 0.0f, 1.6f, 3200.0f, 0.0f));
      MctsNode* root = manager.createNode();
      MctsNode* node = root;

      // Expand a child from the actual previous position, preserving turn and parent linkage.
      if (child_node) {
        root->initialize(
            "l7l/1p2k1n2/r2ppb1+R1/pPps1g2p/Pn5N1/1GPS4P/3PP+l+p2/"
            "bK1G1+p3/+n1+s5L b GSP3p 111", turn + remaining);
        Move move(Position(1, 2), Position(1, 0), false);
        root->applyInferenceResult(0.0f, 40.0f, {{move, 1.0f}});
        node = root->pickupNextNode(false, 0, 1.0f, 0.0f, []() { return false; });
        assert(node != nullptr && node != root && node->getParent() == root);
      } else {
        root->initialize(MATE_SFEN, turn + remaining);
      }
      assert(node->getBoard().getSfen() == board.getSfen());

      // Check rejecting a mate preserves evaluation values, statistics, and candidates.
      Move policy_move = node->getBoard().getLegalMoves(true, false).front();
      node->applyInferenceResult(0.25f, 40.0f, {{policy_move, 1.0f}});
      node->updateMctsValue(0.125f);
      node->_waitingPolicies.emplace(policy_move, 1.0f);
      node->_waitingMoves.insert(policy_move.getValue());
      PnSearchEngine engine(100000);
      node->searchCheckmateMoves(&engine, std::min(29, remaining + 1));
      assert(node->isCheckmateSearched());
      assert(node->isEvaluated());
      assert(node->getVisits() == 0);
      assert(node->getRemainingTurns() == 40.0f);

      // Accept mates ending exactly at or before the draw limit through normal confirmation.
      if (remaining >= mate_length) {
        assert(node->getCheckmateMoves() == expected_moves);
        assert(node->getNodeValue() == node->getBoard().getColor());
        assert(node->getMctsValue() == node->getBoard().getColor());
        assert(node->getPolicies().empty());
        assert(node->_waitingPolicies.empty() && node->_waitingMoves.empty());
        continue;
      }

      // Do not store over-limit mates or mark the node as a draw or proven nonmate.
      assert(node->getCheckmateMoves().empty());
      assert(node->getNodeValue() == 0.25f && node->getMctsValue() == 0.125f);
      auto policies = node->getPolicies();
      assert(policies.size() == 1 && policies.front().getMove() == policy_move);
      assert(node->_waitingPolicies.size() == 1 && node->_waitingMoves.size() == 1);
      if (remaining <= 0) {
        assert(engine.getNodeCount() == 0);
      } else {
        // Continue normal search with retained candidates if moves remain before the draw limit.
        MctsNode* next = node->pickupNextNode(false, 0, 1.0f, 0.0f, []() { return false; });
        assert(next != nullptr && next != node && next->getParent() == node);
      }
    }
  }
}

/**
 * Check normal search returns candidates after rejecting an over-limit mate.
 * @param processor Inference processor.
 */
static void TestSearchAfterRejectedMate(InferenceProcessor* processor) {
  Player player(processor, 1, 1000000, 28, 27, 0.0f, 29, 100000, 1,
                1.6f, 3200.0f, 0.0f);
  player.initialize(MATE_SFEN, 139);

  // Run PN search before the first visit without storing an over-limit mate.
  player._runCheckmateSearch(player._root);
  assert(player._root->isCheckmateSearched());
  assert(player._root->getCheckmateMoves().empty());
  assert(!player._root->isEvaluated());
  assert(player.getVisits() == 0);

  // Release the wait through model evaluation and visit notifications without another PN search.
  player.startEvaluation(false, 0, 1.0f, 0.0f);
  player.waitEvaluation(4, 2.0f, true);
  auto candidates = player.getCandidates();
  assert(player.getVisits() >= 4);
  assert(!candidates.empty());
  assert(player._root->getCheckmateMoves().empty());
  for (const auto& candidate : candidates) {
    assert(std::isfinite(candidate.getValue()));
  }
}

/**
 * Check repetition penalizes the returning side and backpropagation preserves the sign.
 * @param processor Inference processor.
 */
static void TestSennichitePenalty(InferenceProcessor* processor) {
  for (float penalty : {0.0f, 0.2f}) {
    for (int32_t initial_color : {COLOR_BLACK, COLOR_WHITE}) {
      for (bool from_history : {false, true}) {
        Player player(processor, 1, 64, 28, 27, penalty, 1, 1, 0,
                      1.6f, 3200.0f, 0.0f);
        std::string sfen = "4k4/9/9/9/9/9/9/9/4K4 ";
        sfen += initial_color == COLOR_BLACK ? "b - 1" : "w - 1";
        player.initialize(sfen, 512);

        // Move both kings to adjacent files and back, repeating the initial position on move four.
        const int32_t first_rank = initial_color == COLOR_BLACK ? 8 : 0;
        const int32_t second_rank = 8 - first_rank;
        const std::vector<Move> moves = {
            Move(Position(4, first_rank), Position(5, first_rank), false),
            Move(Position(4, second_rank), Position(5, second_rank), false),
            Move(Position(5, first_rank), Position(4, first_rank), false),
            Move(Position(5, second_rank), Position(4, second_rank), false)};
        if (from_history) {
          // Use actual moves to put the repeated position in game history only.
          for (int32_t i = 0; i < 3; ++i) {
            player.play(moves[i]);
          }
        }

        // Fix one candidate per node and expand the actual path to the repeating leaf.
        MctsNode* node = player._root;
        for (int32_t i = from_history ? 3 : 0; i < 4; ++i) {
          node->applyInferenceResult(0.0f, 4.0f, {{moves[i], 1.0f}});
          MctsNode* next = node->pickupNextNode(false, 0, 1.0f, 0.0f,
                                               []() { return false; });
          assert(next != nullptr && next != node && next->getParent() == node);
          node = next;
        }

        // Check the value penalizes the side causing repetition rather than the resulting side to
        // move.
        const int32_t repeating_color = OPPOSITE_COLOR(initial_color);
        const float expected_value = -static_cast<float>(repeating_color) * penalty;
        assert(node->pickupNextNode(false, 0, 1.0f, 0.0f,
                                    []() { return false; }) == node);
        assert(node->isEvaluated() && node->getPolicies().empty());
        assert(node->getCheckmateMoves().empty());
        assert(std::fabs(node->getNodeValue() - expected_value) < 1e-6f);
        assert(std::fabs(node->getNodeValue() * repeating_color + penalty) < 1e-6f);

        // Backpropagate the leaf value with the real update thread and check signs at every
        // ancestor.
        {
          std::lock_guard<std::mutex> lock(player._mutex);
          player._evaluatingNodes.push(node);
        }
        player._updateCondition.notify_one();
        {
          std::unique_lock<std::mutex> lock(player._mutex);
          bool updated = player._stopCondition.wait_for(lock, 2s, [&player]() {
            return player._isSearchIdle();
          });
          assert(updated);
        }
        for (MctsNode* current = node; current != nullptr; current = current->getParent()) {
          assert(std::fabs(current->getMctsValue() - expected_value) < 1e-6f);
        }

        // Check C++ candidates contain the same value without Python corrections.
        auto candidates = player.getCandidates();
        assert(candidates.size() == 1);
        assert(std::fabs(candidates.front().getValue() - expected_value) < 1e-6f);
      }
    }
  }
}

/**
 * Check failed initialization preserves tree and stop state and permits temporary node reuse.
 * @param processor Inference processor.
 */
static void TestFailedInitializeKeepsTree(InferenceProcessor* processor) {
  for (bool reuse_node : {false, true}) {
    Player player(processor, 1, 1000000, 31, 32, 0.0f, 1, 1, 0,
                  1.6f, 3200.0f, 0.0f);
    MctsNode* root = player._root;
    const std::string sfen = root->getBoard().getSfen();
    root->initialize(sfen, 300);

    // Prepare a tree with children and values and check initialization failure preserves it.
    Move move = root->getBoard().getLegalMoves(true, false).front();
    root->applyInferenceResult(0.25f, 20.0f, {{move, 1.0f}});
    MctsNode* child = root->pickupNextNode(false, 0, 1.0f, 0.0f, []() { return false; });
    assert(child != nullptr && child != root);
    root->updateMctsValue(0.375f);
    const auto children = root->getChildren();
    const auto used_nodes = player._manager._usedNodes;

    // Cover new allocations, pool reuse, and cancellation from explicit stops.
    if (reuse_node) {
      MctsNode* unused = player._manager.createNode();
      player._manager.releaseNode(unused);
      player.stopEvaluation();
    }
    const std::size_t expected_nodes = player._manager._nodes.size() + (reuse_node ? 0 : 1);
    const bool canceled = player._canceled.load(std::memory_order_acquire);
    const bool stop_requested = player._stopRequested;
    const uint64_t stop_sequence = player._stopSequence;
    const std::string invalid_turn = sfen.substr(0, sfen.find_last_of(' ') + 1) + "invalid";

    // Repeat invalid piece and turn inputs, returning and reusing the same temporary node.
    for (int32_t attempt = 0; attempt < 4; ++attempt) {
      bool rejected = false;
      try {
        player.initialize(attempt % 2 == 0 ? "X8/9/9/9/9/9/9/9/8K b - 1" : invalid_turn, 400);
      } catch (const std::out_of_range&) {
        assert(attempt % 2 == 0);
        rejected = true;
      } catch (const std::invalid_argument&) {
        assert(attempt % 2 == 1);
        rejected = true;
      }
      assert(rejected);
      assert(player._root == root && root->getBoard().getSfen() == sfen);
      assert(root->getBoard().getDrawTurn() == 300 && root->getChildren() == children);
      assert(root->getNodeValue() == 0.25f && root->getMctsValue() == 0.375f);
      assert(child->getParent() == root);
      assert(!player._paused && player._stopped);
      assert(player._canceled.load(std::memory_order_acquire) == canceled);
      assert(player._stopRequested == stop_requested && player._stopSequence == stop_sequence);
      assert(player._manager._usedNodes == used_nodes);
      assert(player._manager._nodes.size() == expected_nodes);
      assert(player._manager._poolNodes.size() == 1);
    }

    // Allow search and play after failure and replace the board on successful initialization.
    player.startEvaluation(true, 0, 1.0f, 0.0f);
    player.waitEvaluation(16, 2.0f, true);
    auto candidates = player.getCandidates();
    assert(!candidates.empty() && player.getVisits() > 0);
    Board expected;
    expected.copyFrom(&root->getBoard());
    expected.play(candidates.front().getMove());
    player.play(candidates.front().getMove());
    assert(player._root->getBoard().getSfen() == expected.getSfen());
    player.initialize(MATE_SFEN, 512);
    assert(player._root->getBoard().getSfen() == MATE_SFEN);
    assert(player._root->getBoard().getDrawTurn() == 512 && player.getVisits() == 0);
  }
}

/**
 * Check invalid position initialization does not stop active search.
 * @param processor Inference processor.
 */
static void TestFailedInitializeWhileSearching(InferenceProcessor* processor) {
  Player player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);
  player.startEvaluation(true, 16, 1.0f, 0.0f);
  player.waitEvaluation(4, 2.0f, false);

  // Race initialization failure with active search while preserving the valid root.
  MctsNode* root = player._root;
  const std::string sfen = root->getBoard().getSfen();
  bool rejected = false;
  try {
    player.initialize("X8/9/9/9/9/9/9/9/8K b - 1", 400);
  } catch (const std::out_of_range&) {
    rejected = true;
  }
  assert(rejected);
  {
    std::lock_guard<std::mutex> lock(player._mutex);
    assert(player._root == root && root->getBoard().getSfen() == sfen);
    assert(!player._paused && !player._stopped && !player._stopRequested);
    assert(!player._canceled.load(std::memory_order_acquire));
  }

  // Check visits increase without restarting and results remain available.
  int32_t visits = player.getVisits();
  player.waitEvaluation(visits + 16, 2.0f, true);
  assert(player.getVisits() > visits);
  assert(!player.getCandidates().empty());
}

/**
 * Check temporary node allocation failure preserves the board and pause state.
 * @param processor Inference processor.
 */
static void TestInitializeAllocationFailure(InferenceProcessor* processor) {
  Player player(processor, 1, 64, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);
  MctsNode* root = player._root;
  const std::string sfen = root->getBoard().getSfen();

  // Fail the first temporary node allocation without returning a node that was never acquired.
  bool rejected = false;
  fail_next_allocation = true;
  try {
    player.initialize(sfen, 300);
  } catch (const std::bad_alloc&) {
    rejected = true;
  }
  assert(rejected && !fail_next_allocation);
  assert(player._root == root && root->getBoard().getSfen() == sfen);
  assert(!player._paused && player._stopped);
  assert(!player._canceled.load(std::memory_order_acquire));
  assert(player._manager._usedNodes.size() == 1);
  assert(player._manager._nodes.size() == 1 && player._manager._poolNodes.empty());
}

/**
 * Check failed moves return temporary nodes and preserve the original board and children.
 */
static void TestFailedGetChildReturnsNode() {
  for (bool reuse_node : {false, true}) {
    MctsManager manager(MctsParameter(28, 27, 0.0f, 1.6f, 3200.0f, 0.0f));
    MctsNode* root = manager.createNode();
    root->initialize(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", 300);
    const std::string sfen = root->getBoard().getSfen();
    if (reuse_node) {
      manager.releaseNode(manager.createNode());
    }

    // Cover empty sources, failures during captures, and insufficient hand pieces on temporary
    // boards.
    const std::vector<Move> invalid_moves = {
        Move(Position(0, 4), Position(0, 3), false),
        Move(Position(0, 4), Position(0, 2), false),
        Move(Position(BOARD_SIZE, PIECE_HAND_PAWN), Position(0, 4), false)};
    for (int32_t attempt = 0; attempt < 6; ++attempt) {
      bool rejected = false;
      try {
        root->getChild(invalid_moves[attempt % invalid_moves.size()]);
      } catch (const std::invalid_argument&) {
        rejected = true;
      }
      assert(rejected);
      assert(root->getBoard().getSfen() == sfen && root->getChildren().empty());
      assert(manager._usedNodes.size() == 1 && manager._usedNodes.count(root) == 1);
      assert(manager._nodes.size() == 2 && manager._poolNodes.size() == 1);
    }

    // Reuse failed temporary nodes for legal moves without carrying partial board changes over.
    MctsNode* unused = manager._poolNodes.back();
    const Move move = root->getBoard().getLegalMoves(true, false).front();
    Board expected;
    expected.copyFrom(&root->getBoard());
    expected.play(move);
    MctsNode* child = root->getChild(move);
    assert(child == unused && child->getBoard().getSfen() == expected.getSfen());
    assert(child->getBoard().getDrawTurn() == 300);
    manager.releaseNode(child);
  }
}

/**
 * Check failed moves preserve stop state, tree, and settings and permit later legal moves.
 * @param processor Inference processor.
 */
static void TestFailedPlayKeepsTree(InferenceProcessor* processor) {
  for (int32_t state : {0, 1, 2}) {
    Player player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                  1.6f, 3200.0f, 0.0f);
    MctsNode* root = player._root;
    const std::string sfen = root->getBoard().getSfen();

    // Prepare values, children, and history; compare unstarted, explicitly stopped, and paused
    // states.
    const Move move = root->getBoard().getLegalMoves(true, false).front();
    root->applyInferenceResult(0.25f, 20.0f, {{move, 1.0f}});
    MctsNode* child = root->pickupNextNode(false, 0, 1.0f, 0.0f, []() { return false; });
    root->updateMctsValue(0.375f);
    root->_appearedBoardHashes.insert(BoardHash(&root->getBoard()));
    if (state == 1) {
      player.stopEvaluation();
    }
    {
      std::lock_guard<std::mutex> lock(player._mutex);
      player._paused = state == 2;
      player._canceled.store(state != 0, std::memory_order_release);
      player._searchEqually = true;
      player._searchCandidateWidth = 16;
      player._searchTemperature = 0.75f;
      player._searchNoise = 0.25f;
    }
    const auto children = root->getChildren();
    const auto hashes = root->_appearedBoardHashes;
    const auto used_nodes = player._manager._usedNodes;
    const int32_t visits = root->getVisits();
    const uint64_t stop_sequence = player._stopSequence;

    // Preserve execution state, statistics, history, and active nodes across repeated failures.
    for (int32_t attempt = 0; attempt < 3; ++attempt) {
      bool rejected = false;
      try {
        player.play(Move(Position(0, 4), Position(0, 2), false));
      } catch (const std::invalid_argument& error) {
        rejected = true;
        assert(std::string(error.what()) == "Position does not have a piece");
      }
      assert(rejected);
      assert(player._root == root && root->getBoard().getSfen() == sfen);
      assert(root->getChildren() == children && child->getParent() == root);
      assert(root->_appearedBoardHashes.size() == hashes.size());
      for (const auto& hash : hashes) {
        assert(root->_appearedBoardHashes.count(hash) == 1);
      }
      assert(root->getVisits() == visits);
      assert(root->getNodeValue() == 0.25f && root->getMctsValue() == 0.375f);
      assert(player._paused == (state == 2) && player._stopped);
      assert(player._canceled.load(std::memory_order_acquire) == (state != 0));
      assert(player._stopRequested == (state == 1) && player._stopSequence == stop_sequence);
      assert(player._searchEqually && player._searchCandidateWidth == 16);
      assert(player._searchTemperature == 0.75f && player._searchNoise == 0.25f);
      assert(player._manager._usedNodes == used_nodes);
      assert(player._manager._poolNodes.size() == 1);
    }

    // Preserve successful reuse of an existing child as root and subsequent search.
    Board expected;
    expected.copyFrom(&root->getBoard());
    expected.play(move);
    player.play(move);
    assert(player._root == child && child->getParent() == nullptr);
    assert(child->getBoard().getSfen() == expected.getSfen());
    assert(!player._searchEqually && player._searchCandidateWidth == 0);
    assert(player._searchTemperature == 1.0f && player._searchNoise == 0.0f);
    player.startEvaluation(false, 0, 1.0f, 0.0f);
    player.waitEvaluation(4, 2.0f, true);
    assert(player.getVisits() >= 4 && !player.getCandidates().empty());
  }
}

/**
 * Check failed moves during search need no restart and leak no temporary nodes.
 * @param processor Inference processor.
 */
static void TestFailedPlayWhileSearching(InferenceProcessor* processor) {
  Player player(processor, 2, 1000000, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);
  player.startEvaluation(true, 16, 0.75f, 0.25f);
  player.waitEvaluation(4, 2.0f, false);
  MctsNode* root = player._root;
  const std::string sfen = root->getBoard().getSfen();

  // Repeat invalid moves with real workers and inspect restored settings.
  for (int32_t attempt = 0; attempt < 3; ++attempt) {
    bool rejected = false;
    try {
      player.play(Move(Position(0, 4), Position(0, 3), false));
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
    {
      std::lock_guard<std::mutex> lock(player._mutex);
      assert(player._root == root && root->getBoard().getSfen() == sfen);
      assert(!player._paused && !player._stopped && !player._stopRequested);
      assert(!player._canceled.load(std::memory_order_acquire));
      assert(player._searchEqually && player._searchCandidateWidth == 16);
      assert(player._searchTemperature == 0.75f && player._searchNoise == 0.25f);
    }
    const int32_t visits = player.getVisits();
    player.waitEvaluation(visits + 16, 2.0f, false);
    assert(player.getVisits() > visits);
  }

  // Check reachability after stopping to distinguish valid expansion from leaked temporary nodes.
  player.stopEvaluation();
  const auto candidates = player.getCandidates();
  assert(!candidates.empty());
  std::set<MctsNode*> reachable;
  std::vector<MctsNode*> stack = {root};
  while (!stack.empty()) {
    MctsNode* node = stack.back();
    stack.pop_back();
    assert(reachable.insert(node).second);
    const auto children = node->getChildren();
    stack.insert(stack.end(), children.begin(), children.end());
  }
  assert(player._manager._usedNodes == reachable);
  player.play(candidates.front().getMove());
  player.startEvaluation(false, 0, 1.0f, 0.0f);
  player.waitEvaluation(8, 2.0f, true);
  assert(player.getVisits() >= 8 && !player.getCandidates().empty());
}

/**
 * Do not undo normal or explicit stops received while waiting for a failed move to pause.
 * @param processor Inference processor.
 */
static void TestStopDuringFailedPlay(InferenceProcessor* processor) {
  for (bool explicit_stop : {false, true}) {
    Player player(processor, 1, 64, 28, 27, 0.0f, 1, 1, 0,
                  1.6f, 3200.0f, 0.0f);
    // Simulate active tasks and pending inference to control stopping order.
    {
      std::lock_guard<std::mutex> lock(player._mutex);
      player._stopped = false;
      player._runnings = 1;
      player._pendingInferences = 1;
    }
    auto result = std::async(std::launch::async, [&player]() {
      try {
        player.play(Move(Position(0, 4), Position(0, 3), false));
      } catch (const std::invalid_argument&) {
        return true;
      }
      return false;
    });
    bool paused = false;
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
      {
        std::lock_guard<std::mutex> lock(player._mutex);
        paused = player._paused;
      }
      if (paused) {
        break;
      }
      std::this_thread::yield();
    }
    assert(paused);

    // Accept stop while waiting, then signal inference completion and attempt the invalid move.
    if (explicit_stop) {
      player.stopEvaluation();
    } else {
      player.waitEvaluation(0, 0.0f, true);
    }
    {
      std::lock_guard<std::mutex> lock(player._mutex);
      player._runnings = 0;
      player._pendingInferences = 0;
      player._stopCondition.notify_all();
    }
    assert(result.wait_for(2s) == std::future_status::ready && result.get());
    assert(!player._paused && player._stopped && player._stopSequence == 1);
    assert(player._stopRequested == explicit_stop);
    assert(player._canceled.load(std::memory_order_acquire) == explicit_stop);
    assert(player._manager._usedNodes.size() == 1);
  }
}

/**
 * Check move node allocation failure preserves execution state and the board.
 * @param processor Inference processor.
 */
static void TestPlayAllocationFailure(InferenceProcessor* processor) {
  Player player(processor, 1, 64, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);
  MctsNode* root = player._root;
  const std::string sfen = root->getBoard().getSfen();
  const Move move = root->getBoard().getLegalMoves(true, false).front();

  // Restore state even for exceptions before acquiring a node and never return an unacquired node.
  bool rejected = false;
  fail_next_allocation = true;
  try {
    player.play(move);
  } catch (const std::bad_alloc&) {
    rejected = true;
  }
  assert(rejected && !fail_next_allocation);
  assert(player._root == root && root->getBoard().getSfen() == sfen);
  assert(!player._paused && player._stopped && !player._stopRequested);
  assert(!player._canceled.load(std::memory_order_acquire));
  assert(player._manager._usedNodes.size() == 1);
  assert(player._manager._nodes.size() == 1 && player._manager._poolNodes.empty());
  player.play(move);
}

/**
 * Check first-visit waits have no deadline and normal stops release them.
 * @param processor Inference processor.
 */
static void TestNormalStop(InferenceProcessor* processor) {
  Player player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);

  // Do not return before the first visit even with zero timeout; release on another wait's stop.
  auto result = std::async(std::launch::async, [&player]() {
    player.waitEvaluation(1000, 0.0f, true);
  });
  assert(result.wait_for(20ms) == std::future_status::timeout);
  player.waitEvaluation(0, 0.0f, true);
  AssertReleased(player, result);
  assert(player.getVisits() == 0);

  // Start a timed wait after a visit and release it through normal stop before timeout.
  player.initialize("k8/1G7/1K7/9/9/9/9/9/9 w - 1", 512);
  player._root->pickupNextNode(false, 0, 1.0f, 0.0f, []() { return false; });
  {
    std::lock_guard<std::mutex> lock(player._mutex);
    // Prevent initialization notifications from restarting search and test only wait stop
    // conditions.
    player._paused = true;
    player._stopped = false;
  }
  result = std::async(std::launch::async, [&player]() {
    player.waitEvaluation(1000, 60.0f, true);
  });
  assert(result.wait_for(20ms) == std::future_status::timeout);
  player.waitEvaluation(0, 0.0f, true);
  AssertReleased(player, result);
}

/**
 * Check an old wait cannot stop a search restarted immediately after a stop.
 * @param processor Inference processor.
 */
static void TestStopAndRestart(InferenceProcessor* processor) {
  Player player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);
  auto result = std::async(std::launch::async, [&player]() {
    player.waitEvaluation(1000, 60.0f, true);
  });
  assert(result.wait_for(20ms) == std::future_status::timeout);

  // Complete stop notification and restart state changes before the waiter reacquires the mutex.
  {
    std::lock_guard<std::mutex> lock(player._mutex);
    ++player._stopSequence;
    player._stopped = true;
    player._stopRequested = true;
    player._waitCondition.notify_all();
    player._paused = true;
    player._stopped = false;
    player._stopRequested = false;
  }
  AssertReleased(player, result);
  {
    std::lock_guard<std::mutex> lock(player._mutex);
    assert(!player._stopped);
    player._stopped = true;
  }
}

/**
 * Check search finishes when the last visit notification races with waiting.
 * @param processor Inference processor.
 */
static void TestLastVisitNotification(InferenceProcessor* processor) {
  Player player(processor, 1, 1, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);
  for (int32_t i = 0; i < 100; ++i) {
    // Use a position with no legal moves to race the first visit notification without inference.
    player.initialize("k8/1G7/1K7/9/9/9/9/9/9 w - 1", 512);
    auto result = std::async(std::launch::async, [&player]() {
      player.waitEvaluation(1, 60.0f, true);
    });
    player.startEvaluation(false, 0, 1.0f, 0.0f);
    AssertReleased(player, result);
    assert(player.getVisits() == 1);
  }
}

/**
 * Check clearing the mate queue wakes an update thread that can now terminate.
 * @param processor Inference processor.
 * @param runningTask True to await an active task after clearing the queue.
 * @return True if both threads exit without a rescue notification.
 */
static bool TestQueueDiscardShutdown(InferenceProcessor* processor, bool runningTask) {
  // Keep completion notifications alive past player destruction and exercise destructor joins.
  std::promise<void> search_finished;
  std::promise<void> update_finished;
  auto search_result = search_finished.get_future();
  auto update_result = update_finished.get_future();
  Player player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);

  // Stop initial threads while idle, then restart in the test-controlled order.
  {
    std::lock_guard<std::mutex> lock(player._mutex);
    player._terminated = true;
  }
  player._searchCondition.notify_one();
  player._updateCondition.notify_one();
  player._searchThread.join();
  player._updateThread.join();
  {
    std::lock_guard<std::mutex> lock(player._mutex);
    player._stopRequested = true;
    player._checkingNodes.push(player._root);
    player._evaluatingNodes.push(player._root);
    player._runnings = runningTask ? 1 : 0;
  }

  // Block actual updates at the evaluated check to fix ordering with a false termination condition.
  std::unique_lock<std::shared_mutex> node_lock(player._root->_mutex);
  player._updateThread = std::thread([&player, &update_finished]() {
    player._runUpdate();
    update_finished.set_value();
  });
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  bool update_has_lock = false;
  while (std::chrono::steady_clock::now() < deadline) {
    if (!player._mutex.try_lock()) {
      update_has_lock = true;
      break;
    }
    player._mutex.unlock();
    std::this_thread::yield();
  }
  assert(update_has_lock);
  node_lock.unlock();

  // Acquiring the mutex proves updates are waiting and only the mate queue remains.
  {
    std::lock_guard<std::mutex> lock(player._mutex);
    player._evaluatingNodes.pop();
    assert(!player._isSearchIdle());
    player._searchThread = std::thread([&player, &search_finished]() {
      player._runSearch();
      search_finished.set_value();
    });
  }

  // With an active task, queue clearing alone must not terminate; its completion notification does.
  if (runningTask) {
    {
      std::unique_lock<std::mutex> lock(player._mutex);
      bool discarded = player._stopCondition.wait_for(lock, 2s, [&player]() {
        return player._checkingNodes.empty();
      });
      assert(discarded);
      assert(!player._isSearchIdle());
      assert(search_result.wait_for(0s) == std::future_status::timeout);
      assert(update_result.wait_for(0s) == std::future_status::timeout);
      player._runnings = 0;
    }
    player._searchCondition.notify_one();
    player._updateCondition.notify_one();
    player._stopCondition.notify_all();
  }

  // Send a rescue notification only on failure so the test also cleans up before the fix.
  bool search_exited = search_result.wait_for(2s) == std::future_status::ready;
  bool update_exited = update_result.wait_for(2s) == std::future_status::ready;
  if (!search_exited || !update_exited) {
    player._searchCondition.notify_one();
    player._updateCondition.notify_one();
  }
  search_result.get();
  update_result.get();
  if (!search_exited || !update_exited) {
    std::cerr << "Queue discard shutdown failed: runningTask=" << runningTask
              << ", search_exited=" << search_exited
              << ", update_exited=" << update_exited << std::endl;
  }
  return search_exited && update_exited;
}

/**
 * Check the evaluated flag does not imply idleness before callback completion.
 * @param processor Inference processor with caching enabled.
 */
static void TestInferenceCompletion(InferenceProcessor* processor) {
  Player player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);
  std::string sfen = player._root->getBoard().getSfen();
  int32_t draw_turn = player._root->getBoard().getDrawTurn();
  player._root->pickupNextNode(false, 0, 1.0f, 0.0f, []() { return false; });

  // Block inference before queue submission and acquire the player mutex after the count increases.
  std::unique_lock<std::mutex> queue_lock(processor->_queueMutex);
  auto submitted = std::async(std::launch::async, [&player]() {
    player._submitInference(player._root);
  });
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  bool registered = false;
  while (std::chrono::steady_clock::now() < deadline) {
    {
      std::lock_guard<std::mutex> lock(player._mutex);
      registered = player._pendingInferences == 1;
    }
    if (registered) {
      break;
    }
    std::this_thread::yield();
  }
  assert(registered);
  std::unique_lock<std::mutex> lock(player._mutex);
  queue_lock.unlock();

  // Wait for real inference to mark evaluation complete, blocking only its callback with the mutex.
  {
    std::unique_lock<std::shared_mutex> node_lock(player._root->_mutex);
    bool evaluated = player._root->_condition.wait_for(node_lock, 2s, [&player]() {
      return player._root->_evaluated;
    });
    assert(evaluated);
  }
  assert(player._runnings == 0 && player._updatingNodes == 0);
  assert(player._evaluatingNodes.empty() && player._checkingNodes.empty());
  assert(player._pendingInferences == 1);
  assert(!player._isSearchIdle());
  lock.unlock();
  submitted.get();
  lock.lock();
  bool idle = player._stopCondition.wait_for(lock, 2s, [&player]() {
    return player._isSearchIdle();
  });
  assert(idle);
  lock.unlock();

  // On a cache hit for the same position, the count is zero when the synchronous callback returns.
  player.initialize(sfen, draw_turn);
  player._root->pickupNextNode(false, 0, 1.0f, 0.0f, []() { return false; });
  player._submitInference(player._root);
  assert(player._root->isEvaluated());
  {
    std::lock_guard<std::mutex> state_lock(player._mutex);
    assert(player._pendingInferences == 0);
    assert(player._isSearchIdle());
  }
  assert(processor->getCacheHitRate() > 0.0f);
}

/**
 * Check failed submissions leave no pending count if the request was never accepted.
 * @param processor Inference processor with caching disabled.
 */
static void TestInferenceRegistrationFailure(InferenceProcessor* processor) {
  Player player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                1.6f, 3200.0f, 0.0f);
  player._root->pickupNextNode(false, 0, 1.0f, 0.0f, []() { return false; });
  bool failed = false;
  fail_next_allocation = true;
  try {
    player._submitInference(player._root);
  } catch (const std::bad_alloc&) {
    failed = true;
  }
  fail_next_allocation = false;
  assert(failed);
  {
    std::lock_guard<std::mutex> lock(player._mutex);
    assert(player._pendingInferences == 0);
    assert(player._isSearchIdle());
  }

  // Resubmit the same unevaluated node and verify normal completion waits still work.
  player._submitInference(player._root);
  std::unique_lock<std::mutex> lock(player._mutex);
  bool idle = player._stopCondition.wait_for(lock, 2s, [&player]() {
    return player._isSearchIdle();
  });
  assert(idle);
  assert(player._root->isEvaluated());
}

/**
 * Check operations using nodes await pending callback completion.
 * @param processor Inference processor.
 */
static void TestInferenceWaiters(InferenceProcessor* processor) {
  for (int32_t operation = 0; operation < 3; ++operation) {
    Player player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                  1.6f, 3200.0f, 0.0f);
    Move move = player._root->getBoard().getLegalMoves(true, false).front();
    {
      std::lock_guard<std::mutex> lock(player._mutex);
      player._pendingInferences = 1;
    }
    // Make results, initialization, and moves wait for a callback after results have been applied.
    auto result = std::async(std::launch::async, [&player, operation, move]() {
      if (operation == 0) {
        player.getCandidates();
      } else if (operation == 1) {
        player.initialize(MATE_SFEN, 512);
      } else {
        player.play(move);
      }
    });
    assert(result.wait_for(20ms) == std::future_status::timeout);
    player._finishInference();
    AssertReleased(player, result);
  }
}

/**
 * Check destruction waits even if only a pending callback remains.
 * @param processor Inference processor.
 */
static void TestInferenceShutdown(InferenceProcessor* processor) {
  auto* player = new Player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                            1.6f, 3200.0f, 0.0f);
  {
    std::lock_guard<std::mutex> lock(player->_mutex);
    player->_pendingInferences = 1;
  }
  auto destroyed = std::async(std::launch::async, [player]() { delete player; });
  assert(destroyed.wait_for(20ms) == std::future_status::timeout);
  // Do not access the player after this notification because destruction can proceed.
  player->_finishInference();
  assert(destroyed.wait_for(2s) == std::future_status::ready);
  destroyed.get();
}

/**
 * Check queued PN searches do not run after destruction is requested.
 * @param processor Inference processor.
 */
static void TestQueuedCheckmateShutdown(InferenceProcessor* processor) {
  std::promise<void> worker_started;
  std::promise<void> release_worker;
  std::promise<bool> search_skipped;
  auto started = worker_started.get_future();
  auto release = release_worker.get_future();
  auto skipped = search_skipped.get_future();
  auto* player = new Player(processor, 1, 1000000, 28, 27, 0.0f, 1, 1, 0,
                            1.6f, 3200.0f, 0.0f);

  // Block the worker on an earlier task so PN search has not started.
  player->_threadPool.submit([&worker_started, &release]() {
    worker_started.set_value();
    release.wait();
  });
  started.get();
  {
    std::lock_guard<std::mutex> lock(player->_mutex);
    player->_runnings = 1;
  }
  player->_threadPool.submit([player, &search_skipped]() {
    player->_runCheckmateSearch(player->_root);
    search_skipped.set_value(!player->_root->isCheckmateSearched());
    // Finish all player access from this task before allowing destruction to proceed.
    std::lock_guard<std::mutex> lock(player->_mutex);
    --player->_runnings;
    player->_searchCondition.notify_one();
    player->_updateCondition.notify_one();
    player->_stopCondition.notify_all();
  });

  // Block workers until shutdown is requested so destruction first enters its completion wait.
  auto destroyed = std::async(std::launch::async, [player]() { delete player; });
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  bool terminated = false;
  while (std::chrono::steady_clock::now() < deadline) {
    {
      std::lock_guard<std::mutex> lock(player->_mutex);
      terminated = player->_terminated;
    }
    if (terminated) {
      break;
    }
    std::this_thread::yield();
  }
  assert(terminated);
  assert(destroyed.wait_for(0s) == std::future_status::timeout);
  release_worker.set_value();
  assert(skipped.get());
  assert(destroyed.wait_for(2s) == std::future_status::ready);
  destroyed.get();
}

/**
 * Check synchronous prediction awaits completion for both asynchronous work and cache hits.
 * @param processor Inference processor with caching enabled.
 */
static void TestSynchronousPrediction(InferenceProcessor* processor) {
  Board board;
  board.initialize("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", 512);
  board.play(board.getLegalMoves(true, false).front());
  float first = processor->predict(&board);
  float cached = processor->predict(&board);
  assert(std::isfinite(first));
  assert(first == cached);
  assert(processor->getCacheHitRate() > 0.0f);
}

/**
 * Check each class normalizes its own settings and remains usable.
 * @param processor Inference processor.
 */
static void TestConstructorCorrections(InferenceProcessor* processor) {
  Board board;
  board.initialize(
      "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", 512);

  // Check negative node counts behave like zero capacity and one-node search still works.
  for (int32_t nodes : {std::numeric_limits<int32_t>::min(), -1, 0, 1}) {
    PnSearchEngine engine(nodes);
    assert(engine.getCheckmateMoves(&board, 1).empty());
    assert(engine.getNodeCount() == (nodes == 1 ? 1 : 0));
  }

  for (int32_t size : {std::numeric_limits<int32_t>::min(), -1, 0, 1, 2}) {
    const int32_t expected_size = size == 2 ? 2 : 1;

    // Acquire the normalized number of engines and reuse them after release.
    PnSearchManager manager(-1, size);
    std::vector<PnSearchEngine*> engines;
    for (int32_t i = 0; i < expected_size; ++i) {
      PnSearchEngine* engine = manager.acquire();
      assert(std::find(engines.begin(), engines.end(), engine) == engines.end());
      assert(engine->getCheckmateMoves(&board, 1).empty());
      assert(engine->getNodeCount() == 0);
      engines.push_back(engine);
    }
    for (PnSearchEngine* engine : engines) {
      manager.release(engine);
    }
    PnSearchEngine* reused = manager.acquire();
    assert(std::find(engines.begin(), engines.end(), reused) != engines.end());
    manager.release(reused);

    // Check submitted tasks run after thread count normalization.
    std::promise<void> completed;
    auto result = completed.get_future();
    ThreadPool pool(size);
    assert(pool.getSize() == expected_size);
    pool.submit([&completed]() { completed.set_value(); });
    assert(result.wait_for(2s) == std::future_status::ready);
    result.get();
  }

  // Normalize values passed by Player in each owning class and search to one maximum visit.
  for (int32_t value : {std::numeric_limits<int32_t>::min(), -1, 0, 1}) {
    Player player(processor, value, value, 28, 27, 0.0f, 1, value, 0,
                  1.6f, -1.0f, 0.0f);
    assert(player._searchMaxVisits == 1);
    assert(player._threadPool.getSize() == 1);
    assert(player._manager.getParameter().getPucbConstantBase() == 1.0f);
    player.startEvaluation(false, 0, 1.0f, 0.0f);
    player.waitEvaluation(1, 5.0f, true);
    assert(player.getVisits() == 1);
    assert(!player.getCandidates().empty());
  }

  // Reject a null processor in the constructor body and clean up constructed workers.
  bool rejected = false;
  try {
    Player player(nullptr, 1, 1, 28, 27, 0.0f, 1, 1, 0, 1.6f, 1.0f, 0.0f);
  } catch (const std::invalid_argument& error) {
    rejected = true;
    assert(std::string(error.what()).find("processor") != std::string::npos);
  }
  assert(rejected);
}

/**
 * Check allocation failures at every PN pool construction stage leak no resources.
 */
static void TestPnSearchManagerAllocationFailure() {
  bool constructed = false;
  for (int32_t failure_index = 0; failure_index < 128; ++failure_index) {
    // Vary failures from the first allocation through partial construction of multiple engines.
    allocations_until_failure = failure_index;
    tracked_live_allocations = 0;
    track_allocations = true;
    try {
      PnSearchManager manager(1, 3);
      constructed = true;
    } catch (const std::bad_alloc&) {
    }
    track_allocations = false;
    if (tracked_live_allocations != 0) {
      std::cerr << "PN pool allocation failure: index=" << failure_index
                << ", remaining=" << tracked_live_allocations << std::endl;
    }
    assert(tracked_live_allocations == 0);
    if (constructed) {
      break;
    }
  }
  assert(constructed);
}

/**
 * Check direct C++ construction rejects nonfinite PUCB bases.
 * @param processor Inference processor.
 */
static void TestPucbBaseValidation(InferenceProcessor* processor) {
  // Inspect original arguments to reject negative infinity even if normalized to one.
  for (float base : {std::numeric_limits<float>::quiet_NaN(),
                     std::numeric_limits<float>::infinity(),
                     -std::numeric_limits<float>::infinity()}) {
    bool parameter_rejected = false;
    try {
      MctsParameter parameter(28, 27, 0.0f, 1.6f, base, 0.0f);
    } catch (const std::invalid_argument& error) {
      parameter_rejected = true;
      assert(std::string(error.what()).find("pucb_constant_base") != std::string::npos);
    }
    assert(parameter_rejected);

    // Clean up previously created workers if player member construction fails.
    bool rejected = false;
    try {
      Player player(processor, 1, 16, 28, 27, 0.0f, 1, 1, 0, 1.6f, base, 0.0f);
    } catch (const std::invalid_argument& error) {
      rejected = true;
      assert(std::string(error.what()).find("pucb_constant_base") != std::string::npos);
    }
    assert(rejected);
  }

  // Start a valid search with the same processor after rejection.
  Player player(processor, 1, 16, 28, 27, 0.0f, 1, 1, 0, 1.6f, 0.5f, 0.0f);
  player.startEvaluation(false, 0, 1.0f, 0.0f);
  player.waitEvaluation(16, 5.0f, true);
  assert(player.getVisits() >= 16);
  assert(!player.getCandidates().empty());
}

/**
 * Check valid PUCB boundary values give finite priorities consistent with normal calculations.
 */
static void TestPucbPriority() {
  for (float base : {0.0f, -0.0f, -1.0f, -std::numeric_limits<float>::max(),
                     0.5f, 1.0f, 3200.0f, std::numeric_limits<float>::denorm_min(),
                     std::numeric_limits<float>::max()}) {
    for (float init : {0.0f, 1.6f}) {
      MctsManager manager(MctsParameter(28, 27, 0.0f, init, base, 0.5f));
      const float effective_base = manager.getParameter().getPucbConstantBase();
      assert(effective_base == (base <= 0.0f ? 1.0f : base));
      MctsNode* node = manager.createNode();
      node->initialize(
          "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", 512);
      node->_nodeValue = 0.25f;
      node->_mctsSelects.store(3, std::memory_order_relaxed);

      // Avoid NaN from zero times infinity and overflow at maximum visits.
      for (float probability : {0.0f, 0.25f}) {
        node->_probability = probability;
        for (int32_t total_visits : {0, 100, std::numeric_limits<int32_t>::max()}) {
          const auto priority = node->getPriorityByPUCB(total_visits, 4);
          assert(std::isfinite(priority.second));
          assert(priority.first == (3.0 < static_cast<double>(total_visits) / 8.0));

          // Compare with the original formula at wider precision to preserve normal-value
          // semantics.
          const long double total = total_visits;
          const long double increment = std::log(
              (1.0L + total + effective_base) / effective_base);
          const long double expected = -0.25L + init * (1.0L + increment) * probability *
              std::sqrt(total) / 4.0L;
          const long double tolerance = 1e-5L * std::max(1.0L, std::abs(expected));
          assert(std::abs(priority.second - expected) <= tolerance);
        }
      }
    }
  }
}

/**
 * Check MCTS settings are normalized at construction and nonfinite values are rejected.
 */
static void TestMctsParameterSettings() {
  // Preserve positive values and zero and inspect normalized penalties and PUCB constants.
  for (float value : {-1.0f, 0.0f, 0.25f, 2.0f}) {
    MctsParameter parameter(28, 27, value, value, 3200.0f, value);
    const float expected = value < 0.0f ? 0.0f : value;
    assert(parameter.getSennichitePenalty() == expected);
    assert(parameter.getPucbConstantInit() == expected);
    assert(parameter.getPucbMinVisitsRate() == expected);
  }

  // Reject negative infinity from the original argument even when normalized to zero.
  for (int32_t setting = 0; setting < 3; setting++) {
    for (float value : {std::numeric_limits<float>::quiet_NaN(),
                        std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity()}) {
      bool rejected = false;
      try {
        MctsParameter parameter(28, 27, setting == 0 ? value : 0.0f,
                                setting == 1 ? value : 1.6f, 3200.0f,
                                setting == 2 ? value : 0.0f);
      } catch (const std::invalid_argument&) {
        rejected = true;
      }
      assert(rejected);
    }
  }
}

/**
 * Check inference construction normalizes settings and standalone executors can be destroyed.
 * @param modelPath Small model path.
 * @param processor Existing inference processor.
 */
static void TestInferenceConstructorSettings(
    const std::string& modelPath, InferenceProcessor* processor) {
  Board board;
  board.initialize(
      "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1", 512);

  // Reject unavailable GPUs before loading model files.
  bool invalid_gpu = false;
  try {
    InferenceModel model("unused.model", std::numeric_limits<int32_t>::max(), false, true);
  } catch (const std::invalid_argument&) {
    invalid_gpu = true;
  }
  assert(invalid_gpu);

  // Preserve negative GPU IDs as CPU devices and normalized batch and thread counts.
  InferenceModel cpu_model(modelPath, -2, false, true);
  assert(cpu_model.getDevice().is_cpu());
  for (int32_t value : {std::numeric_limits<int32_t>::min(), -1, 0, 1, 2}) {
    const int32_t expected = value == 2 ? 2 : 1;
    {
      InferenceExecutor executor(processor, modelPath, -1, false, true, value, value);
      assert(executor._batchSize == expected);
      assert(executor._threads.size() == static_cast<size_t>(expected));
      assert(executor._batchFillRates.size() == static_cast<size_t>(expected));
      assert(executor._inputBuffers.front().size() == expected * MODEL_INPUT_PACK_SIZE);
      assert(executor._maskBuffers.front().size() == expected * MODEL_OUTPUT_PACK_SIZE);
      assert(executor._outputBuffers.front().size() == expected * MODEL_OUTPUT_SIZE);
      assert(std::isfinite(processor->predict(&board)));
    }
    assert(!processor->_terminated);
    assert(std::isfinite(processor->predict(&board)));
  }

  // Reject null processors and overflowing buffer sizes in the owning constructor body.
  for (bool null_processor : {false, true}) {
    bool rejected = false;
    try {
      InferenceExecutor executor(null_processor ? nullptr : processor, "unused.model", -1,
                                 false, true, std::numeric_limits<int32_t>::max(), 1);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  // Validate the processor's device list and total thread count before constructing executors.
  for (const auto& devices : {std::vector<int32_t>{}, std::vector<int32_t>{-1, -1}}) {
    bool rejected = false;
    try {
      InferenceProcessor invalid("unused.model", devices, false, true, 1,
                                 std::numeric_limits<int32_t>::max(), 0);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    assert(rejected);
  }

  // Run inference with normalized settings in the same process after initialization fails.
  InferenceProcessor corrected(modelPath, {-2}, false, true, 0, -1, 0);
  assert(corrected.getBatchSize() == 1 && corrected.getThreadSize() == 1);
  assert(std::isfinite(corrected.predict(&board)));
}

/**
 * Check later executor or model loading failures return without terminating the process.
 * @param modelPath Small model path.
 */
static void TestInferenceConstructionFailure(const std::string& modelPath) {
  // Fail an unavailable second GPU after the first CPU worker has started.
  auto later_failure = std::async(std::launch::async, [&modelPath]() {
    try {
      InferenceProcessor processor(modelPath, {-1, std::numeric_limits<int32_t>::max()},
                                    false, true, 1, 1, 0);
    } catch (const std::invalid_argument&) {
      return true;
    }
    return false;
  });
  assert(later_failure.wait_for(5s) == std::future_status::ready);
  assert(later_failure.get());

  // Return model file errors to the constructing caller rather than raising in a worker.
  bool missing_model = false;
  try {
    InferenceProcessor processor(modelPath + ".missing", {-1}, false, true, 1, 1, 0);
  } catch (const std::exception&) {
    missing_model = true;
  }
  assert(missing_model);
}

/**
 * Check player construction validates its evaluation queue limit and retains supported values.
 * @param processor Inference processor.
 */
static void TestEvaluationQueueSettings(InferenceProcessor* processor) {
  const int32_t saved_batch = processor->_batchSize;
  const int32_t saved_threads = processor->_threadSize;
  const int32_t maximum = std::numeric_limits<int32_t>::max();

  // Check multiplication boundaries through settings without allocating large buffers.
  for (const auto& sizes : {std::pair<int32_t, int32_t>{maximum, 1},
                            {1, maximum}, {maximum, maximum}}) {
    processor->_batchSize = sizes.first;
    processor->_threadSize = sizes.second;
    bool rejected = false;
    try {
      Player player(processor, 1, 1, 28, 27, 0.0f, 1, 1, 0, 1.6f, 3200.0f, 0.0f);
    } catch (const std::invalid_argument&) {
      rejected = true;
    }
    processor->_batchSize = saved_batch;
    processor->_threadSize = saved_threads;
    assert(rejected);
  }

  // Calculate valid limits once and retain them if the source settings change later.
  processor->_batchSize = maximum / 20;
  processor->_threadSize = 2;
  Player player(processor, 1, 1, 28, 27, 0.0f, 1, 1, 0, 1.6f, 3200.0f, 0.0f);
  processor->_batchSize = saved_batch;
  processor->_threadSize = saved_threads;
  assert(player._maxEvaluatingSize == (maximum / 20) * 20);
  player.startEvaluation(false, 0, 1.0f, 0.0f);
  player.waitEvaluation(1, 5.0f, true);
  assert(player.getVisits() == 1);
}

/**
 * Run native wait and notification regressions using the supplied model path.
 * @param argc Argument count.
 * @param argv Arguments; the first argument is the model path.
 * @return Exit code.
 */
int main(int argc, char** argv) {
  assert(argc == 2);
  InferenceProcessor processor(argv[1], {-1}, false, true, 1, 1, 0);
  TestMctsParameterSettings();
  TestInferenceConstructorSettings(argv[1], &processor);
  TestInferenceConstructionFailure(argv[1]);
  TestEvaluationQueueSettings(&processor);
  TestConstructorCorrections(&processor);
  TestPnSearchManagerAllocationFailure();
  TestFailedGetChildReturnsNode();
  TestFailedPlayKeepsTree(&processor);
  TestFailedPlayWhileSearching(&processor);
  TestStopDuringFailedPlay(&processor);
  TestPlayAllocationFailure(&processor);
  TestPucbBaseValidation(&processor);
  TestPucbPriority();
  TestFailedInitializeKeepsTree(&processor);
  TestFailedInitializeWhileSearching(&processor);
  TestInitializeAllocationFailure(&processor);
  TestMateDrawTurnLimits();
  TestSearchAfterRejectedMate(&processor);
  TestSennichitePenalty(&processor);
  TestMateBeforeVisit(&processor);
  TestNormalStop(&processor);
  TestStopAndRestart(&processor);
  TestLastVisitNotification(&processor);
  bool queue_only = TestQueueDiscardShutdown(&processor, false);
  bool running_task = TestQueueDiscardShutdown(&processor, true);
  if (!queue_only || !running_task) {
    return 1;
  }
  TestInferenceRegistrationFailure(&processor);
  TestInferenceWaiters(&processor);
  TestInferenceShutdown(&processor);
  TestQueuedCheckmateShutdown(&processor);
  {
    InferenceProcessor cached_processor(argv[1], {-1}, false, true, 1, 1, 64);
    TestInferenceCompletion(&cached_processor);
    TestSynchronousPrediction(&cached_processor);
  }
  std::cout << "Player wait native tests passed." << std::endl;
  return 0;
}

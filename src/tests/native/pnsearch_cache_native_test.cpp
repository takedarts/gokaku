#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

#include "BoardHash.h"
#include "PnSearchNode.h"
// Check capacity boundaries and generation number wraparound directly.
#define private public
#include "PnSearchEngine.h"
#undef private

using namespace deepshogi;

/** Check collisions, invalidation of all entries, and generation number wraparound. */
void TestCollisions() {
  PnSearchCache cache(4);
  std::array<uint64_t, 6> first{1, 2, 3, 4, 5, 6};
  std::array<uint64_t, 6> second = first;
  second[5] = 7;
  // Use the same hash to check full key comparison and wraparound from the array end.
  size_t slot = cache.Find(first, 7);
  cache.Insert(slot, first, 7, 0);
  size_t other = cache.Find(second, 7);
  assert(other != slot && cache.GetNodeIndex(other) == -1);
  cache.Insert(other, second, 7, 1);
  assert(cache.GetNodeIndex(cache.Find(first, 7)) == 0);
  assert(cache.GetNodeIndex(cache.Find(second, 7)) == 1);
  cache.Clear();
  assert(cache.GetNodeIndex(cache.Find(first, 7)) == -1);
  cache.Insert(cache.Find(first, 7), first, 7, 2);
  cache._generation = std::numeric_limits<uint32_t>::max();
  cache.Clear();
  assert(cache.GetNodeIndex(cache.Find(first, 7)) == -1);
  assert(cache.GetNodeIndex(cache.Find(second, 7)) == -1);

  // Distinguish differences in any key field even when the lookup hash is the same.
  PnSearchCache fields(8);
  fields.Insert(fields.Find(first, 0), first, 0, 0);
  for (size_t i = 0; i < first.size(); i++) {
    auto changed = first;
    changed[i] ^= 1;
    auto position = fields.Find(changed, 0);
    assert(fields.GetNodeIndex(position) == -1);
    fields.Insert(position, changed, 0, static_cast<uint32_t>(i + 1));
    assert(fields.GetNodeIndex(fields.Find(first, 0)) == 0);
  }
}

/** Check the contracts of node capacity and cache bypassing. */
void TestNodeCapacity() {
  Board board;
  board.initialize("4k4/9/4K4/9/9/9/9/9 b G 1", 0x7fff);
  PnSearchEngine empty(0);
  assert(empty._getNode(&board, 1, false) == nullptr);
  PnSearchEngine engine(2);
  auto* first = engine._getNode(&board, 1, false);
  auto* clone = engine._getNode(&board, 1, true);
  assert(first != nullptr && clone != nullptr && first != clone);
  // The cache for the same position can be reused even after the capacity is exhausted.
  assert(engine._getNode(&board, 3, false) == first);
  assert(engine._getNode(&board, 1, true) == nullptr);
  Board other;
  other.initialize("4k4/9/4K4/9/9/9/9/9 b S 1", 0x7fff);
  assert(engine._getNode(&other, 1, false) == nullptr);
  // Clones of unregistered positions are not published to the cache either.
  PnSearchEngine uncached(2);
  auto* detached = uncached._getNode(&board, 1, true);
  auto* registered = uncached._getNode(&board, 1, false);
  assert(detached != registered);
  assert(uncached._getNode(&board, 3, false) == registered);
}

/** Check key information retention and table behavior. */
int main() {
  TestCollisions();
  TestNodeCapacity();
  std::cout << "PN search cache: OK\n";
  return 0;
}

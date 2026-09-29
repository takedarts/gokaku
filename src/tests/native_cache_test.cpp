#include <cassert>

#include "Board.h"
#include "BoardHash.h"
#include "InferenceCache.h"
#include "PnSearchCache.h"

using namespace deepshogi;

/** Run cache key, eviction, and collision regressions; return int exit status. */
int main() {
  // Equal positions with different rule settings must use different inference keys.
  const std::string sfen = "4k4/9/9/9/9/9/9/9/4K4 b R 1";
  Board first(28, 27, 20), second(31, 31, 20), third(28, 27, 21);
  first.initialize(sfen);
  second.initialize(sfen);
  third.initialize(sfen);
  InferenceHash a(&first), b(&second), c(&third);
  assert((a < b || b < a) && (a < c || c < a));
  assert(BoardHash(&first).getCacheKey() == BoardHash(&second).getCacheKey());

  // Draw distances greater than fifty share the same normalized input and key.
  Board distant(28, 27, 200), more_distant(28, 27, 300);
  distant.initialize(sfen);
  more_distant.initialize(sfen);
  InferenceHash d(&distant), e(&more_distant);
  assert(!(d < e) && !(e < d));

  // Access the oldest item, then verify that LRU eviction retains it.
  InferenceCache cache(2);
  InferenceResult result, found;
  result.value = 0.25f;
  result.remainingTurns = 12.0f;
  cache.put(a, result);
  cache.put(b, result);
  assert(cache.get(a, found));
  assert(found.value == result.value && found.remainingTurns == result.remainingTurns);
  cache.put(c, result);
  assert(!cache.get(b, found));
  assert(cache.get(a, found) && cache.get(c, found));
  assert(cache.getHitRate() > 0.0f);

  // Duplicate inserts preserve the original result, and capacity zero disables caching.
  result.value = -0.5f;
  cache.put(a, result);
  assert(cache.get(a, found) && found.value == 0.25f);
  InferenceCache disabled(0);
  disabled.put(a, result);
  assert(!disabled.get(a, found));

  // Force equal lookup hashes to verify full-key comparison and linear probing.
  PnSearchCache pn_cache(2);
  auto first_key = BoardHash(&first).getCacheKey();
  auto second_key = first_key;
  second_key[1] ^= 1;
  uint64_t hash = PnSearchCache::Hash(first_key);
  auto first_slot = pn_cache.Find(first_key, hash);
  assert(pn_cache.GetNodeIndex(first_slot) == -1);
  pn_cache.Insert(first_slot, first_key, hash, 0);
  auto second_slot = pn_cache.Find(second_key, hash);
  assert(first_slot != second_slot);
  pn_cache.Insert(second_slot, second_key, hash, 1);
  assert(pn_cache.GetNodeIndex(pn_cache.Find(first_key, hash)) == 0);
  assert(pn_cache.GetNodeIndex(pn_cache.Find(second_key, hash)) == 1);
  pn_cache.Clear();
  assert(pn_cache.GetNodeIndex(pn_cache.Find(first_key, hash)) == -1);
  pn_cache.Insert(pn_cache.Find(second_key, hash), second_key, hash, 0);
  assert(pn_cache.GetNodeIndex(pn_cache.Find(second_key, hash)) == 0);
  return 0;
}

#include "PnSearchCache.h"

#include <algorithm>
#include <bit>

namespace deepshogi {

/**
 * Hash the board and hand pieces for cache lookup.
 * @param key const std::array<uint64_t, 6>& position key.
 * @return uint64_t lookup hash.
 */
uint64_t PnSearchCache::Hash(const std::array<uint64_t, 6>& key) {
  // Mix positions with different hand pieces and use the low bits as the slot index.
  uint64_t value = key[0] ^ std::rotl(key[3], 17) ^ std::rotl(key[4], 39);

  // Spread differences in the upper bits into the lower bits.
  value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
  value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
  return value ^ (value >> 31);
}

/**
 * Create a cache with at least twice as many slots as its entry capacity.
 * @param capacity uint32_t maximum entry count.
 */
PnSearchCache::PnSearchCache(uint32_t capacity)
    : _entries(std::bit_ceil(std::max(size_t(1), size_t(capacity) * 2))),
      _keys(capacity),
      _generation(1) {}

/**
 * Invalidate all registered entries.
 */
void PnSearchCache::Clear() {
  // Reinitialize all slots only when the generation counter wraps around.
  if (++_generation == 0) {
    for (Entry& entry : _entries) {
      entry.generation = 0;
    }
    _generation = 1;
  }
}

/**
 * Return the matching slot or the first empty slot.
 * @param key const std::array<uint64_t, 6>& position key.
 * @param hash uint64_t lookup hash.
 * @return size_t slot index; never insert more entries than the capacity.
 */
size_t PnSearchCache::Find(
    const std::array<uint64_t, 6>& key, uint64_t hash) const {
  // Use the low hash bits as the initial lookup slot.
  size_t mask = _entries.size() - 1;
  size_t slot = hash & mask;

  // With no individual deletions, reaching an empty slot proves the key is absent.
  while (_entries[slot].generation == _generation) {
    const Entry& entry = _entries[slot];
    if (entry.hash == hash && _keys[entry.nodeIndex] == key) {
      return slot;
    }
    slot = (slot + 1) & mask;
  }
  return slot;
}

/**
 * Insert the position key and search node index
 * into the empty slot found during lookup.
 * @param slot size_t empty slot index.
 * @param key const std::array<uint64_t, 6>& position key.
 * @param hash uint64_t lookup hash.
 * @param nodeIndex uint32_t search node index.
 * @return void
 */
void PnSearchCache::Insert(
    size_t slot, const std::array<uint64_t, 6>& key,
    uint64_t hash, uint32_t nodeIndex) {
  // Save the position key associated with the search node index.
  _keys[nodeIndex] = key;

  // Activate the entry after initializing the node and storing its key.
  Entry& entry = _entries[slot];
  entry.hash = hash;
  entry.nodeIndex = nodeIndex;
  entry.generation = _generation;
}

}  // namespace deepshogi

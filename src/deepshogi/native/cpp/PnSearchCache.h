#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace deepshogi {

/**
 * Fixed-capacity PN search cache.
 * Do not delete individual entries or grow the cache during search.
 */
class PnSearchCache {
 private:
  /**
   * Store a lookup hash, node index, and valid generation.
   */
  struct Entry {
    /** Lookup hash computed from the position key. */
    uint64_t hash = 0;

    /** Search node index corresponding to the position key. */
    uint32_t nodeIndex = 0;

    /** Generation in which this slot is valid. */
    uint32_t generation = 0;
  };

 public:
  /**
   * Hash the board and hand pieces for cache lookup.
   * @param key const std::array<uint64_t, 6>& position key.
   * @return uint64_t lookup hash.
   */
  static uint64_t Hash(const std::array<uint64_t, 6>& key);

  /**
   * Create a cache with at least twice as many slots as its entry capacity.
   * @param capacity uint32_t maximum entry count.
   */
  explicit PnSearchCache(uint32_t capacity);

  /**
   * Invalidate all registered entries.
   */
  void Clear();

  /**
   * Return the matching slot or the first empty slot.
   * @param key const std::array<uint64_t, 6>& position key.
   * @param hash uint64_t lookup hash.
   * @return size_t slot index; never insert more entries than the capacity.
   */
  size_t Find(const std::array<uint64_t, 6>& key, uint64_t hash) const;

  /**
   * Return the search node index registered in a slot.
   * @param slot size_t slot index.
   * @return int32_t node index, or -1 for an empty slot.
   */
  inline int32_t GetNodeIndex(size_t slot) const {
    const Entry& entry = _entries[slot];
    return entry.generation == _generation
               ? static_cast<int32_t>(entry.nodeIndex)
               : -1;
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
  void Insert(
      size_t slot, const std::array<uint64_t, 6>& key,
      uint64_t hash, uint32_t nodeIndex);

 private:
  /**
   * Slots holding lookup hashes, node indices, and valid generations.
   */
  std::vector<Entry> _entries;

  /**
   * Position keys indexed by search node index.
   */
  std::vector<std::array<uint64_t, 6>> _keys;

  /**
   * Generation identifying currently valid entries.
   */
  uint32_t _generation;
};

}  // namespace deepshogi

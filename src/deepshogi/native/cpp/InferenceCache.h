#pragma once

#include <atomic>
#include <cstdint>
#include <list>
#include <map>
#include <mutex>

#include "InferenceHash.h"
#include "InferenceResult.h"

namespace deepshogi {

/**
 * Hold an inference cache entry.
 */
class InferenceCacheEntry {
 public:
  /**
   * Construct an inference cache entry.
   * @param result Inference result.
   * @param orderIterator Iterator into the cache usage-order list.
   */
  InferenceCacheEntry(
      const InferenceResult& result,
      std::list<InferenceHash>::iterator orderIterator);

  /**
   * Return the inference result.
   * @return Inference result.
   */
  inline const InferenceResult& getResult() const {
    return _result;
  }

  /**
   * Return the iterator into the cache usage-order list.
   * @return Iterator into the cache usage-order list.
   */
  inline std::list<InferenceHash>::iterator getOrderIterator() const {
    return _orderIterator;
  }

  /**
   * Set the iterator into the cache usage-order list.
   * @param orderIterator Iterator into the cache usage-order list.
   */
  inline void setOrderIterator(
      std::list<InferenceHash>::iterator orderIterator) {
    _orderIterator = orderIterator;
  }

 private:
  /**
   * Inference result.
   */
  InferenceResult _result;

  /**
   * Iterator into the cache usage-order list.
   */
  std::list<InferenceHash>::iterator _orderIterator;
};

/**
 * Cache inference results using least-recently-used eviction.
 */
class InferenceCache {
 public:
  /**
   * Construct the inference result cache.
   * @param cacheSize Maximum number of cached inference results.
   */
  explicit InferenceCache(int32_t cacheSize);

  /**
   * Destroy the inference result cache.
   */
  virtual ~InferenceCache() = default;

  /**
   * Retrieve the inference result for the specified hash.
   * Move a matching entry to the most recently used position.
   * @param inferenceHash Hash used to look up the inference result.
   * @param result Object receiving the cached inference result.
   * @return True if a cached result was found.
   */
  bool get(const InferenceHash& inferenceHash, InferenceResult& result);

  /**
   * Cache the inference result for the specified hash.
   * Do nothing if the hash is already registered.
   * @param inferenceHash Hash under which to cache the inference result.
   * @param result Inference result to cache.
   */
  void put(const InferenceHash& inferenceHash, const InferenceResult& result);

  /**
   * Return the inference result cache hit rate.
   * @return Inference result cache hit rate.
   */
  inline float getHitRate() const {
    return _hitRate.load(std::memory_order_relaxed);
  }

 private:
  /**
   * Update the cache hit rate.
   * Call this method while holding _mutex.
   * @param cacheHit True if a cached entry was found.
   */
  void updateHitRate(bool cacheHit);

  /**
   * Mutex protecting the cache.
   */
  std::mutex _mutex;

  /**
   * Maximum number of cached inference results.
   */
  int32_t _cacheSize;

  /**
   * Cache usage order.
   * The front is most recently used; the back is least recently used.
   */
  std::list<InferenceHash> _order;

  /**
   * Inference result cache.
   * Keys are position hashes; values hold results and usage-order iterators.
   */
  std::map<InferenceHash, InferenceCacheEntry> _entries;

  /**
   * Inference result cache hit rate.
   */
  std::atomic<float> _hitRate;
};

}  // namespace deepshogi

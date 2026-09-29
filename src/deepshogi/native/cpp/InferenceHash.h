#pragma once

#include <cstdint>

#include "BoardHash.h"

namespace deepshogi {

/**
 * Hold a position hash used for caching inference results.
 */
class InferenceHash {
 public:
  /**
   * Create a hash for caching inference results.
   * @param board Board object.
   */
  InferenceHash(const Board* board);

  /**
   * Copy an inference cache hash.
   * @param other Source hash.
   */
  InferenceHash(const InferenceHash& other) = default;

  /**
   * Destroy the inference cache hash.
   */
  virtual ~InferenceHash() = default;

  /**
   * Compare whether this object is less than the specified object.
   * @param other InferenceHash object to compare.
   * @return True if this object is less than the specified object.
   */
  inline bool operator<(const InferenceHash& other) const {
    if (_boardHash < other._boardHash) {
      return true;
    } else if (other._boardHash < _boardHash) {
      return false;
    } else if (_nyugyokuScores[0] != other._nyugyokuScores[0]) {
      return _nyugyokuScores[0] < other._nyugyokuScores[0];
    } else if (_nyugyokuScores[1] != other._nyugyokuScores[1]) {
      return _nyugyokuScores[1] < other._nyugyokuScores[1];
    } else if (_remainingTurn != other._remainingTurn) {
      return _remainingTurn < other._remainingTurn;
    } else {
      return false;
    }
  }

 private:
  /**
   * Position hash shared with repetition detection.
   */
  BoardHash _boardHash;

  /**
   * Points required for entering-king declaration (0: black, 1: white).
   */
  int8_t _nyugyokuScores[2];

  /**
   * Remaining moves until a draw, clamped to 0 through 50.
   */
  int8_t _remainingTurn;
};

}  // namespace deepshogi

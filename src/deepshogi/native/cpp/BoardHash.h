#pragma once

#include <cstdint>

#include "BitBoard.h"
#include "Board.h"

namespace deepshogi {

/**
 * 盤面のハッシュ値を保持するクラス。
 */
class BoardHash {
 public:
  /**
   * 盤面のハッシュ値を保持するオブジェクトを作成する。
   * @param board 盤面オブジェクト
   */
  BoardHash(const Board* board);

  /**
   * 盤面のハッシュ値を保持するオブジェクトを複製する。
   * @param other 複製元の盤面のハッシュ値を保持するオブジェクト
   */
  BoardHash(const BoardHash& other) = default;

  /**
   * 盤面のハッシュ値を保持するオブジェクトを破棄する。
   */
  virtual ~BoardHash() = default;

  /**
   * このオブジェクトが指定されたオブジェクトより大きいかどうかを比較する。
   * @param other 比較対象のBoardHashオブジェクト
   * @return このオブジェクトが指定されたオブジェクトより小さい場合はtrue
   */
  inline bool operator<(const BoardHash& other) const {
    if (_cellHash != other._cellHash) {
      return _cellHash < other._cellHash;
    } else if (_colorBitBoards[0] != other._colorBitBoards[0]) {
      return _colorBitBoards[0] < other._colorBitBoards[0];
    } else if (_colorBitBoards[1] != other._colorBitBoards[1]) {
      return _colorBitBoards[1] < other._colorBitBoards[1];
    } else if (_handBits[0] != other._handBits[0]) {
      return _handBits[0] < other._handBits[0];
    } else if (_handBits[1] != other._handBits[1]) {
      return _handBits[1] < other._handBits[1];
    } else {
      return false;
    }
  }

 private:
  /**
   * 盤面上の駒の配置を表すハッシュ値。
   */
  uint64_t _cellHash;

  /**
   * 駒の配置を表すビットボード。
   */
  BitBoard _colorBitBoards[2];

  /**
   * 持ち駒の数を表すビット表現。
   */
  uint64_t _handBits[2];
};

}  // namespace deepshogi

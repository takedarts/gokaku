#include "BoardHash.h"

namespace deepshogi {

/**
 * 盤面のハッシュ値を保持するオブジェクトを作成する。
 * @param board 盤面オブジェクト
 */
BoardHash::BoardHash(const Board* board) {
  _cellHash = board->_cellHash;
  _colorBitBoards[0] = board->_colorBitBoards[0];
  _colorBitBoards[1] = board->_colorBitBoards[1];
  _handBits[0] = board->_handBits[0];
  _handBits[1] = board->_handBits[1];

  if (board->_color == COLOR_WHITE) {
    _cellHash ^= 0xffffffffffffffffULL;
  }
}

}  // namespace deepshogi

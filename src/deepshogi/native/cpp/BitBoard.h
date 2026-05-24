#pragma once

#include <bit>
#include <cstdint>
#include <ostream>
#include <string>

namespace deepshogi {

/**
 * 盤面の状態をビット列で表現し、効率的な計算を行うためのクラス。
 * 各ビットは盤面の特定の位置を表し、駒の存在や種類を示すために使用する。
 * 盤面の右上を0ビット目とし、縦横の順でビットが割り当てられる。
 * 例えば、9x9の将棋盤の場合、0ビット目は右上のマスを表し、8ビット目は右下のマスを表す。
 * 将棋盤を想定しているため、以下の2つの変数を使用してビット状態を管理する。
 *  - lower (int64_t): 盤面の下位ビット（0-53ビット目）を表すビット列。
 *  - upper (int32_t): 盤面の上位ビット（54-80ビット目）を表すビット列。
 */
class BitBoard {
 public:
  /**
   * 全てのビットを0に設定したオブジェクトを生成する。
   */
  BitBoard();

  /**
   * 指定されたビット列を設定したオブジェクトを生成する。
   * @param lower 盤面の下位ビットを表すビット列。
   * @param upper 盤面の上位ビットを表すビット列。
   */
  BitBoard(uint64_t lower, uint32_t upper);

  /**
   * 指定された位置にビットを立てたオブジェクトを生成する。
   * @param index ビットを立てる位置を表す整数値
   */
  BitBoard(int8_t index);

  /**
   * 指定されたビット列をコピーしてオブジェクトを生成する。
   */
  BitBoard(const BitBoard& other) = default;

  /**
   * オブジェクトを破棄する。
   */
  virtual ~BitBoard() = default;

  /**
   * 指定された位置にビットを立てる。
   * @param index ビットを立てる位置を表す整数値
   */
  void setBit(int8_t index);

  /**
   * 指定された位置のビットをクリアする。
   * @param index ビットをクリアする位置を表す整数値
   */
  void clearBit(int8_t index);

  /**
   * 指定された位置のビットが立っているかを返す。
   * @param index ビットの状態を確認する位置を表す整数値
   * @return 指定された位置のビットが立っている場合はtrue
   */
  bool hasBit(int8_t index) const;

  /**
   * 最も左のビットが立っている位置を返す。
   * ビット列が全て0である場合は-1を返す。
   * @return 最も左のビットが立っている位置を表す整数値
   */
  int8_t getLeftmostBitIndex() const;

  /**
   * 最も右のビットが立っている位置を返す。
   * ビット列が全て0である場合は-1を返す。
   * @return 最も右のビットが立っている位置を表す整数値
   */
  int8_t getRightmostBitIndex() const;

  /**
   * 最も右のビットが立っている位置をクリアし、その位置を返す。
   * ビット列が全て0である場合は-1を返す。
   * @return 最も右のビットが立っている位置を表す整数値
   */
  int8_t popRightmostBitIndex();

  /**
   * ビット列を文字列形式で表現する。
   * @return ビット列を表す文字列
   */
  std::string toString() const;

  /**
   * ビット列の論理否定を計算した結果を返す。
   * @return 論理否定を取った結果のBitBoardオブジェクト
   */
  BitBoard operator~() const;

  /**
   * ビット列を左にシフトしてこのオブジェクトに代入する。
   * @param shift シフトするビット数
   * @return 左シフトした結果のBitBoardオブジェクトへの参照
   */
  BitBoard& operator<<=(int32_t shift);

  /**
   * ビット列を右にシフトしてこのオブジェクトに代入する。
   * @param shift シフトするビット数
   * @return 右シフトした結果のBitBoardオブジェクトへの参照
   */
  BitBoard& operator>>=(int32_t shift);

  /**
   * 全てのビットを0にリセットする。
   */
  inline void clearAll() {
    _lower = 0;
    _upper = 0;
  }

  /**
   * 盤面を表すビット列の下位部分を返す。
   * @return 盤面の下位ビット
   */
  inline uint64_t getLower() const {
    return _lower;
  }

  /**
   * 盤面を表すビット列の上位部分を返す。
   * @return 盤面の上位ビット
   */
  inline uint32_t getUpper() const {
    return _upper;
  }

  /**
   * ビット列が立っているビットの数を返す。
   * @return ビット列が立っているビットの数を表す整数値
   */
  inline int8_t countBit() const {
    return std::popcount(_lower) + std::popcount(_upper);
  }

  /**
   * ビット列のいずれかのビットが立っているならばtrueを返す。
   * @return ビット列のいずれかのビットが立っている場合はtrue
   */
  explicit inline operator bool() const {
    return _lower != 0 || _upper != 0;
  }

  /**
   * ビット列が等しいならばtrueを返す。
   */
  inline bool operator==(const BitBoard& other) const {
    return _lower == other._lower && _upper == other._upper;
  }

  /**
   * ビット列が異なるならばtrueを返す。
   */
  inline bool operator!=(const BitBoard& other) const {
    return !(*this == other);
  }

  /**
   * ビット列の論理積をこのオブジェクトに代入する。
   * @param other 論理積を取る相手のBitBoardオブジェクト
   * @return 論理積を取った結果のBitBoardオブジェクトへの参照
   */
  inline BitBoard& operator&=(const BitBoard& other) {
    _lower &= other._lower;
    _upper &= other._upper;
    return *this;
  }

  /**
   * ビット列の論理和をこのオブジェクトに代入する。
   * @param other 論理和を取る相手のBitBoardオブジェクト
   * @return 論理和を取った結果のBitBoardオブジェクトへの参照
   */
  inline BitBoard& operator|=(const BitBoard& other) {
    _lower |= other._lower;
    _upper |= other._upper;
    return *this;
  }

  /**
   * ビット列の排他的論理和をこのオブジェクトに代入する。
   * @param other 排他的論理和を取る相手のBitBoardオブジェクト
   * @return 排他的論理和を取った結果のBitBoardオブジェクトへの参照
   */
  inline BitBoard& operator^=(const BitBoard& other) {
    _lower ^= other._lower;
    _upper ^= other._upper;
    return *this;
  }

  /**
   * ビット列の論理積を返す。
   * @param other 論理積を取る相手のBitBoardオブジェクト
   * @return 論理積を取った結果のBitBoardオブジェクト
   */
  inline BitBoard operator&(const BitBoard& other) const {
    return BitBoard(_lower & other._lower, _upper & other._upper);
  }

  /**
   * ビット列の論理和を返す。
   * @param other 論理和を取る相手のBitBoardオブジェクト
   * @return 論理和を取った結果のBitBoardオブジェクト
   */
  inline BitBoard operator|(const BitBoard& other) const {
    return BitBoard(_lower | other._lower, _upper | other._upper);
  }

  /**
   * ビット列の排他的論理和を返す。
   * @param other 排他的論理和を取る相手のBitBoardオブジェクト
   * @return 排他的論理和を取った結果のBitBoardオブジェクト
   */
  inline BitBoard operator^(const BitBoard& other) const {
    return BitBoard(_lower ^ other._lower, _upper ^ other._upper);
  }

  /**
   * ビット列を左にシフトした結果を返す。
   * @param shift シフトするビット数
   * @return 左シフトした結果のBitBoardオブジェクト
   */
  inline BitBoard operator<<(int32_t shift) const {
    BitBoard result(*this);
    result <<= shift;
    return result;
  }

  /**
   * ビット列を右にシフトした結果を返す。
   * @param shift シフトするビット数
   * @return 右シフトした結果のBitBoardオブジェクト
   */
  inline BitBoard operator>>(int32_t shift) const {
    BitBoard result(*this);
    result >>= shift;
    return result;
  }

  /**
   * ビット列の大小関係を比較する。
   * ビット列の上位部分を優先して比較し、上位部分が等しい場合は下位部分を比較する。
   * @param other 比較対象のBitBoardオブジェクト
   * @return ビット列がこのオブジェクトのビット列より小さい場合はtrue
   */
  inline bool operator<(const BitBoard& other) const {
    if (_upper != other._upper) {
      return _upper < other._upper;
    } else {
      return _lower < other._lower;
    }
  }

  /**
   * ビット列の文字列表現を出力ストリームに書き込む。
   * @param os 出力ストリーム
   * @param board ビットボードオブジェクト
   * @return 出力ストリーム
   */
  friend std::ostream& operator<<(std::ostream& os, const BitBoard& bitboard) {
    os << bitboard.toString();
    return os;
  }

 private:
  /**
   * 盤面の下位ビットを表すビット列。
   */
  uint64_t _lower;

  /**
   * 盤面の上位ビットを表すビット列。
   */
  uint32_t _upper;
};

}  // namespace deepshogi

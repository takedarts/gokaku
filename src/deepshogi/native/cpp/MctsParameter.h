#pragma once

#include <cstdint>

namespace deepshogi {

/**
 * MCTSの探索に関するパラメータを管理するクラス。
 */
class MctsParameter {
 public:
  /**
   * パラメータオブジェクトを作成する。
   * @param nyugyokuScoreBlack 先手番の入玉宣言に必要となる点数
   * @param nyugyokuScoreWhite 後手番の入玉宣言に必要となる点数
   * @param drawTurn 引き分けとなるまでの手数
   * @param checkSearchDepth 詰み手筋の探索深さ
   * @param checkSearchNode 詰み手筋の探索ノード数
   * @param ucbConstant UCBの信頼上限に掛ける定数
   * @param pucbConstantInit PUCBの信頼上限に掛ける定数の初期値
   * @param pucbConstantBase PUCBの信頼上限に掛ける定数の変化値
   */
  MctsParameter(
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
      float ucbConstant, float pucbConstantInit, float pucbConstantBase);

  /**
   * パラメータオブジェクトをコピーする。
   * @param other コピー元のパラメータオブジェクト
   */
  MctsParameter(const MctsParameter& other) = default;

  /**
   * パラメータオブジェクトを破棄する。
   */
  virtual ~MctsParameter() = default;

  /**
   * 先手番の入玉宣言に必要となる点数を取得する。
   * @return 先手番の入玉宣言に必要となる点数
   */
  inline int32_t getNyugyokuScoreBlack() const {
    return _nyugyokuScoreBlack;
  }

  /**
   * 後手番の入玉宣言に必要となる点数を取得する。
   * @return 後手番の入玉宣言に必要となる点数
   */
  inline int32_t getNyugyokuScoreWhite() const {
    return _nyugyokuScoreWhite;
  }

  /**
   * 引き分けとなるまでの手数を取得する。
   * @return 引き分けとなるまでの手数
   */
  inline int32_t getDrawTurn() const {
    return _drawTurn;
  }

  /**
   * UCBの信頼上限に掛ける定数を取得する。
   * @return UCBの信頼上限に掛ける定数
   */
  inline float getUcbConstant() const {
    return _ucbConstant;
  }

  /**
   * PUCBの信頼上限に掛ける定数の初期値を取得する。
   * @return PUCBの信頼上限に掛ける定数の初期値
   */
  inline float getPucbConstantInit() const {
    return _pucbConstantInit;
  }

  /**
   * PUCBの信頼上限に掛ける定数の変化値を取得する。
   * @return PUCBの信頼上限に掛ける定数の変化値
   */
  inline float getPucbConstantBase() const {
    return _pucbConstantBase;
  }

 private:
  /**
   * 先手番の入玉宣言に必要となる点数。
   */
  int32_t _nyugyokuScoreBlack;

  /**
   * 後手番の入玉宣言に必要となる点数。
   */
  int32_t _nyugyokuScoreWhite;

  /**
   * 引き分けとなるまでの手数。
   */
  int32_t _drawTurn;

  /**
   * UCBの信頼上限に掛ける定数。
   */
  float _ucbConstant;

  /**
   * PUCBの信頼上限に掛ける定数の初期値。
   */
  float _pucbConstantInit;

  /**
   * PUCBの信頼上限に掛ける定数の変化値。
   */
  float _pucbConstantBase;
};

}  // namespace deepshogi

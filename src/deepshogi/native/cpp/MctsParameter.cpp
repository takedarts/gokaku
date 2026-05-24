#include "MctsParameter.h"

namespace deepshogi {

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
MctsParameter::MctsParameter(
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
    float ucbConstant, float pucbConstantInit, float pucbConstantBase)
    : _nyugyokuScoreBlack(nyugyokuScoreBlack),
      _nyugyokuScoreWhite(nyugyokuScoreWhite),
      _drawTurn(drawTurn),
      _ucbConstant(ucbConstant),
      _pucbConstantInit(pucbConstantInit),
      _pucbConstantBase(pucbConstantBase) {
}

}  // namespace deepshogi

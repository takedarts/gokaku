#include "NodeParameter.h"

namespace deepshogi {

/**
 * Create a parameter object.
 * @param processor Object that performs inference
 * @param cacheSize Cache size for evaluation results
 * @param nyugyokuScoreBlack Points required for black's entering king declaration
 * @param nyugyokuScoreWhite Points required for white's entering king declaration
 * @param drawTurn Number of moves until a draw
 * @param ucbConstant Constant multiplied to UCB upper confidence bound
 * @param pucbConstantInit Initial value applied to PUCB upper confidence bound
 * @param pucbConstantBase Base value applied to PUCB upper confidence bound
 */
NodeParameter::NodeParameter(
    Processor* processor, int32_t cacheSize,
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
    float ucbConstant, float pucbConstantInit, float pucbConstantBase)
    : _processor(processor),
      _cacheSize(cacheSize),
      _nyugyokuScoreBlack(nyugyokuScoreBlack),
      _nyugyokuScoreWhite(nyugyokuScoreWhite),
      _drawTurn(drawTurn),
      _ucbConstant(ucbConstant),
      _pucbConstantInit(pucbConstantInit),
      _pucbConstantBase(pucbConstantBase) {
}

}  // namespace deepshogi

#include "MctsParameter.h"

namespace deepshogi {

/**
 * Creates a parameter object.
 * @param nyugyokuScoreBlack Score required for black's entering-king declaration
 * @param nyugyokuScoreWhite Score required for white's entering-king declaration
 * @param drawTurn Number of moves until a draw
 * @param pucbConstantInit Initial value of the constant multiplied by the PUCB confidence upper bound
 * @param pucbConstantBase Rate-of-change value of the constant multiplied by the PUCB confidence upper bound
 */
MctsParameter::MctsParameter(
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
    float pucbConstantInit, float pucbConstantBase)
    : _nyugyokuScoreBlack(nyugyokuScoreBlack),
      _nyugyokuScoreWhite(nyugyokuScoreWhite),
      _drawTurn(drawTurn),
      _pucbConstantInit(pucbConstantInit),
      _pucbConstantBase(pucbConstantBase) {
}

}  // namespace deepshogi

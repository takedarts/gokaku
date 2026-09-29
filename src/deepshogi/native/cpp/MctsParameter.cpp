#include "MctsParameter.h"

namespace deepshogi {

/**
 * Create the parameter object.
 * @param nyugyokuScoreBlack Points required for Black's entering-king declaration.
 * @param nyugyokuScoreWhite Points required for White's entering-king declaration.
 * @param drawTurn Move count at which the game is drawn.
 * @param sennichitePenalty Penalty assigned to repetition evaluations.
 * @param pucbConstantInit Initial PUCB exploration coefficient.
 * @param pucbConstantBase Base controlling the PUCB exploration coefficient.
 * @param pucbMinVisitsRate Minimum visit ratio for prioritizing PUCB children.
 */
MctsParameter::MctsParameter(
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
    float sennichitePenalty, float pucbConstantInit, float pucbConstantBase,
    float pucbMinVisitsRate)
    : _nyugyokuScoreBlack(nyugyokuScoreBlack),
      _nyugyokuScoreWhite(nyugyokuScoreWhite),
      _drawTurn(drawTurn),
      _sennichitePenalty(sennichitePenalty),
      _pucbConstantInit(pucbConstantInit),
      _pucbConstantBase(pucbConstantBase),
      _pucbMinVisitsRate(pucbMinVisitsRate) {
}

}  // namespace deepshogi

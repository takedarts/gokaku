#include "MctsParameter.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace deepshogi {

/**
 * Create the parameter object.
 * @param sennichitePenalty Penalty assigned to repetition evaluations.
 * @param pucbConstantInit Initial PUCB exploration coefficient.
 * @param pucbConstantBase Base controlling the PUCB exploration coefficient.
 * @param pucbMinVisitsRate Minimum visit ratio for prioritizing PUCB children.
 */
MctsParameter::MctsParameter(
    float sennichitePenalty, float pucbConstantInit,
    float pucbConstantBase, float pucbMinVisitsRate)
    : _sennichitePenalty(std::max(sennichitePenalty, 0.0f)),
      _pucbConstantInit(std::max(pucbConstantInit, 0.0f)),
      _pucbConstantBase(pucbConstantBase > 0.0f ? pucbConstantBase : 1.0f),
      _pucbMinVisitsRate(std::max(pucbMinVisitsRate, 0.0f)) {
  if (!std::isfinite(pucbConstantBase)) {
    throw std::invalid_argument("pucb_constant_base must be finite.");
  }

  if (!std::isfinite(sennichitePenalty)) {
    throw std::invalid_argument("sennichite_penalty must be finite.");
  }

  if (!std::isfinite(pucbConstantInit)) {
    throw std::invalid_argument("pucb_constant_init must be finite.");
  }

  if (!std::isfinite(pucbMinVisitsRate)) {
    throw std::invalid_argument("pucb_min_visits_rate must be finite.");
  }
}

}  // namespace deepshogi

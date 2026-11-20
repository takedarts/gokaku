#pragma once

namespace deepshogi {

/**
 * A class for managing parameters related to MCTS search.
 */
class MctsParameter {
 public:
  /**
   * Create the parameter object.
   * @param sennichitePenalty Penalty assigned to repetition evaluations.
   * @param pucbConstantInit Initial PUCB exploration coefficient.
   * @param pucbConstantBase Base controlling the PUCB exploration coefficient.
   * @param pucbMinVisitsRate Minimum visit ratio for prioritizing PUCB children.
   */
  MctsParameter(
      float sennichitePenalty, float pucbConstantInit,
      float pucbConstantBase, float pucbMinVisitsRate);

  /**
   * Copy the parameter object.
   * @param other Source parameter object.
   */
  MctsParameter(const MctsParameter& other) = default;

  /**
   * Destroys the parameter object.
   */
  virtual ~MctsParameter() = default;

  /**
   * Return the penalty assigned to repetition evaluations.
   * @return Penalty assigned to repetition evaluations.
   */
  inline float getSennichitePenalty() const {
    return _sennichitePenalty;
  }

  /**
   * Return the initial PUCB exploration coefficient.
   * @return Initial PUCB exploration coefficient.
   */
  inline float getPucbConstantInit() const {
    return _pucbConstantInit;
  }

  /**
   * Return the base controlling the PUCB exploration coefficient.
   * @return Base controlling the PUCB exploration coefficient.
   */
  inline float getPucbConstantBase() const {
    return _pucbConstantBase;
  }

  /**
   * Return the minimum visit ratio for prioritizing PUCB children.
   * @return Minimum visit ratio for prioritizing PUCB children.
   */
  inline float getPucbMinVisitsRate() const {
    return _pucbMinVisitsRate;
  }

 private:
  /**
   * Penalty assigned to repetition evaluations.
   */
  float _sennichitePenalty;

  /**
   * Initial PUCB exploration coefficient.
   */
  float _pucbConstantInit;

  /**
   * Base controlling the PUCB exploration coefficient.
   */
  float _pucbConstantBase;

  /**
   * Minimum visit ratio for prioritizing PUCB children.
   */
  float _pucbMinVisitsRate;
};

}  // namespace deepshogi

#pragma once

#include "Processor.h"

namespace deepshogi {

/**
 * Class for configuration parameters of node objects.
 */
class NodeParameter {
 public:
  /**
   * Create a parameter object.
   * @param processor Object that performs inference
   * @param nyugyokuScoreBlack Points required for black's entering king declaration
   * @param nyugyokuScoreWhite Points required for white's entering king declaration
   * @param drawTurn Number of moves until a draw
   * @param ucbConstant Constant multiplied to UCB upper confidence bound
   * @param pucbConstantInit Initial value applied to PUCB upper confidence bound
   * @param pucbConstantBase Base value applied to PUCB upper confidence bound
   */
  NodeParameter(
      Processor* processor,
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
      float ucbConstant, float pucbConstantInit, float pucbConstantBase);

  /**
   * Destroy the parameter object.
   */
  virtual ~NodeParameter() = default;

  /**
   * Return the object that performs inference.
   * @return Object that performs inference
   */
  Processor* getProcessor() const;

  /**
   * Get the points required for black's entering king declaration.
   * @return Points required for black's entering king declaration
   */
  int32_t getNyugyokuScoreBlack() const;

  /**
   * Get the points required for white's entering king declaration.
   * @return Points required for white's entering king declaration
   */
  int32_t getNyugyokuScoreWhite() const;

  /**
   * Get the number of moves until a draw.
   * @return Number of moves until a draw
   */
  int32_t getDrawTurn() const;

  /**
   * Get the constant multiplied to UCB upper confidence bound.
   * @return Constant multiplied to UCB upper confidence bound
   */
  float getUcbConstant() const;

  /**
   * Get the initial value applied to PUCB upper confidence bound.
   * @return Initial value applied to PUCB upper confidence bound
   */
  float getPucbConstantInit() const;

  /**
   * Get the base value applied to PUCB upper confidence bound.
   * @return Base value applied to PUCB upper confidence bound
   */
  float getPucbConstantBase() const;

 private:
  /**
   * Object that performs inference.
   */
  Processor* _processor;

  /**
   * Points required for black's entering king declaration.
   */
  int32_t _nyugyokuScoreBlack;

  /**
   * Points required for white's entering king declaration.
   */
  int32_t _nyugyokuScoreWhite;

  /**
   * Number of moves until a draw.
   */
  int32_t _drawTurn;

  /**
   * Constant multiplied to UCB upper confidence bound.
   */
  float _ucbConstant;

  /**
   * Initial value applied to PUCB upper confidence bound.
   */
  float _pucbConstantInit;

  /**
   * Base value applied to PUCB upper confidence bound.
   */
  float _pucbConstantBase;
};

}  // namespace deepshogi

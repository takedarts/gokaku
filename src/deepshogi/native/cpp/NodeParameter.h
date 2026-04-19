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
   * @param cacheSize Cache size for evaluation results
   * @param nyugyokuScoreBlack Points required for black's entering king declaration
   * @param nyugyokuScoreWhite Points required for white's entering king declaration
   * @param drawTurn Number of moves until a draw
   * @param ucbConstant Constant multiplied to UCB upper confidence bound
   * @param pucbConstantInit Initial value applied to PUCB upper confidence bound
   * @param pucbConstantBase Base value applied to PUCB upper confidence bound
   */
  NodeParameter(
      Processor* processor, int32_t cacheSize,
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
  inline Processor* getProcessor() const {
    return _processor;
  }

  /**
   * Get the cache size for evaluation results.
   * @return Cache size for evaluation results
   */
  inline int32_t getCacheSize() const {
    return _cacheSize;
  }

  /**
   * Get the points required for black's entering king declaration.
   * @return Points required for black's entering king declaration
   */
  inline int32_t getNyugyokuScoreBlack() const {
    return _nyugyokuScoreBlack;
  }

  /**
   * Get the points required for white's entering king declaration.
   * @return Points required for white's entering king declaration
   */
  inline int32_t getNyugyokuScoreWhite() const {
    return _nyugyokuScoreWhite;
  }

  /**
   * Get the number of moves until a draw.
   * @return Number of moves until a draw
   */
  inline int32_t getDrawTurn() const {
    return _drawTurn;
  }

  /**
   * Get the constant multiplied to UCB upper confidence bound.
   * @return Constant multiplied to UCB upper confidence bound
   */
  inline float getUcbConstant() const {
    return _ucbConstant;
  }

  /**
   * Get the initial value applied to PUCB upper confidence bound.
   * @return Initial value applied to PUCB upper confidence bound
   */
  inline float getPucbConstantInit() const {
    return _pucbConstantInit;
  }

  /**
   * Get the base value applied to PUCB upper confidence bound.
   * @return Base value applied to PUCB upper confidence bound
   */
  inline float getPucbConstantBase() const {
    return _pucbConstantBase;
  }

 private:
  /**
   * Object that performs inference.
   */
  Processor* _processor;

  /**
   * Cache size for evaluation results.
   */
  int32_t _cacheSize;

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

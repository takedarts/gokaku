#pragma once

#include <cstdint>

namespace deepshogi {

/**
 * A class for managing parameters related to MCTS search.
 */
class MctsParameter {
 public:
  /**
   * Creates a parameter object.
   * @param nyugyokuScoreBlack Score required for black's entering-king declaration
   * @param nyugyokuScoreWhite Score required for white's entering-king declaration
   * @param drawTurn Number of moves until a draw
   * @param pucbConstantInit Initial value of the constant multiplied by the PUCB confidence upper bound
   * @param pucbConstantBase Rate-of-change value of the constant multiplied by the PUCB confidence upper bound
   */
  MctsParameter(
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
      float pucbConstantInit, float pucbConstantBase);

  /**
   * Copies a parameter object.
   * @param other The source parameter object to copy from
   */
  MctsParameter(const MctsParameter& other) = default;

  /**
   * Destroys the parameter object.
   */
  virtual ~MctsParameter() = default;

  /**
   * Gets the score required for black's entering-king declaration.
   * @return Score required for black's entering-king declaration
   */
  inline int32_t getNyugyokuScoreBlack() const {
    return _nyugyokuScoreBlack;
  }

  /**
   * Gets the score required for white's entering-king declaration.
   * @return Score required for white's entering-king declaration
   */
  inline int32_t getNyugyokuScoreWhite() const {
    return _nyugyokuScoreWhite;
  }

  /**
   * Gets the number of moves until a draw.
   * @return Number of moves until a draw
   */
  inline int32_t getDrawTurn() const {
    return _drawTurn;
  }

  /**
   * Gets the initial value of the constant multiplied by the PUCB confidence upper bound.
   * @return Initial value of the constant multiplied by the PUCB confidence upper bound
   */
  inline float getPucbConstantInit() const {
    return _pucbConstantInit;
  }

  /**
   * Gets the rate-of-change value of the constant multiplied by the PUCB confidence upper bound.
   * @return Rate-of-change value of the constant multiplied by the PUCB confidence upper bound
   */
  inline float getPucbConstantBase() const {
    return _pucbConstantBase;
  }

 private:
  /**
   * Score required for black's entering-king declaration.
   */
  int32_t _nyugyokuScoreBlack;

  /**
   * Score required for white's entering-king declaration.
   */
  int32_t _nyugyokuScoreWhite;

  /**
   * Number of moves until a draw.
   */
  int32_t _drawTurn;

  /**
   * Initial value of the constant multiplied by the PUCB confidence upper bound.
   */
  float _pucbConstantInit;

  /**
   * Rate-of-change value of the constant multiplied by the PUCB confidence upper bound.
   */
  float _pucbConstantBase;
};

}  // namespace deepshogi

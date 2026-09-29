#pragma once

#include <cstdint>

namespace deepshogi {

/**
 * A class for managing parameters related to MCTS search.
 */
class MctsParameter {
 public:
  /**
   * Create the parameter object.
   * @param nyugyokuScoreBlack Points required for Black's entering-king declaration.
   * @param nyugyokuScoreWhite Points required for White's entering-king declaration.
   * @param drawTurn Move count at which the game is drawn.
   * @param sennichitePenalty Penalty assigned to repetition evaluations.
   * @param checkSearchDepth Checkmate search depth.
   * @param checkSearchNode Checkmate search node capacity.
   * @param pucbConstantInit Initial PUCB exploration coefficient.
   * @param pucbConstantBase Base controlling the PUCB exploration coefficient.
   * @param pucbMinVisitsRate Minimum visit ratio for prioritizing PUCB children.
   */
  MctsParameter(
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
      float sennichitePenalty, float pucbConstantInit, float pucbConstantBase,
      float pucbMinVisitsRate);

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
   * Points required for Black's entering-king declaration.
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

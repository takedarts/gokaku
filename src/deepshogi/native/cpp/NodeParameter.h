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
   * @param drawSteps Number of moves until a draw
   * @param checkSearchDepth Depth for mate search
   * @param checkSearchNode Number of nodes for mate search
   */
  NodeParameter(
      Processor* processor,
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps,
      int32_t checkSearchDepth, int32_t checkSearchNode);

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
  int32_t getDrawSteps() const;

  /**
   * Get the depth for mate search.
   * @return Depth for mate search
   */
  int32_t getCheckSearchDepth() const;

  /**
   * Get the number of nodes for mate search.
   * @return Number of nodes for mate search
   */
  int32_t getCheckSearchNode() const;

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
  int32_t _drawSteps;

  /**
   * Depth for mate search.
   */
  int32_t _checkSearchDepth;

  /**
   * Number of nodes for mate search.
   */
  int32_t _checkSearchNode;
};

}  // namespace deepshogi

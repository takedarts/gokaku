#include "NodeParameter.h"

namespace deepshogi {

/**
 * Create a parameter object.
 * @param processor Object that performs inference
 * @param nyugyokuScoreBlack Points required for black's entering king declaration
 * @param nyugyokuScoreWhite Points required for white's entering king declaration
 * @param drawTurn Number of moves until a draw
 * @param checkSearchDepth Depth for mate search
 * @param checkSearchNode Number of nodes for mate search
 * @param ucbConstant Constant multiplied to UCB upper confidence bound
 * @param pucbConstantInit Initial value applied to PUCB upper confidence bound
 * @param pucbConstantBase Base value applied to PUCB upper confidence bound
 */
NodeParameter::NodeParameter(
    Processor* processor,
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
    int32_t checkSearchDepth, int32_t checkSearchNode,
    float ucbConstant, float pucbConstantInit, float pucbConstantBase)
    : _processor(processor),
      _nyugyokuScoreBlack(nyugyokuScoreBlack),
      _nyugyokuScoreWhite(nyugyokuScoreWhite),
      _drawTurn(drawTurn),
      _checkSearchDepth(checkSearchDepth),
      _checkSearchNode(checkSearchNode),
      _ucbConstant(ucbConstant),
      _pucbConstantInit(pucbConstantInit),
      _pucbConstantBase(pucbConstantBase) {
}

/**
 * Return the object that performs inference.
 * @return Object that performs inference
 */
Processor* NodeParameter::getProcessor() const {
  return _processor;
}

/**
 * Get the points required for black's entering king declaration.
 * @return Points required for black's entering king declaration
 */
int32_t NodeParameter::getNyugyokuScoreBlack() const {
  return _nyugyokuScoreBlack;
}

/**
 * Get the points required for white's entering king declaration.
 * @return Points required for white's entering king declaration
 */
int32_t NodeParameter::getNyugyokuScoreWhite() const {
  return _nyugyokuScoreWhite;
}

/**
 * Get the number of moves until a draw.
 * @return Number of moves until a draw
 */
int32_t NodeParameter::getDrawTurn() const {
  return _drawTurn;
}

/**
 * Get the depth for mate search.
 * @return Depth for mate search
 */
int32_t NodeParameter::getCheckSearchDepth() const {
  return _checkSearchDepth;
}

/**
 * Get the number of nodes for mate search.
 * @return Number of nodes for mate search
 */
int32_t NodeParameter::getCheckSearchNode() const {
  return _checkSearchNode;
}

/**
 * Get the constant multiplied to UCB upper confidence bound.
 * @return Constant multiplied to UCB upper confidence bound
 */
float NodeParameter::getUcbConstant() const {
  return _ucbConstant;
}

/**
 * Get the initial value applied to PUCB upper confidence bound.
 * @return Initial value applied to PUCB upper confidence bound
 */
float NodeParameter::getPucbConstantInit() const {
  return _pucbConstantInit;
}

/**
 * Get the base value applied to PUCB upper confidence bound.
 * @return Base value applied to PUCB upper confidence bound
 */
float NodeParameter::getPucbConstantBase() const {
  return _pucbConstantBase;
}

}  // namespace deepshogi

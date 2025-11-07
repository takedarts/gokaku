#pragma once

#include "Board.h"
#include "Config.h"
#include "Policy.h"
#include "Processor.h"

namespace deepshogi {

/**
 * Class to store evaluation results.
 */
class Evaluator {
 public:
  /**
   * Create evaluation result object.
   * @param processor Object to execute inference
   */
  Evaluator(Processor* processor);

  /**
   * Clear evaluation results from the model.
   */
  void clear();

  /**
   * Execute evaluation by the model.
   * @param board Board to be evaluated
   */
  void evaluate(Board* board);

  /**
   * Return true if evaluation results from the model are set.
   * @return True if evaluation results from the model are set
   */
  bool isEvaluated();

  /**
   * Get list of predicted candidate moves from model inference results.
   * @return List of predicted candidate moves
   */
  std::vector<Policy> getPolicies();

  /**
   * Get predicted win rate from model inference results.
   * @return Predicted win rate from model inference results
   */
  float getValue();

 private:
  /**
   * Object to execute inference.
   */
  Processor* _processor;

  /**
   * List of candidate moves from model inference results
   */
  std::vector<Policy> _policies;

  /**
   * Predicted win rate from model inference results.
   */
  float _value;

  /**
   * True if evaluation results from the model are set.
   */
  bool _evaluated;
};

}  // namespace deepshogi

#pragma once

#include <vector>

#include "Policy.h"

namespace deepshogi {

/**
 * Class that stores evaluation results.
 */
class Evaluation {
 public:
  /**
   * Creates an object that stores model evaluation results.
   * @param value Evaluation value produced by the model
   * @param policies List of candidate moves from model inference
   */
  Evaluation(float value, const std::vector<Policy>& policies);

  /**
   * Copies model evaluation results.
   * @param other Source evaluation result to copy
   */
  Evaluation(const Evaluation& other);

  /**
   * Gets the model evaluation value.
   * @return Evaluation value produced by the model
   */
  inline float getValue() const {
    return _value;
  }

  /**
   * Gets the list of candidate moves from model inference.
   * @return List of candidate moves from model inference
   */
  inline const std::vector<Policy>& getPolicies() const {
    return _policies;
  }

 private:
  /**
   * List of candidate moves from model inference.
   */
  std::vector<Policy> _policies;

  /**
   * Evaluation value from model inference.
   */
  float _value;
};

}  // namespace deepshogi

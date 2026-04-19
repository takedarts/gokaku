#include "Evaluation.h"

namespace deepshogi {

/**
 * Creates an object that stores model evaluation results.
 * @param value Evaluation value produced by the model
 * @param policies List of candidate moves from model inference
 */
Evaluation::Evaluation(float value, const std::vector<Policy>& policies)
    : _value(value),
      _policies(policies) {
}

/**
 * Copies model evaluation results.
 * @param other Source evaluation result to copy
 */
Evaluation::Evaluation(const Evaluation& other)
    : _value(other._value),
      _policies(other._policies) {
}

}  // namespace deepshogi

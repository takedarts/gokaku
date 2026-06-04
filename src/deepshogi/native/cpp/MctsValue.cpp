#include "MctsValue.h"

#include <cmath>

#include "Config.h"

namespace deepshogi {

/**
 * Creates an MCTS evaluation value object.
 */
MctsValue::MctsValue()
    : _mutex(),
      _value(0.0f),
      _count(0) {
}

/**
 * Copies an MCTS evaluation value object.
 * @param other The source object to copy from
 */
MctsValue::MctsValue(const MctsValue& other)
    : _mutex(),
      _value(other._value),
      _count(other._count) {
}

/**
 * Resets the MCTS evaluation value.
 */
void MctsValue::reset() {
  std::lock_guard<std::mutex> lock(_mutex);
  _value = 0.0f;
  _count = 0;
}

/**
 * Updates the MCTS evaluation value.
 * @param value Evaluation value
 */
void MctsValue::update(float value) {
  std::lock_guard<std::mutex> lock(_mutex);
  _value += value;
  _count++;
}

/**
 * Sets the MCTS evaluation value.
 * @param value Evaluation value
 */
void MctsValue::setValue(float value) {
  std::lock_guard<std::mutex> lock(_mutex);
  _value = value * _count;
}

/**
 * Gets the average MCTS evaluation value.
 * @param defaultValue Value to return when the evaluation count is 0
 * @return Average evaluation value
 */
float MctsValue::getValue(float defaultValue) {
  std::lock_guard<std::mutex> lock(_mutex);
  return (_count != 0) ? _value / _count : defaultValue;
}

/**
 * Gets the lower confidence bound of the MCTS evaluation value.
 * @param color Turn color (COLOR_BLACK or COLOR_WHITE)
 * @param defaultValue Value to return when the evaluation count is 0
 * @return Lower confidence bound of the evaluation value
 */
float MctsValue::getValueLCB(int8_t color, float defaultValue) {
  std::lock_guard<std::mutex> lock(_mutex);
  float value = (_count != 0) ? _value / _count : defaultValue;
  float lower = 1.96f * 0.5f / std::sqrt((float)(_count + 1));

  return value - (lower * color);
}

}  // namespace deepshogi

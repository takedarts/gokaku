#pragma once

#include <cstdint>
#include <mutex>

namespace deepshogi {

/**
 * A class for managing MCTS evaluation values.
 * Manages the sum of evaluation values and the evaluation count.
 * Allows thread-safe retrieval of the average evaluation value.
 */
class MctsValue {
 public:
  /**
   * Creates an MCTS evaluation value object.
   */
  MctsValue();

  /**
   * Copies an MCTS evaluation value object.
   * @param other The source object to copy from
   */
  MctsValue(const MctsValue& other);

  /**
   * Destroys the MCTS evaluation value object.
   */
  virtual ~MctsValue() = default;

  /**
   * Resets the MCTS evaluation value.
   */
  void reset();

  /**
   * Updates the MCTS evaluation value.
   * @param value Evaluation value
   */
  void update(float value);

  /**
   * Sets the MCTS evaluation value.
   * @param value Evaluation value
   */
  void setValue(float value);

  /**
   * Gets the average MCTS evaluation value.
   * @param defaultValue Value to return when the evaluation count is 0
   * @return Average evaluation value
   */
  float getValue(float defaultValue);

  /**
   * Gets the lower confidence bound of the MCTS evaluation value.
   * @param color Turn color (COLOR_BLACK or COLOR_WHITE)
   * @param defaultValue Value to use as the evaluation value when the evaluation count is 0
   * @return Lower confidence bound of the evaluation value
   */
  float getValueLCB(int8_t color, float defaultValue);

 private:
  /**
   * Mutex for synchronization.
   */
  std::mutex _mutex;

  /**
   * Sum of evaluation values.
   */
  float _value;

  /**
   * Evaluation count.
   */
  int32_t _count;
};

}  // namespace deepshogi

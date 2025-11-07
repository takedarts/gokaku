#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace deepshogi {

class ExecutorJob {
 public:
  /**
   * Create a computation object.
   */
  ExecutorJob(float* inputs, float* outputs, int32_t size);

  /**
   * Wait until computation is complete.
   */
  void wait();

  /**
   * Notify that computation is complete.
   */
  void notify();

  /**
   * Return input data.
   */
  float* getInputs() const;

  /**
   * Return output data.
   */
  float* getOutputs() const;

  /**
   * Return the number of data.
   */
  int32_t getSize() const;

 private:
  /**
   * Mutex for synchronization.
   */
  std::mutex _mutex;

  /**
   * Condition variable for synchronization.
   */
  std::condition_variable _condition;

  /**
   * Input data.
   */
  float* _inputs;

  /**
   * Output data.
   */
  float* _outputs;

  /**
   * Number of data.
   */
  int32_t _size;

  /**
   * True if computation is complete.
   */
  bool _terminated;
};

}  // namespace deepshogi

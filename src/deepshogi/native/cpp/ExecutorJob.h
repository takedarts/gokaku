#pragma once

#include <atomic>
#include <cstdint>
#include <exception>

namespace deepshogi {

class ExecutorJob {
 public:
  /**
   * Create a computation object.
   */
  ExecutorJob(int32_t* inputs, float* outputs, int32_t size);

  /**
   * Wait until computation is complete.
   */
  void wait();

  /**
   * Notify that computation is complete.
   * @param exception Exception pointer if an exception occurred during computation
   */
  void notify(std::exception_ptr exception = nullptr);

  /**
   * Return input data.
   */
  int32_t* getInputs() const;

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
   * Input data.
   */
  int32_t* _inputs;

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
  std::atomic<bool> _terminated;

  /**
   * Exception that occurred during computation.
   */
  std::exception_ptr _exception;
};

}  // namespace deepshogi

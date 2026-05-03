#include "ExecutorJob.h"

namespace deepshogi {

/**
 * Create a computation object.
 */
ExecutorJob::ExecutorJob(int32_t* inputs, float* outputs, int32_t size)
    : _inputs(inputs),
      _outputs(outputs),
      _size(size),
      _terminated(false),
      _exception(nullptr) {
}

/**
 * Wait until computation is complete.
 */
void ExecutorJob::wait() {
  while (!_terminated.load(std::memory_order_acquire)) {
    _terminated.wait(false, std::memory_order_acquire);
  }

  if (_exception != nullptr) {
    std::rethrow_exception(_exception);
  }
}

/**
 * Notify that computation is complete.
 * @param exception Exception pointer if an exception occurred during computation
 */
void ExecutorJob::notify(std::exception_ptr exception) {
  _exception = exception;
  _terminated.store(true, std::memory_order_release);
  _terminated.notify_all();
}

/**
 * Return input data.
 */
int32_t* ExecutorJob::getInputs() const {
  return _inputs;
}

/**
 * Return output data.
 */
float* ExecutorJob::getOutputs() const {
  return _outputs;
}

/**
 * Return the number of data.
 */
int32_t ExecutorJob::getSize() const {
  return _size;
}

}  // namespace deepshogi

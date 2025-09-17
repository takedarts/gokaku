#include "ExecutorJob.h"

namespace deepshogi {

/**
 * Create a computation object.
 */
ExecutorJob::ExecutorJob(float* inputs, float* outputs, int32_t size)
    : _mutex(),
      _condition(),
      _inputs(inputs),
      _outputs(outputs),
      _size(size),
      _terminated(false) {
}

/**
 * Wait until computation is complete.
 */
void ExecutorJob::wait() {
  {  // synchronize
    std::unique_lock<std::mutex> lock(_mutex);
    _condition.wait(lock, [this] { return _terminated; });
  }
}

/**
 * Notify that computation is complete.
 */
void ExecutorJob::notify() {
  {  // synchronize
    std::unique_lock<std::mutex> lock(_mutex);
    _terminated = true;
    _condition.notify_all();
  }
}

/**
 * Return input data.
 */
float* ExecutorJob::getInputs() const {
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

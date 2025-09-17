#include "Executor.h"

#include <iostream>

#include "Config.h"

namespace deepshogi {

/**
 * Create an inference execution object.
 * @param model Model file
 * @param gpu GPU number
 * @param batchSize Maximum batch size
 * @param fp16 True to compute with 16-bit precision
 * @param deterministic True to make computation results reproducible
 */
Executor::Executor(
    std::string model, int32_t gpu, int32_t batchSize, bool fp16, bool deterministic)
    : _mutex(),
      _condition(),
      _model(new Model(model, gpu, fp16, deterministic)),
      _thread(),
      _terminated(false),
      _queue(),
      _waitingCount(0),
      _reservedCount(0),
      _batchSize(batchSize) {
  _thread.reset(new std::thread([this]() { this->_run(); }));
}

/**
 * Destructor.
 */
Executor::~Executor() {
  // Set the termination flag.
  {  // synchronize
    std::unique_lock<std::mutex> lock(_mutex);
    _terminated = true;
    _condition.notify_all();
  }

  // Wait for the thread to stop.
  _thread->join();
}

/**
 * Execute inference.
 * @param inputs Input data
 * @param outputs Output data
 * @param size Number of input/output data
 */
void Executor::execute(float* inputs, float* outputs, int32_t size) {
  // Create a computation object.
  std::unique_ptr<ExecutorJob> job(new ExecutorJob(inputs, outputs, size));

  // Add to the queue.
  {  // synchronize
    std::unique_lock<std::mutex> lock(_mutex);
    _queue.push(job.get());
    _waitingCount += job->getSize();
    _reservedCount = std::max(0, _reservedCount - job->getSize());
    _condition.notify_all();
  }

  // Wait until computation is complete.
  job->wait();
}

/**
 * Return the number of pending computation tasks.
 * @return Number of pending computation tasks
 */
int32_t Executor::getWaitingCount() {
  std::unique_lock<std::mutex> lock(_mutex);
  return _waitingCount + _reservedCount;
}

/**
 * Increase the number of reserved computation tasks.
 * @param reservedCount Number of reservations
 */
void Executor::addReservedCount(int32_t reservedCount) {
  std::unique_lock<std::mutex> lock(_mutex);
  _reservedCount += reservedCount;
}

/**
 * Method executed by the thread.
 */
void Executor::_run() {
  try {
    while (true) {
      std::vector<ExecutorJob*> jobs;

      // Check the state of the queue and execution
      {  // synchronize
        // If the queue is empty, wait.
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [this] { return !_queue.empty() || _terminated; });

        // If the termination flag is set, terminate.
        if (_terminated) {
          // Take out all computation objects remaining in the queue and notify termination.
          while (!_queue.empty()) {
            ExecutorJob* job = _queue.front();
            _queue.pop();
            job->notify();
          }

          break;
        }

        // Take computation objects out of the queue.
        int32_t jobs_size = 0;

        while (!_queue.empty() && jobs_size < _batchSize) {
          ExecutorJob* job = _queue.front();
          _queue.pop();
          _waitingCount -= job->getSize();
          jobs.push_back(job);
          jobs_size += job->getSize();
        }
      }

      // Execute computation.
      _forward(jobs);

      // Notify that computation is complete.
      for (auto job : jobs) {
        job->notify();
      }
    }
  } catch (std::exception& e) {
    std::cerr << "Executor::_run : " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Executor::_run : unknown error" << std::endl;
  }
}

/**
 * Execute computation.
 * @param jobs List of computation tasks
 */
void Executor::_forward(std::vector<ExecutorJob*>& jobs) {
  // Calculate the size of the input data
  int32_t jobs_size = 0;

  for (auto job : jobs) {
    jobs_size += job->getSize();
  }

  // Allocate space for input and output data
  std::unique_ptr<float[]> all_inputs(new float[jobs_size * MODEL_INPUT_SIZE]);
  std::unique_ptr<float[]> all_outputs(new float[jobs_size * MODEL_OUTPUT_SIZE]);

  // Prepare input data
  int32_t input_offset = 0;

  for (auto job : jobs) {
    float* inputs = job->getInputs();
    int32_t size = job->getSize();

    memcpy(
        all_inputs.get() + input_offset * MODEL_INPUT_SIZE,
        inputs,
        size * MODEL_INPUT_SIZE * sizeof(float));

    input_offset += size;
  }

  // Execute computation
  _model->forward(all_inputs.get(), all_outputs.get(), jobs_size);

  // Copy output data
  int32_t out_offset = 0;

  for (auto job : jobs) {
    float* outputs = job->getOutputs();
    int32_t size = job->getSize();

    memcpy(
        outputs,
        all_outputs.get() + out_offset * MODEL_OUTPUT_SIZE,
        size * MODEL_OUTPUT_SIZE * sizeof(float));

    out_offset += size;
  }
}

}  // namespace deepshogi

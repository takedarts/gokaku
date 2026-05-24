#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "InferenceModel.h"
#include "InferenceResult.h"
#include "MctsNode.h"

namespace deepshogi {

/**
 * Type of the callback function called when inference completes.
 */
typedef std::function<void(MctsNode*, const InferenceResult&)> InferenceExecutorCallback;

/**
 * A class that executes inference for board evaluation.
 */
class InferenceExecutor {
 public:
  /**
   * Creates an inference executor object.
   * @param model Path to the inference model file
   * @param gpu ID of the GPU to use
   * @param fp16 True to use half-precision floating point
   * @param deterministic True to enable deterministic behavior
   * @param batchSize Batch size
   * @param threads Number of threads to run
   */
  InferenceExecutor(
      std::string model, int32_t gpu, bool fp16, bool deterministic,
      int32_t batchSize, int32_t threads);

  /**
   * Destroys the inference manager object.
   */
  virtual ~InferenceExecutor();

  /**
   * Schedules an inference execution.
   * The inference computation runs asynchronously, so this function returns immediately.
   * When the computation completes, the node's evaluation value is updated.
   * @param node Node to run inference on
   * @param callback Callback function to notify when inference completes
   */
  void submit(MctsNode* node, InferenceExecutorCallback callback);

  /**
   * Runs inference synchronously.
   * @param inputs Input data
   * @param masks Output data mask
   * @param outputs Output data
   * @param size Number of evaluation data items
   */
  void execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size);

  /**
   * Returns the number of pending inference executions in the submission queue.
   * @return Number of pending inference executions in the queue
   */
  int32_t getQueueSize();

 private:
  /**
   * Mutex object for model synchronization.
   */
  std::mutex _modelMutex;

  /**
   * Mutex object for thread synchronization.
   */
  std::mutex _threadMutex;

  /**
   * Condition variable for synchronization.
   */
  std::condition_variable _condition;

  /**
   * Inference model object.
   */
  InferenceModel* _model;

  /**
   * Path to the inference model file.
   */
  std::string _modelFile;

  /**
   * ID of the GPU to use.
   */
  int32_t _gpu;

  /**
   * Whether to use half-precision floating point.
   */
  bool _fp16;

  /**
   * Whether to enable deterministic behavior.
   */
  bool _deterministic;

  /**
   * Batch size.
   */
  int32_t _batchSize;

  /**
   * List of thread objects for running inference.
   */
  std::vector<std::thread> _threads;

  /**
   * True to terminate inference processing.
   */
  bool _terminated;

  /**
   * Queue of scheduled inference executions.
   */
  std::vector<std::pair<MctsNode*, InferenceExecutorCallback>> _queue;

  /**
   * Method executed on a thread.
   */
  void _run();
};

}  // namespace deepshogi

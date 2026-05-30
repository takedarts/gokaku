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

class InferenceProcessor;

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
   * @param processor Inference manager object
   * @param file Path to the inference model file
   * @param gpu ID of the GPU to use
   * @param fp16 True to use half-precision floating point
   * @param deterministic True to enable deterministic behavior
   * @param batchSize Batch size
   * @param threads Number of threads to run
   */
  InferenceExecutor(
      InferenceProcessor* processor, std::string file, int32_t gpu, bool fp16,
      bool deterministic, int32_t batchSize, int32_t threads);

  /**
   * Destroys the inference manager object.
   */
  virtual ~InferenceExecutor();

  /**
   * Runs inference synchronously.
   * @param inputs Input data
   * @param masks Output data mask
   * @param outputs Output data
   * @param size Number of evaluation data items
   */
  void execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size);

  /**
   * Gets the inference efficiency.
   * @return Inference efficiency
   */
  inline float getEfficiency() const {
    float total_efficiency = 0.0f;

    for (const auto& efficiency : _efficiencies) {
      total_efficiency += efficiency.load(std::memory_order_relaxed);
    }

    return total_efficiency / static_cast<float>(_efficiencies.size());
  }

 private:
  /**
   * Mutex object for model synchronization.
   */
  std::mutex _mutex;

  /**
   * Inference manager object.
   */
  InferenceProcessor* _processor;

  /**
   * Inference model object.
   */
  InferenceModel* _model;

  /**
   * Path to the inference model file.
   */
  std::string _file;

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
   * Statistical information on inference efficiency.
   */
  std::vector<std::atomic<float>> _efficiencies;

  /**
   * Method executed on a thread.
   * @param threadIndex Thread index
   */
  void _run(int32_t threadIndex);
};

}  // namespace deepshogi

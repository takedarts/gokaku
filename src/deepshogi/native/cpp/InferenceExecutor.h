#pragma once

#include <condition_variable>
#include <memory>
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
   * Gets the batch fill rate.
   * @return Batch fill rate
   */
  inline float getBatchFillRate() const {
    float total_fill_rate = 0.0f;

    for (const auto& fill_rate : _batchFillRates) {
      total_fill_rate += fill_rate.load(std::memory_order_relaxed);
    }

    return total_fill_rate / static_cast<float>(_batchFillRates.size());
  }

 private:
  /**
   * Inference processor.
   */
  InferenceProcessor* _processor;

  /**
   * Inference model.
   */
  std::unique_ptr<InferenceModel> _model;

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
   * Per-thread fraction of batch capacity occupied by inference requests.
   */
  std::vector<std::atomic<float>> _batchFillRates;

  /**
   * Per-thread input buffers allocated before workers start.
   */
  std::vector<std::vector<int32_t>> _inputBuffers;

  /**
   * Per-thread output mask buffers allocated before workers start.
   */
  std::vector<std::vector<int32_t>> _maskBuffers;

  /**
   * Per-thread inference result buffers allocated before workers start.
   */
  std::vector<std::vector<float>> _outputBuffers;

  /**
   * Method executed on a thread.
   * @param threadIndex Thread index
   */
  void _run(int32_t threadIndex);
};

}  // namespace deepshogi

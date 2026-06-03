#include "InferenceExecutor.h"

#include "InferenceProcessor.h"

namespace deepshogi {

constexpr int32_t DIR_H = 0;    // Move: drop
constexpr int32_t DIR_U = 1;    // Move: up
constexpr int32_t DIR_D = 2;    // Move: down
constexpr int32_t DIR_R = 3;    // Move: right
constexpr int32_t DIR_L = 4;    // Move: left
constexpr int32_t DIR_UR = 5;   // Move: upper-right
constexpr int32_t DIR_UL = 6;   // Move: upper-left
constexpr int32_t DIR_DR = 7;   // Move: lower-right
constexpr int32_t DIR_DL = 8;   // Move: lower-left
constexpr int32_t DIR_KR = 9;   // Move: knight right
constexpr int32_t DIR_KL = 10;  // Move: knight left

/**
 * Returns the policy index for the specified move.
 * @param board Board
 * @param move Move
 * @return Policy index
 */
static int32_t getPolicyIndex(const Board* board, Move move) {
  // Calculate the piece index and move direction
  int32_t move_x = 0;
  int32_t move_y = 0;
  int32_t piece = 0;

  if (move.getSrc().getX() >= BOARD_SIZE) {
    move_x = 0;
    move_y = 0;
    piece = move.getSrc().getY() - PIECE_HAND_BEGIN;
  } else {
    move_x = move.getDst().getX() - move.getSrc().getX();
    move_y = move.getDst().getY() - move.getSrc().getY();
    piece = board->getPiece(move.getSrc());

    if (move.isPromote()) {
      piece += PIECE_PROMOTE;
    }

    if (PIECE_BLACK_BEGIN <= piece && piece < PIECE_BLACK_END) {
      piece -= PIECE_BLACK_BEGIN;
    } else if (PIECE_WHITE_BEGIN <= piece && piece < PIECE_WHITE_END) {
      piece -= PIECE_WHITE_BEGIN;
    } else {
      throw std::invalid_argument("Invalid piece");
    }
  }

  // Flip the move coordinates for the white player
  if (board->getColor() == COLOR_WHITE) {
    move_x = -move_x;
    move_y = -move_y;
  }

  // Determine the move direction
  int32_t dir = 0;

  if (move_x == 0 && move_y == 0) {  // drop
    dir = DIR_H;
  } else if (move_x == 0 && move_y < 0) {  // up
    dir = DIR_U;
  } else if (move_x == 0 && move_y > 0) {  // down
    dir = DIR_D;
  } else if (move_x < 0 && move_y == 0) {  // right
    dir = DIR_R;
  } else if (move_x > 0 && move_y == 0) {  // left
    dir = DIR_L;
  } else if (move_x < 0 && move_y == move_x) {  // upper-right
    dir = DIR_UR;
  } else if (move_x > 0 && move_y == -move_x) {  // upper-left
    dir = DIR_UL;
  } else if (move_x < 0 && move_y == -move_x) {  // lower-right
    dir = DIR_DR;
  } else if (move_x > 0 && move_y == move_x) {  // lower-left
    dir = DIR_DL;
  } else if (move_x == -1 && move_y == -2) {  // knight right
    dir = DIR_KR;
  } else if (move_x == 1 && move_y == -2) {  // knight left
    dir = DIR_KL;
  } else {
    throw std::invalid_argument("Invalid move direction");
  }

  // Calculate the policy index
  int32_t index = 0;

  if (piece == 0) {  // pawn
    index =
        0 + ((dir == DIR_U)   ? 0
             : (dir == DIR_H) ? 1
                              : -1);
  } else if (piece == 1) {  // lance
    index =
        2 + ((dir == DIR_U)   ? 0
             : (dir == DIR_H) ? 1
                              : -3);
  } else if (piece == 2) {  // knight
    index =
        4 + ((dir == DIR_KR)   ? 0
             : (dir == DIR_KL) ? 1
             : (dir == DIR_H)  ? 2
                               : -5);
  } else if (piece == 3) {  // silver
    index =
        7 + ((dir == DIR_U)    ? 0
             : (dir == DIR_UR) ? 1
             : (dir == DIR_UL) ? 2
             : (dir == DIR_DR) ? 3
             : (dir == DIR_DL) ? 4
             : (dir == DIR_H)  ? 5
                               : -8);
  } else if (piece == 4) {  // bishop
    index =
        13 + ((dir == DIR_UR)   ? 0
              : (dir == DIR_UL) ? 1
              : (dir == DIR_DR) ? 2
              : (dir == DIR_DL) ? 3
              : (dir == DIR_H)  ? 4
                                : -14);
  } else if (piece == 5) {  // rook
    index =
        18 + ((dir == DIR_U)   ? 0
              : (dir == DIR_D) ? 1
              : (dir == DIR_R) ? 2
              : (dir == DIR_L) ? 3
              : (dir == DIR_H) ? 4
                               : -19);
  } else if (piece == 6) {  // gold
    index =
        23 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_H)  ? 6
                                : -24);
  } else if (piece == 7) {  // king
    index =
        30 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -31);
  } else if (piece == 8) {  // tokin (promoted pawn)
    index =
        38 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
                                : -39);
  } else if (piece == 9) {  // promoted lance
    index =
        44 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
                                : -45);
  } else if (piece == 10) {  // promoted knight
    index =
        50 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_KR) ? 6
              : (dir == DIR_KL) ? 7
                                : -51);
  } else if (piece == 11) {  // promoted silver
    index =
        58 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -59);
  } else if (piece == 12) {  // horse (promoted bishop)
    index =
        66 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -67);
  } else if (piece == 13) {  // dragon (promoted rook)
    index =
        74 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -75);
  } else {
    throw std::invalid_argument("Invalid piece");
  }

  if (index < 0) {
    throw std::invalid_argument("Invalid index");
  }

  // Get the destination coordinates
  int32_t x = move.getDst().getX();
  int32_t y = move.getDst().getY();

  if (board->getColor() == COLOR_WHITE) {
    x = BOARD_SIZE - 1 - x;
    y = BOARD_SIZE - 1 - y;
  }

  // Return the index
  return (index * BOARD_SIZE * BOARD_SIZE) + (x * BOARD_SIZE + y);
}

/**
 * Creates the output mask data for the specified board.
 * @param board Board
 * @param mask Buffer for mask data
 */
static void getOutputMask(const Board* board, int32_t* mask) {
  // Initialize the mask data to zero
  std::fill(mask, mask + MODEL_OUTPUT_PACK_SIZE, 0);

  // Create the policy mask data
  for (Move move : board->getLegalMoves(true, false)) {
    int32_t index = getPolicyIndex(board, move);

    mask[index / 32] |= (1 << (index % 32));
  }

  // Create the value mask data
  int32_t value_index = MODEL_PREDICTIONS * BOARD_SIZE * BOARD_SIZE;

  mask[value_index / 32] |= (1 << (value_index % 32));
}

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
InferenceExecutor::InferenceExecutor(
    InferenceProcessor* processor, std::string file, int32_t gpu, bool fp16,
    bool deterministic, int32_t batchSize, int32_t threads)
    : _mutex(),
      _processor(processor),
      _model(nullptr),
      _file(file),
      _gpu(gpu),
      _fp16(fp16),
      _deterministic(deterministic),
      _batchSize(batchSize),
      _threads(threads),
      _terminated(false),
      _batchFillRates(threads) {
  for (int32_t i = 0; i < threads; i++) {
    _threads[i] = std::thread(&InferenceExecutor::_run, this, i);
    _batchFillRates[i].store(0.0f, std::memory_order_relaxed);
  }
}

/**
 * Destroys the inference manager object.
 */
InferenceExecutor::~InferenceExecutor() {
  // Wait for threads to finish
  for (auto& thread : _threads) {
    thread.join();
  }

  // Destroy the inference model object
  {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_model != nullptr) {
      delete _model;
      _model = nullptr;
    }
  }
}

/**
 * Runs inference synchronously.
 * @param inputs Input data
 * @param masks Output data mask
 * @param outputs Output data
 * @param size Number of evaluation data items
 */
void InferenceExecutor::execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size) {
  // Set the device to use
  torch::DeviceGuard device_guard(InferenceModel::getDevice(_gpu));

  // Get the model object
  // Create the model object if it has not been created yet
  InferenceModel* model = nullptr;

  {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_model == nullptr) {
      _model = new InferenceModel(_file, _gpu, _fp16, _deterministic);
    }

    model = _model;
  }

  // For non-CPU devices, run inference with a fixed batch size
  if (!_model->isCpu()) {
    std::vector<int32_t> input_buffer(_batchSize * MODEL_INPUT_PACK_SIZE);
    std::vector<int32_t> mask_buffer(_batchSize * MODEL_OUTPUT_PACK_SIZE);
    std::vector<float> output_buffer(_batchSize * MODEL_OUTPUT_SIZE);

    for (int32_t i = 0; i < size; i += _batchSize) {
      int32_t batch_size = std::min(_batchSize, size - i);
      int32_t* inputs_begin = inputs + (i * MODEL_INPUT_PACK_SIZE);
      int32_t inputs_size = batch_size * MODEL_INPUT_PACK_SIZE;
      int32_t* masks_begin = masks + (i * MODEL_OUTPUT_PACK_SIZE);
      int32_t masks_size = batch_size * MODEL_OUTPUT_PACK_SIZE;
      float* outputs_begin = outputs + (i * MODEL_OUTPUT_SIZE);
      int32_t outputs_size = batch_size * MODEL_OUTPUT_SIZE;

      std::copy(inputs_begin, inputs_begin + inputs_size, input_buffer.data());
      std::copy(masks_begin, masks_begin + masks_size, mask_buffer.data());
      model->forward(input_buffer.data(), mask_buffer.data(), output_buffer.data(), _batchSize);
      std::copy(output_buffer.data(), output_buffer.data() + outputs_size, outputs_begin);
    }
  } else {
    model->forward(inputs, masks, outputs, size);
  }
}

/**
 * Method executed on a thread.
 * @param threadIndex Thread index
 */
void InferenceExecutor::_run(int32_t threadIndex) {
  // Set the device to use
  torch::DeviceGuard device_guard(InferenceModel::getDevice(_gpu));

  // Create buffers for input data, mask data, and output data
  std::vector<int32_t> input_buffer(_batchSize * MODEL_INPUT_PACK_SIZE);
  std::vector<int32_t> mask_buffer(_batchSize * MODEL_OUTPUT_PACK_SIZE);
  std::vector<float> output_buffer(_batchSize * MODEL_OUTPUT_SIZE);

  // Create the model object if it has not been created yet
  {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_model == nullptr) {
      _model = new InferenceModel(_file, _gpu, _fp16, _deterministic);
    }
  }

  // Repeatedly dequeue scheduled inference executions, build batches,
  // run inference, and apply results to nodes
  while (true) {
    // Dequeue scheduled inference executions and build a batch
    std::vector<std::pair<MctsNode*, InferenceExecutorCallback>> batch;

    {
      // Acquire the lock for synchronization
      std::unique_lock<std::mutex> lock(_processor->_queueMutex);

      // Wait if there are no scheduled inference executions
      // [stop] If a stop request is received and the queue is empty, exit the wait
      // [inference] If there are scheduled inference executions, exit the wait
      _processor->_queueCondition.wait(lock, [this] {
        if (_processor->_terminated && _processor->_queue.empty()) {
          return true;
        } else if (!_processor->_queue.empty()) {
          return true;
        } else {
          return false;
        }
      });

      // Exit the loop if inference processing should terminate
      if (_processor->_terminated && _processor->_queue.empty()) {
        break;
      }

      // Dequeue scheduled inference executions
      while (!_processor->_queue.empty() && batch.size() < _batchSize) {
        batch.push_back(_processor->_queue.front());
        _processor->_queue.pop();
      }

      // Notify other threads that the queue contents have been updated
      _processor->_queueCondition.notify_all();
    }

    // Initialize the input and mask data buffers
    std::fill(input_buffer.begin(), input_buffer.end(), 0);
    std::fill(mask_buffer.begin(), mask_buffer.end(), 0);

    // Create the input and mask data
    for (size_t i = 0; i < batch.size(); i++) {
      MctsNode* node = batch[i].first;
      int32_t* inputs = input_buffer.data() + (i * MODEL_INPUT_PACK_SIZE);
      int32_t* mask = mask_buffer.data() + (i * MODEL_OUTPUT_PACK_SIZE);

      node->getBoard().getInputs(inputs);
      getOutputMask(&node->getBoard(), mask);
    }

    // Run inference
    // For CPU execution, match the batch size to the actual number of items
    // For non-CPU devices, run inference with a fixed batch size
    int32_t batch_size = (_model->isCpu()) ? static_cast<int32_t>(batch.size()) : _batchSize;

    _model->forward(input_buffer.data(), mask_buffer.data(), output_buffer.data(), batch_size);

    // Calculate and store the ratio of inference requests contained in the batch
    float fill_rate = static_cast<float>(batch.size()) / static_cast<float>(batch_size);
    float old_fill_rate = _batchFillRates[threadIndex].load(std::memory_order_relaxed);
    float new_fill_rate = old_fill_rate * 0.99f + fill_rate * 0.01f;

    _batchFillRates[threadIndex].store(new_fill_rate, std::memory_order_relaxed);

    // Apply inference results to nodes and invoke callbacks
    for (size_t i = 0; i < batch.size(); i++) {
      MctsNode* node = batch[i].first;
      InferenceExecutorCallback callback = batch[i].second;
      float* outputs = output_buffer.data() + (i * MODEL_OUTPUT_SIZE);
      std::vector<Move> legal_moves = node->getBoard().getLegalMoves(true, false);
      InferenceResult result;

      // Create the policy inference result
      float total_probability = 0.0f;

      for (Move move : legal_moves) {
        int32_t index = getPolicyIndex(&node->getBoard(), move);
        total_probability += outputs[index];
      }

      for (Move move : legal_moves) {
        int32_t index = getPolicyIndex(&node->getBoard(), move);
        float probability = outputs[index] / (total_probability + 1e-6f);

        result.policies.emplace_back(move, probability);
      }

      // Create the value inference result
      result.value = outputs[MODEL_PREDICTIONS * BOARD_SIZE * BOARD_SIZE] * 2.0f - 1.0f;

      if (node->getBoard().getColor() == COLOR_WHITE) {
        result.value = -result.value;
      }

      // Invoke the callback function
      callback(node, result);
    }
  }
}

}  // namespace deepshogi

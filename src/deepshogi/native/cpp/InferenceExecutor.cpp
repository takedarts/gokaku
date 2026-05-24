#include "InferenceExecutor.h"

namespace deepshogi {

constexpr int32_t DIR_H = 0;    // 移動:打
constexpr int32_t DIR_U = 1;    // 移動:上
constexpr int32_t DIR_D = 2;    // 移動:下
constexpr int32_t DIR_R = 3;    // 移動:右
constexpr int32_t DIR_L = 4;    // 移動:左
constexpr int32_t DIR_UR = 5;   // 移動:右上
constexpr int32_t DIR_UL = 6;   // 移動:左上
constexpr int32_t DIR_DR = 7;   // 移動:右下
constexpr int32_t DIR_DL = 8;   // 移動:左下
constexpr int32_t DIR_KR = 9;   // 移動:桂馬右
constexpr int32_t DIR_KL = 10;  // 移動:桂馬左

/**
 * 指定された着手のPolicyインデックスを取得する。
 * @param board 盤面
 * @param move 着手
 * @return Policyインデックス
 */
static int32_t getPolicyIndex(const Board* board, Move move) {
  // 駒番号と移動方向を計算する
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

  // 後手番の場合は移動座標を反転する
  if (board->getColor() == COLOR_WHITE) {
    move_x = -move_x;
    move_y = -move_y;
  }

  // 移動方向を判定する
  int32_t dir = 0;

  if (move_x == 0 && move_y == 0) {  // 打
    dir = DIR_H;
  } else if (move_x == 0 && move_y < 0) {  // 上
    dir = DIR_U;
  } else if (move_x == 0 && move_y > 0) {  // 下
    dir = DIR_D;
  } else if (move_x < 0 && move_y == 0) {  // 右
    dir = DIR_R;
  } else if (move_x > 0 && move_y == 0) {  // 左
    dir = DIR_L;
  } else if (move_x < 0 && move_y == move_x) {  // 右上
    dir = DIR_UR;
  } else if (move_x > 0 && move_y == -move_x) {  // 左上
    dir = DIR_UL;
  } else if (move_x < 0 && move_y == -move_x) {  // 右下
    dir = DIR_DR;
  } else if (move_x > 0 && move_y == move_x) {  // 左下
    dir = DIR_DL;
  } else if (move_x == -1 && move_y == -2) {  // 桂馬右
    dir = DIR_KR;
  } else if (move_x == 1 && move_y == -2) {  // 桂馬左
    dir = DIR_KL;
  } else {
    throw std::invalid_argument("Invalid move direction");
  }

  // Policyインデックスを計算する
  int32_t index = 0;

  if (piece == 0) {  // 歩
    index =
        0 + ((dir == DIR_U)   ? 0
             : (dir == DIR_H) ? 1
                              : -1);
  } else if (piece == 1) {  // 香車
    index =
        2 + ((dir == DIR_U)   ? 0
             : (dir == DIR_H) ? 1
                              : -3);
  } else if (piece == 2) {  // 桂馬
    index =
        4 + ((dir == DIR_KR)   ? 0
             : (dir == DIR_KL) ? 1
             : (dir == DIR_H)  ? 2
                               : -5);
  } else if (piece == 3) {  // 銀
    index =
        7 + ((dir == DIR_U)    ? 0
             : (dir == DIR_UR) ? 1
             : (dir == DIR_UL) ? 2
             : (dir == DIR_DR) ? 3
             : (dir == DIR_DL) ? 4
             : (dir == DIR_H)  ? 5
                               : -8);
  } else if (piece == 4) {  // 角
    index =
        13 + ((dir == DIR_UR)   ? 0
              : (dir == DIR_UL) ? 1
              : (dir == DIR_DR) ? 2
              : (dir == DIR_DL) ? 3
              : (dir == DIR_H)  ? 4
                                : -14);
  } else if (piece == 5) {  // 飛車
    index =
        18 + ((dir == DIR_U)   ? 0
              : (dir == DIR_D) ? 1
              : (dir == DIR_R) ? 2
              : (dir == DIR_L) ? 3
              : (dir == DIR_H) ? 4
                               : -19);
  } else if (piece == 6) {  // 金
    index =
        23 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_H)  ? 6
                                : -24);
  } else if (piece == 7) {  // 玉
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
  } else if (piece == 8) {  // と金
    index =
        38 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
                                : -39);
  } else if (piece == 9) {  // 成香
    index =
        44 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
                                : -45);
  } else if (piece == 10) {  // 成桂
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
  } else if (piece == 11) {  // 成銀
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
  } else if (piece == 12) {  // 馬
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
  } else if (piece == 13) {  // 龍
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

  // 移動先の座標を取得する
  int32_t x = move.getDst().getX();
  int32_t y = move.getDst().getY();

  if (board->getColor() == COLOR_WHITE) {
    x = BOARD_SIZE - 1 - x;
    y = BOARD_SIZE - 1 - y;
  }

  // 番号を返す
  return (index * BOARD_SIZE * BOARD_SIZE) + (x * BOARD_SIZE + y);
}

/**
 * 指定された盤面の出力マスクデータを作成する。
 * @param board 盤面
 * @param mask マスクデータのバッファ
 */
static void getOutputMask(const Board* board, int32_t* mask) {
  // マスクデータを0で初期化する
  std::fill(mask, mask + MODEL_OUTPUT_PACK_SIZE, 0);

  // Policyのマスクデータを作成する
  for (Move move : board->getLegalMoves(true, false)) {
    int32_t index = getPolicyIndex(board, move);

    mask[index / 32] |= (1 << (index % 32));
  }

  // Valueのマスクデータを作成する
  int32_t value_index = MODEL_PREDICTIONS * BOARD_SIZE * BOARD_SIZE;

  mask[value_index / 32] |= (1 << (value_index % 32));
}

/**
 * 推論実行オブジェクトを生成する。
 * @param model 推論モデルファイルのパス
 * @param gpu 使用するGPUのID
 * @param fp16 半精度浮動小数点を使用するならTrue
 * @param deterministic 決定論的動作を行うならTrue
 * @param batchSize バッチサイズ
 * @param threads 実行するスレッド数
 */
InferenceExecutor::InferenceExecutor(
    std::string model, int32_t gpu, bool fp16, bool deterministic,
    int32_t batchSize, int32_t threads)
    : _modelMutex(),
      _threadMutex(),
      _condition(),
      _model(nullptr),
      _modelFile(model),
      _gpu(gpu),
      _fp16(fp16),
      _deterministic(deterministic),
      _batchSize(batchSize),
      _threads(threads),
      _terminated(false),
      _queue() {
  for (int32_t i = 0; i < threads; i++) {
    _threads[i] = std::thread(&InferenceExecutor::_run, this);
  }
}

/**
 * 推論管理オブジェクトを破棄する。
 */
InferenceExecutor::~InferenceExecutor() {
  // 推論処理を終了する
  {
    std::lock_guard<std::mutex> lock(_threadMutex);
    _terminated = true;
  }

  _condition.notify_all();

  // スレッドの終了を待機する
  for (auto& thread : _threads) {
    thread.join();
  }

  // 推論モデルオブジェクトを破棄する
  {
    std::lock_guard<std::mutex> lock(_modelMutex);

    if (_model != nullptr) {
      delete _model;
      _model = nullptr;
    }
  }
}

/**
 * 推論実行を予約する。
 * 推論計算は非同期に実行されるため、この関数はすぐに返る。
 * 推論計算が完了すると、ノードの評価値が更新される。
 * @param node 推論を実行するノード
 * @param callback 推論計算の完了を通知するコールバック関数
 */
void InferenceExecutor::submit(MctsNode* node, InferenceExecutorCallback callback) {
  std::lock_guard<std::mutex> lock(_threadMutex);
  _queue.emplace_back(node, callback);
  _condition.notify_one();
}

/**
 * 推論を同期的に実行する。
 * @param inputs 入力データ
 * @param masks 出力データのマスク
 * @param outputs 出力データ
 * @param size 評価データの数
 */
void InferenceExecutor::execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size) {
  // 使用するデバイスを設定する
  torch::DeviceGuard device_guard(InferenceModel::getDevice(_gpu));

  // モデルオブジェクトを取得する
  // モデルオブジェクトが作成されていない場合は作成する
  InferenceModel* model = nullptr;

  {
    std::lock_guard<std::mutex> lock(_modelMutex);

    if (_model == nullptr) {
      _model = new InferenceModel(_modelFile, _gpu, _fp16, _deterministic);
    }

    model = _model;
  }

  // CPU以外で計算するなら固定されたバッチサイズで推論を実行する
  if (!_model->isCpu()) {
    std::vector<int32_t> input_buffer(_batchSize * MODEL_INPUT_PACK_SIZE);
    std::vector<int32_t> mask_buffer(_batchSize * MODEL_OUTPUT_PACK_SIZE);
    std::vector<float> output_buffer(_batchSize * MODEL_OUTPUT_SIZE);

    std::copy(inputs, inputs + (size * MODEL_INPUT_PACK_SIZE), input_buffer.data());
    std::copy(masks, masks + (size * MODEL_OUTPUT_PACK_SIZE), mask_buffer.data());
    model->forward(input_buffer.data(), mask_buffer.data(), output_buffer.data(), _batchSize);
    std::copy(output_buffer.data(), output_buffer.data() + (size * MODEL_OUTPUT_SIZE), outputs);
  } else {
    model->forward(inputs, masks, outputs, size);
  }
}

/**
 * 推論実行の予約のキューに入っている待機中の推論実行の数を返す。
 * @return 推論実行の予約のキューに入っている待機中の推論実行の数
 */
int32_t InferenceExecutor::getQueueSize() {
  std::lock_guard<std::mutex> lock(_threadMutex);
  return static_cast<int32_t>(_queue.size());
}

/**
 * スレッドで実行されるメソッド。
 */
void InferenceExecutor::_run() {
  // 使用するデバイスを設定する
  torch::DeviceGuard device_guard(InferenceModel::getDevice(_gpu));

  // 入力データとマスクデータと出力データのバッファを作成する
  std::vector<int32_t> input_buffer(_batchSize * MODEL_INPUT_PACK_SIZE);
  std::vector<int32_t> mask_buffer(_batchSize * MODEL_OUTPUT_PACK_SIZE);
  std::vector<float> output_buffer(_batchSize * MODEL_OUTPUT_SIZE);

  // モデルオブジェクトが作成されていない場合は作成する
  {
    std::lock_guard<std::mutex> lock(_modelMutex);

    if (_model == nullptr) {
      _model = new InferenceModel(_modelFile, _gpu, _fp16, _deterministic);
    }
  }

  // キューから推論実行の予約を取り出してバッチを作成し、
  // 推論を実行して結果をノードに適用する処理を繰り返す
  while (true) {
    // キューから推論実行の予約を取り出してバッチを作成する
    std::vector<std::pair<MctsNode*, InferenceExecutorCallback>> batch;

    {
      // 同期処理を行うためのロックを取得する
      std::unique_lock<std::mutex> lock(_threadMutex);

      // 推論実行の予約がない場合は待機する
      // [停止] 停止要求があり、キューが空である場合は待機を終了する
      // [推論実行] 推論実行の予約がある場合は待機を終了する
      _condition.wait(lock, [this] {
        if (_terminated && _queue.empty()) {
          return true;
        } else if (!_queue.empty()) {
          return true;
        } else {
          return false;
        }
      });

      // 推論処理を終了するならばループを抜ける
      if (_terminated && _queue.empty()) {
        break;
      }

      // 推論実行の予約をキューから取り出す
      while (!_queue.empty() && batch.size() < _batchSize) {
        batch.push_back(_queue.back());
        _queue.pop_back();
      }
    }

    // 入力データとマスクデータのバッファを初期化する
    std::fill(input_buffer.begin(), input_buffer.end(), 0);
    std::fill(mask_buffer.begin(), mask_buffer.end(), 0);

    // 入力データとマスクデータを作成する
    for (size_t i = 0; i < batch.size(); i++) {
      MctsNode* node = batch[i].first;
      int32_t* inputs = input_buffer.data() + (i * MODEL_INPUT_PACK_SIZE);
      int32_t* mask = mask_buffer.data() + (i * MODEL_OUTPUT_PACK_SIZE);

      node->getBoard().getInputs(inputs);
      getOutputMask(&node->getBoard(), mask);
    }

    // 推論を実行する
    // CPUで実行する場合はバッチサイズを実際のバッチサイズに合わせる
    // CPU以外で計算するなら固定されたバッチサイズで推論を実行する
    int32_t batch_size = (_model->isCpu()) ? static_cast<int32_t>(batch.size()) : _batchSize;

    _model->forward(input_buffer.data(), mask_buffer.data(), output_buffer.data(), batch_size);

    // 推論結果をノードに適用してコールバック関数を呼び出す
    for (size_t i = 0; i < batch.size(); i++) {
      MctsNode* node = batch[i].first;
      InferenceExecutorCallback callback = batch[i].second;
      float* outputs = output_buffer.data() + (i * MODEL_OUTPUT_SIZE);
      std::vector<Move> legal_moves = node->getBoard().getLegalMoves(true, false);
      InferenceResult result;

      // Policyの推論結果を作成する
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

      // Valueの推論結果を作成する
      result.value = outputs[MODEL_PREDICTIONS * BOARD_SIZE * BOARD_SIZE] * 2.0f - 1.0f;

      if (node->getBoard().getColor() == COLOR_WHITE) {
        result.value = -result.value;
      }

      // コールバック関数を呼び出す
      callback(node, result);
    }
  }
}

}  // namespace deepshogi

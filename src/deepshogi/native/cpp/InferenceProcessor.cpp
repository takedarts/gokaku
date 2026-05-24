#include "InferenceProcessor.h"

namespace deepshogi {

/**
 * 推論管理オブジェクトを生成する。
 * @param model モデルファイル
 * @param gpus GPU番号のリスト
 * @param fp16 16bit精度で計算するならTrue
 * @param deterministic 計算結果を再現可能にするならTrue
 * @param batchSize バッチサイズ
 * @param threadsPerGpu GPUごとのスレッド数
 * @param cacheSize 推論結果のキャッシュサイズ
 */
InferenceProcessor::InferenceProcessor(
    std::string model, std::vector<int32_t> gpus, bool fp16, bool deterministic,
    int32_t batchSize, int32_t threadsPerGpu, int32_t cacheSize)
    : _mutex(),
      _executors(),
      _threadSize(static_cast<int32_t>(gpus.size()) * threadsPerGpu),
      _cacheSize(cacheSize),
      _cacheKeys(),
      _cacheResults(),
      _gpus(gpus),
      _batchSize(batchSize) {
  for (int32_t gpu : gpus) {
    _executors.emplace_back(std::make_unique<InferenceExecutor>(
        model, gpu, fp16, deterministic, batchSize, threadsPerGpu));
  }
}

/**
 * 推論実行を予約する。
 * 推論計算は非同期に実行されるため、この関数はすぐに返る。
 * 推論計算が完了すると、ノードの評価値が更新される。
 * @param node 推論を実行するノード
 * @param callback 推論計算の完了を通知するコールバック関数
 */
void InferenceProcessor::submit(
    MctsNode* node, std::function<void(MctsNode*)> callback) {
  // キャッシュされた推論結果が見つかったときに使用する変数
  InferenceResult cached_result;
  // キャッシュされた推論結果が見つかったかどうかを示すフラグ
  bool cached_result_found = false;
  // 推論実行オブジェクトのインデックス
  int32_t executor_index;

  // 同期処理を行うためのロックを取得する
  {
    std::lock_guard<std::mutex> lock(_mutex);

    // キャッシュされた推論結果があるかを確認する
    // キャッシュされた推論結果がある場合は、その推論結果を保存してフラグを立てる
    // ノードへの適用とコールバック関数の呼び出しはロックの外で行う
    BoardHash boardHash(&node->getBoard());
    auto it = _cacheResults.find(boardHash);

    if (it != _cacheResults.end()) {
      cached_result = it->second;
      cached_result_found = true;
    }

    // 最も使用されていない推論実行オブジェクトを選択する
    executor_index = 0;
    size_t min_queue_size = _executors[0]->getQueueSize();

    for (size_t i = 1; i < _executors.size(); i++) {
      size_t queue_size = _executors[i]->getQueueSize();

      if (queue_size < min_queue_size) {
        executor_index = static_cast<int32_t>(i);
        min_queue_size = queue_size;
      }
    }
  }

  // キャッシュされた推論結果が見つかった場合は、それを使用する
  if (cached_result_found) {
    node->applyInferenceResult(cached_result.value, cached_result.policies);
    callback(node);
    return;
  }

  // 推論終了時のコールバック関数を定義する
  auto exec_callback = [this, node, callback](MctsNode*, const InferenceResult& result) {
    // 同期処理を行うためのロックを取得する
    {
      std::lock_guard<std::mutex> lock(_mutex);

      // キャッシュに結果があるかを確認する
      // 結果がない場合は、キャッシュに保存する
      BoardHash boardHash(&node->getBoard());
      auto it = _cacheResults.find(boardHash);

      if (it == _cacheResults.end()) {
        _cacheResults[boardHash] = result;
        _cacheKeys.push(boardHash);
      }

      // キャッシュサイズを超える古いキャッシュを削除する
      if (_cacheKeys.size() >= _cacheSize) {
        _cacheResults.erase(_cacheKeys.front());
        _cacheKeys.pop();
      }
    }

    // ノードに推論結果を適用する
    node->applyInferenceResult(result.value, result.policies);

    // コールバック関数を呼び出す
    callback(node);
  };

  // 推論実行オブジェクトに推論実行を予約する
  _executors[executor_index]->submit(node, exec_callback);
}

/**
 * 指定された盤面の評価値を取得する。
 * @param board 評価する盤面
 * @return 評価値
 */
float InferenceProcessor::predict(Board* board) {
  // ダミーのノードオブジェクトを生成する
  MctsManager manager(MctsParameter(28, 27, 512, 1.0f, 1.0f, 18200.0f));
  MctsNode node(&manager);

  // ノードオブジェクトに盤面を設定する
  node.initialize(board->getSfen());

  // 推論処理の終了を待つための同期オブジェクトと条件変数を生成する
  std::mutex mutex;
  std::condition_variable cv;

  // 推論処理を実行してノードの評価値が更新されるのを待つ
  {
    std::unique_lock<std::mutex> lock(mutex);
    submit(&node, [&cv](MctsNode*) { cv.notify_one(); });
    cv.wait(lock, [&node] { return node.isEvaluated(); });
  }

  return node.getNodeValue();
}

/**
 * 推論を同期的に実行する。
 * @param inputs 入力データ
 * @param masks 出力データのマスク
 * @param outputs 出力データ
 * @param size 評価データの数
 */
void InferenceProcessor::execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size) {
  _executors[0]->execute(inputs, masks, outputs, size);
}

}  // namespace deepshogi

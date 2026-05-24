#pragma once

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "Board.h"
#include "BoardHash.h"
#include "InferenceExecutor.h"
#include "InferenceResult.h"
#include "MctsNode.h"

namespace deepshogi {

/**
 * 盤面評価のための推論実行を管理するクラス。
 */
class InferenceProcessor {
 public:
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
  InferenceProcessor(
      std::string model, std::vector<int32_t> gpus, bool fp16, bool deterministic,
      int32_t batchSize, int32_t threadsPerGpu, int32_t cacheSize);

  /**
   * 推論管理オブジェクトを破棄する。
   */
  virtual ~InferenceProcessor() = default;

  /**
   * 推論実行を予約する。
   * 推論計算は非同期に実行されるため、この関数はすぐに返る。
   * 推論計算が完了すると、ノードの評価値が更新される。
   * @param node 推論を実行するノード
   * @param callback 推論計算の完了を通知するコールバック関数
   */
  void submit(MctsNode* node, std::function<void(MctsNode*)> callback);

  /**
   * 指定された盤面の評価値を取得する。
   * @param board 評価する盤面
   * @return 評価値
   */
  float predict(Board* board);

  /**
   * 推論を同期的に実行する。
   * @param inputs 入力データ
   * @param masks 出力データのマスク
   * @param outputs 出力データ
   * @param size 評価データの数
   */
  void execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size);

  /**
   * バッチサイズを取得する。
   * @return バッチサイズ
   */
  inline int32_t getBatchSize() const {
    return _batchSize;
  }

  /**
   * 推論を実行するためのスレッドの数を取得する。
   * @return 推論を実行するためのスレッドの数
   */
  inline int32_t getThreadSize() const {
    return _threadSize;
  }

 private:
  /**
   * 同期用のミューテックスオブジェクト。
   */
  std::mutex _mutex;

  /**
   * 推論実行オブジェクトの一覧。
   */
  std::vector<std::unique_ptr<InferenceExecutor>> _executors;

  /**
   * 推論を実行するためのスレッドの数。
   */
  int32_t _threadSize;

  /**
   * 推論結果のキャッシュサイズ。
   */
  int32_t _cacheSize;

  /**
   * 推論結果のキャッシュのキーのキュー。
   * キャッシュサイズを超える古いキャッシュを削除するために使用する。
   */
  std::queue<BoardHash> _cacheKeys;

  /**
   * 推論結果のキャッシュ。
   * キーは盤面のハッシュ値、値は推論結果。
   */
  std::map<BoardHash, InferenceResult> _cacheResults;

  /**
   * 使用するGPUのIDのリスト。
   */
  std::vector<int32_t> _gpus;

  /**
   * バッチサイズ。
   */
  int32_t _batchSize;
};

}  // namespace deepshogi

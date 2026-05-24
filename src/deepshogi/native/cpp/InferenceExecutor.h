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
 * 推論が終了したときに呼び出されるコールバック関数の型。
 */
typedef std::function<void(MctsNode*, const InferenceResult&)> InferenceExecutorCallback;

/**
 * 盤面評価のための推論を実行するクラス。
 */
class InferenceExecutor {
 public:
  /**
   * 推論実行オブジェクトを生成する。
   * @param model 推論モデルファイルのパス
   * @param gpu 使用するGPUのID
   * @param fp16 半精度浮動小数点を使用するならTrue
   * @param deterministic 決定論的動作を行うならTrue
   * @param batchSize バッチサイズ
   * @param threads 実行するスレッド数
   */
  InferenceExecutor(
      std::string model, int32_t gpu, bool fp16, bool deterministic,
      int32_t batchSize, int32_t threads);

  /**
   * 推論管理オブジェクトを破棄する。
   */
  virtual ~InferenceExecutor();

  /**
   * 推論実行を予約する。
   * 推論計算は非同期に実行されるため、この関数はすぐに返る。
   * 推論計算が完了すると、ノードの評価値が更新される。
   * @param node 推論を実行するノード
   * @param callback 推論計算の完了を通知するコールバック関数
   */
  void submit(MctsNode* node, InferenceExecutorCallback callback);

  /**
   * 推論を同期的に実行する。
   * @param inputs 入力データ
   * @param masks 出力データのマスク
   * @param outputs 出力データ
   * @param size 評価データの数
   */
  void execute(int32_t* inputs, int32_t* masks, float* outputs, int32_t size);

  /**
   * 推論実行の予約のキューに入っている待機中の推論実行の数を返す。
   * @return 推論実行の予約のキューに入っている待機中の推論実行の数
   */
  int32_t getQueueSize();

 private:
  /**
   * モデル同期用のミューテックスオブジェクト。
   */
  std::mutex _modelMutex;

  /**
   * スレッド同期用のミューテックスオブジェクト。
   */
  std::mutex _threadMutex;

  /**
   * 同期用の条件変数。
   */
  std::condition_variable _condition;

  /**
   * 推論モデルオブジェクト。
   */
  InferenceModel* _model;

  /**
   * 推論モデルファイルのパス。
   */
  std::string _modelFile;

  /**
   * 使用するGPUのID。
   */
  int32_t _gpu;

  /**
   * 半精度浮動小数点を使用するかどうか。
   */
  bool _fp16;

  /**
   * 決定論的動作を行うかどうか。
   */
  bool _deterministic;

  /**
   * バッチサイズ。
   */
  int32_t _batchSize;

  /**
   * 推論を実行するためのスレッドオブジェクトの一覧。
   */
  std::vector<std::thread> _threads;

  /**
   * 推論処理を終了するならTrue。
   */
  bool _terminated;

  /**
   * 推論実行の予約のキュー。
   */
  std::vector<std::pair<MctsNode*, InferenceExecutorCallback>> _queue;

  /**
   * スレッドで実行されるメソッド。
   */
  void _run();
};

}  // namespace deepshogi

#pragma once

#include <ATen/ATen.h>
#include <torch/script.h>
#include <torch/torch.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace deepshogi {

/**
 * モデルを使った推論を実行するクラス。
 */
class InferenceModel {
 public:
  /**
   * 利用可能なGPUの番号を取得する。
   * @return GPUの番号のリスト
   */
  static std::vector<std::int32_t> getAvailableGPUs();
  /**
   * 実行環境で使用できるデバイスを取得する。
   * @param gpu GPU番号
   */
  static at::Device getDevice(int32_t gpu);

  /**
   * 実行環境で使用できるデータ型を取得する。
   * @param gpu GPU番号
   * @param fp16 16bit精度で計算するならTrue
   */
  static at::ScalarType getScalarType(int32_t gpu, bool fp16);

  /**
   * モデルオブジェクトを作成する。
   * GPU番号に-1を指定するとCPUで計算するオブジェクトを生成する。
   * @param filename モデルファイルのパス
   * @param gpu GPU番号
   * @param fp16 16bit精度で計算するならTrue
   * @param deterministic 計算結果を再現可能にするならTrue
   */
  InferenceModel(std::string filename, int32_t gpu, bool fp16, bool deterministic);

  /**
   * デストラクタ。
   */
  virtual ~InferenceModel() = default;

  /**
   * 推論を実行する。
   * @param inputs 入力データ
   * @param masks 出力データのマスク
   * @param outputs 出力データ
   * @param size 評価データの数
   */
  void forward(int32_t* inputs, int32_t* masks, float* outputs, int32_t size);

  /**
   * CUDAを使うならTrueを返す。
   * @return CUDAを使うならTrue
   */
  inline bool isCuda() const {
    return _device.is_cuda();
  }

  /**
   * CPUを使うならTrueを返す。
   * @return CPUを使うならTrue
   */
  inline bool isCpu() const {
    return _cpu;
  }

 private:
  /**
   * 入出力の同期を取るためのオブジェクト。
   */
  std::mutex _ioMutex;

  /**
   * 計算時の同期を取るためのオブジェクト。
   */
  std::mutex _computeMutex;

  /**
   * 推論モデル。
   */
  torch::jit::script::Module _model;

  /**
   * 実行デバイス。
   */
  at::Device _device;

  /**
   * 計算で使用するデータ型。
   */
  at::ScalarType _dtype;

  /**
   * ビットシフト用テンソル。
   */
  torch::Tensor _bitShift;

  /**
   * CPUで計算するならTrue。
   */
  bool _cpu;
};

}  // namespace deepshogi

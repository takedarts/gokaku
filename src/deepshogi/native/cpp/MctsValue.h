#pragma once

#include <cstdint>
#include <mutex>

namespace deepshogi {

/**
 * MCTSの評価値を管理するクラス。
 * 評価値の合計と評価回数を管理する。
 * スレッドセーフに評価値の平均を取得できるようにする。
 */
class MctsValue {
 public:
  /**
   * MCTSの評価値オブジェクトを生成する。
   */
  MctsValue();

  /**
   * MCTSの評価値オブジェクトをコピーする。
   * @param other コピー元のオブジェクト
   */
  MctsValue(const MctsValue& other);

  /**
   * MCTSの評価値オブジェクトを破棄する。
   */
  virtual ~MctsValue() = default;

  /**
   * MCTSの評価値をリセットする。
   */
  void reset();

  /**
   * MCTSの評価値を更新する。
   * @param value 評価値
   */
  void update(float value);

  /**
   * MCTSの評価値を設定する。
   * @param value 評価値
   */
  void setValue(float value);

  /**
   * MCTSの評価値の平均を取得する。
   * @param defaultValue 評価回数が0の場合に返す値
   * @return 評価値の平均
   */
  float getValue(float defaultValue);

  /**
   * MCTSの評価値の信頼区間の下限を取得する。
   * @param color 手番（COLOR_BLACKまたはCOLOR_WHITE）
   * @param defaultValue 評価回数が0の場合に評価値とする値
   * @return 評価値の信頼区間の下限
   */
  float getValueLCB(int8_t color, float defaultValue);

 private:
  /**
   *  同期するためのミューテックス。
   */
  std::mutex _mutex;

  /**
   * 評価値の合計。
   */
  float _value;

  /**
   * 評価回数。
   */
  int32_t _count;
};

}  // namespace deepshogi

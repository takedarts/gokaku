#include "MctsValue.h"

#include <cmath>

#include "Config.h"

namespace deepshogi {

/**
 * MCTSの評価値オブジェクトを生成する。
 */
MctsValue::MctsValue()
    : _mutex(),
      _value(0.0f),
      _count(0) {
}

/**
 * MCTSの評価値オブジェクトをコピーする。
 * @param other コピー元のオブジェクト
 */
MctsValue::MctsValue(const MctsValue& other)
    : _mutex(),
      _value(other._value),
      _count(other._count) {
}

/**
 * MCTSの評価値をリセットする。
 */
void MctsValue::reset() {
  std::lock_guard<std::mutex> lock(_mutex);
  _value = 0.0f;
  _count = 0;
}

/**
 * MCTSの評価値を更新する。
 * @param value 評価値
 */
void MctsValue::update(float value) {
  std::lock_guard<std::mutex> lock(_mutex);
  _value += value;
  _count++;
}

/**
 * MCTSの評価値を設定する。
 * @param value 評価値
 */
void MctsValue::setValue(float value) {
  std::lock_guard<std::mutex> lock(_mutex);
  _value = value * _count;
}

/**
 * MCTSの評価値の平均を取得する。
 * @param defaultValue 評価回数が0の場合に返す値
 * @return 評価値の平均
 */
float MctsValue::getValue(float defaultValue) {
  std::lock_guard<std::mutex> lock(_mutex);
  return (_count != 0) ? _value / _count : defaultValue;
}

/**
 * MCTSの評価値の信頼区間の下限を取得する。
 * @param color 手番（COLOR_BLACKまたはCOLOR_WHITE）
 * @param defaultValue 評価回数が0の場合に返す値
 * @return 評価値の信頼区間の下限
 */
float MctsValue::getValueLCB(int8_t color, float defaultValue) {
  std::lock_guard<std::mutex> lock(_mutex);
  float value = (_count != 0) ? _value / _count : defaultValue;
  float lower = 1.96f * 0.5f / std::sqrt((float)(_count + 1));

  return value - (lower * color);
}

}  // namespace deepshogi

#pragma once

#include "Board.h"
#include "Config.h"
#include "Policy.h"
#include "Processor.h"

namespace deepshogi {

/**
 * 評価結果を格納するクラス。
 */
class Evaluator {
 public:
  /**
   * 評価結果オブジェクトを作成する。
   * @param processor 推論を実行するオブジェクト
   */
  Evaluator(Processor* processor);

  /**
   * モデルによる評価結果をクリアする。
   */
  void clear();

  /**
   * モデルによる評価を実行する。
   * @param board 評価対象の盤面
   */
  void evaluate(Board* board);

  /**
   * モデルによる評価結果が設定されていればtrueを返す。
   * @return モデルによる評価結果が設定されていればtrue
   */
  bool isEvaluated();

  /**
   * モデルによる推論結果の予測候補手の一覧を取得する。
   * @return 予測候補手の一覧
   */
  std::vector<Policy> getPolicies();

  /**
   * モデルによる推論結果の予想勝率を取得する。
   * @return モデルによる推論結果の予想勝率
   */
  float getValue();

 private:
  /**
   * 推論を実行するオブジェクト。
   */
  Processor* _processor;

  /**
   * モデルによる推論結果の候補手の一覧
   */
  std::vector<Policy> _policies;

  /**
   * モデルによる推論結果の予想勝率。
   */
  float _value;

  /**
   * モデルによる評価結果が設定されていればtrue。
   */
  bool _evaluated;
};

}  // namespace deepshogi

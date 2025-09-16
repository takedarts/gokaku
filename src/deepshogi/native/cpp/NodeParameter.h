#pragma once

#include "Processor.h"

namespace deepshogi {

/**
 * ノードオブジェクトの設定パラメータクラス。
 */
class NodeParameter {
 public:
  /**
   * パラメータオブジェクトを作成する。
   * @param processor 推論を実行するオブジェクト
   * @param nyugyokuScoreBlack 先手番の入玉宣言に必要となる点数
   * @param nyugyokuScoreWhite 後手番の入玉宣言に必要となる点数
   * @param drawSteps 引き分けとなるまでの手数
   * @param checkSearchDepth 詰み手筋の探索深さ
   * @param checkSearchNode 詰み手筋の探索ノード数
   */
  NodeParameter(
      Processor* processor,
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps,
      int32_t checkSearchDepth, int32_t checkSearchNode);

  /**
   * パラメータオブジェクトを破棄する。
   */
  virtual ~NodeParameter() = default;

  /**
   * 推論を実行するオブジェクトを返す。
   * @return 推論を実行するオブジェクト
   */
  Processor* getProcessor() const;

  /**
   * 先手番の入玉宣言に必要となる点数を取得する。
   * @return 先手番の入玉宣言に必要となる点数
   */
  int32_t getNyugyokuScoreBlack() const;

  /**
   * 後手番の入玉宣言に必要となる点数を取得する。
   * @return 後手番の入玉宣言に必要となる点数
   */
  int32_t getNyugyokuScoreWhite() const;

  /**
   * 引き分けとなるまでの手数を取得する。
   * @return 引き分けとなるまでの手数
   */
  int32_t getDrawSteps() const;

  /**
   * 詰み手筋の探索深さを取得する。
   * @return 詰み手筋の探索深さ
   */
  int32_t getCheckSearchDepth() const;

  /**
   * 詰み手筋の探索ノード数を取得する。
   * @return 詰み手筋の探索ノード数
   */
  int32_t getCheckSearchNode() const;

 private:
  /**
   * 推論を実行するオブジェクト。
   */
  Processor* _processor;

  /**
   * 先手番の入玉宣言に必要となる点数。
   */
  int32_t _nyugyokuScoreBlack;

  /**
   * 後手番の入玉宣言に必要となる点数。
   */
  int32_t _nyugyokuScoreWhite;

  /**
   * 引き分けとなるまでの手数。
   */
  int32_t _drawSteps;

  /**
   * 詰み手筋の探索深さ。
   */
  int32_t _checkSearchDepth;

  /**
   * 詰み手筋の探索ノード数。
   */
  int32_t _checkSearchNode;
};

}  // namespace deepshogi

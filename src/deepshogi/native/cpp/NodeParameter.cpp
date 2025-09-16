#include "NodeParameter.h"

namespace deepshogi {

/**
 * パラメータオブジェクトを作成する。
 * @param processor 推論を実行するオブジェクト
 * @param nyugyokuScoreBlack 先手番の入玉宣言に必要となる点数
 * @param nyugyokuScoreWhite 後手番の入玉宣言に必要となる点数
 * @param drawSteps 引き分けとなるまでの手数
 * @param checkSearchDepth 詰み手筋の探索深さ
 * @param checkSearchNode 詰み手筋の探索ノード数
 */
NodeParameter::NodeParameter(
    Processor* processor,
    int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps,
    int32_t checkSearchDepth, int32_t checkSearchNode)
    : _processor(processor),
      _nyugyokuScoreBlack(nyugyokuScoreBlack),
      _nyugyokuScoreWhite(nyugyokuScoreWhite),
      _checkSearchDepth(checkSearchDepth),
      _checkSearchNode(checkSearchNode) {
}

/**
 * 推論を実行するオブジェクトを返す。
 * @return 推論を実行するオブジェクト
 */
Processor* NodeParameter::getProcessor() const {
  return _processor;
}

/**
 * 先手番の入玉宣言に必要となる点数を取得する。
 * @return 先手番の入玉宣言に必要となる点数
 */
int32_t NodeParameter::getNyugyokuScoreBlack() const {
  return _nyugyokuScoreBlack;
}

/**
 * 後手番の入玉宣言に必要となる点数を取得する。
 * @return 後手番の入玉宣言に必要となる点数
 */
int32_t NodeParameter::getNyugyokuScoreWhite() const {
  return _nyugyokuScoreWhite;
}

/**
 * 引き分けとなるまでの手数を取得する。
 * @return 引き分けとなるまでの手数
 */
int32_t NodeParameter::getDrawSteps() const {
  return _drawSteps;
}

/**
 * 詰み手筋の探索深さを取得する。
 * @return 詰み手筋の探索深さ
 */
int32_t NodeParameter::getCheckSearchDepth() const {
  return _checkSearchDepth;
}

/**
 * 詰み手筋の探索ノード数を取得する。
 * @return 詰み手筋の探索ノード数
 */
int32_t NodeParameter::getCheckSearchNode() const {
  return _checkSearchNode;
}

}  // namespace deepshogi

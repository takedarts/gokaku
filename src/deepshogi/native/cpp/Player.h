#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <vector>

#include "Board.h"
#include "Candidate.h"
#include "Config.h"
#include "Node.h"
#include "NodeManager.h"
#include "Processor.h"
#include "ThreadPool.h"

namespace deepshogi {

/**
 * ゲームを進行するプレイヤを表すクラス。
 */
class Player {
 public:
  /**
   * プレイヤオブジェクトを作成する。
   * @param processor 推論を実行するオブジェクト
   * @param threads スレッドの数
   * @param nyugyokuScoreBlack 先手番の入玉宣言に必要となる点数
   * @param nyugyokuScoreWhite 後手番の入玉宣言に必要となる点数
   * @param drawSteps 引き分けとなるまでの手数
   * @param checkSearchDepth 詰み手筋の探索深さ
   * @param checkSearchNode 詰み手筋の探索ノード数
   * @param evalLeafOnly 葉ノードのみ評価するならばtrue
   */
  Player(
      Processor* processor, int32_t threads,
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawSteps,
      int32_t checkSearchDepth, int32_t checkSearchNode, bool evalLeafOnly);

  /**
   * プレイヤオブジェクトを破棄する。
   */
  virtual ~Player();

  /**
   * プレイヤオブジェクトの状態を初期化する。
   * @param sfen 初期局面のSFEN
   */
  void initialize(const std::string& sfen);

  /**
   * 次の手番を取得する。
   * @return 手番
   */
  int32_t getColor();

  /**
   * 駒を動かす。
   * @param move 動かす駒の情報
   */
  void play(const Move& move);

  /**
   * 盤面評価を開始する。
   * 探索処理は別スレッドで実行される。
   * 最大訪問回数に0以下の値を指定すると停止命令を指示するまで探索を続ける。
   * @param equally 探索回数を均等にするならばtrue、UCB1かPUCBを使用するならばfalse
   * @param useUcb1 探索先の基準としてUCB1を使用するならばtrue、PUCBを使用するならばfalse
   * @param candidateWidth 候補手の探索幅(0の場合は探索幅を自動で調整する)
   * @param checkNodeDepth 詰み手筋を探索するノードの最大深さ
   * @param temperature 探索の温度パラメータ
   * @param noise 探索のガンベルノイズの強さ
   */
  void startEvaluation(
      bool equally, bool useUcb1, int32_t candidateWidth, int32_t checkNodeDepth,
      float temperature, float noise);

  /**
   * 探索が終了するまで待機する。
   * @param visits 探索の訪問回数
   * @param playouts 探索のプレイアウト回数
   * @param timelimit 待機する時間（秒）
   * @param stop 探索を停止するならばtrue
   */
  void waitEvaluation(int32_t visits, int32_t playouts, float timelimit, bool stop);

  /**
   * 候補手の一覧を取得する。
   * @return 候補手の一覧
   */
  std::vector<Candidate> getCandidates();

  /**
   * 指定された盤面オブジェクトに盤面の状態をコピーする。
   * @param board 盤面オブジェクト
   */
  void copyBoardTo(Board* board);

 private:
  /**
   * 同期オブジェクト。
   */
  std::mutex _mutex;

  /**
   * 条件変数。
   */
  std::condition_variable _condition;

  /**
   * 探索ノードを管理するオブジェクト。
   */
  NodeManager _nodeManager;

  /**
   * スレッド管理オブジェクト。
   */
  ThreadPool _threadPool;

  /**
   * 探索を実行するスレッド。
   */
  std::unique_ptr<std::thread> _thread;

  /**
   * ルートノード。
   */
  Node* _root;

  /**
   * 葉ノードのみ評価するならばtrue。
   */
  bool _evalLeafOnly;

  /**
   * 探索の訪問回数。
   */
  int32_t _searchVisits;

  /**
   * 探索のプレイアウト回数。
   */
  int32_t _searchPlayouts;

  /**
   * 探索回数を均等にするならtrue。
   */
  bool _searchEqually;

  /**
   * 探索先の基準としてUCB1を使用するならtrue。
   */
  bool _searchUseUcb1;

  /**
   * 候補手の探索幅。
   */
  int32_t _searchCandidateWidth;

  /**
   * 詰み探索を行うノードの最大深さ。
   */
  int32_t _searchCheckNodeDepth;

  /**
   * 探索の温度パラメータ。
   */
  float _searchTemperature;

  /**
   * 探索のガンベルノイズの強さ。
   */
  float _searchNoise;

  /**
   * 実行中のスレッド数。
   */
  int32_t _runnings;

  /**
   * 探索を一時停止しているならtrue。
   */
  bool _paused;

  /**
   * 探索を停止しているならtrue。
   */
  bool _stopped;

  /**
   * 探索を終了しているならtrue。
   */
  bool _terminated;

  /**
   * 探索処理を起動する。
   */
  void _run();

  /**
   * 探索を実行する。
   * @return プレイアウト数
   */
  int32_t _evaluate();

  /**
   * ルートノード以外のノードオブジェクトを返却する。
   * @param node 返却するノードオブジェクト
   */
  void _releaseNode(Node* node);
};

}  // namespace deepshogi

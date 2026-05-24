#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include "Board.h"
#include "Candidate.h"
#include "InferenceProcessor.h"
#include "MctsManager.h"
#include "MctsNode.h"
#include "PnSearchManager.h"
#include "ThreadPool.h"

namespace deepshogi {

/**
 * プレイヤとして次の着手を選択するクラス。
 */
class Player {
 public:
  /**
   * プレイヤオブジェクトを作成する。
   * @param processor 推論を実行するオブジェクト
   * @param threads スレッドの数
   * @param searchMaxVisits ノードの最大訪問回数
   * @param nyugyokuScoreBlack 先手番の入玉宣言に必要となる点数
   * @param nyugyokuScoreWhite 後手番の入玉宣言に必要となる点数
   * @param drawTurn 引き分けとなるまでの手数
   * @param checkSearchDepth 詰み手筋の探索深さ
   * @param checkSearchNode 詰み手筋の探索ノード数
   * @param checkNodeDepth 詰み手筋を探索するノードの最大深さ
   * @param ucbConstant UCBの信頼上限に掛ける定数
   * @param pucbConstantInit PUCBの信頼上限に掛ける定数の初期値
   * @param pucbConstantBase PUCBの信頼上限に掛ける定数の変化値
   */
  Player(
      InferenceProcessor* processor, int32_t threads, int32_t searchMaxVisits,
      int32_t nyugyokuScoreBlack, int32_t nyugyokuScoreWhite, int32_t drawTurn,
      int32_t checkSearchDepth, int32_t checkSearchNode, int32_t checkNodeDepth,
      float ucbConstant, float pucbConstantInit, float pucbConstantBase);

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
   * 指定された着手にしたがって駒を動かす。
   * @param move 動かす駒の情報
   */
  void play(const Move& move);

  /**
   * 盤面評価を開始する。
   * 探索処理は別スレッドで実行される。
   * @param equally 探索回数を均等にするならばtrue、UCBかPUCBを使用するならばfalse
   * @param candidateWidth 候補手の探索幅(0の場合は探索幅を自動で調整する)
   * @param temperature 探索の温度パラメータ
   * @param noise 探索のガンベルノイズの強さ
   */
  void startEvaluation(
      bool equally, int32_t candidateWidth, float temperature, float noise);

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

  /**
   * プレイヤオブジェクトの状態を表す文字列を取得する。
   * @return プレイヤオブジェクトの状態を表す文字列
   */
  std::string toString();

  /**
   * プレイヤオブジェクトの状態を出力ストリームに書き込む。
   * @param os 出力ストリーム
   * @param player プレイヤオブジェクト
   * @return 出力ストリーム
   */
  friend std::ostream& operator<<(std::ostream& os, Player& player) {
    os << player.toString();
    return os;
  }

 private:
  /**
   * 同期オブジェクト。
   */
  std::mutex _mutex;

  /**
   * 探索を起動するための条件変数。
   */
  std::condition_variable _searchCondition;

  /**
   * ノードの更新処理を起動するための条件変数。
   */
  std::condition_variable _updateCondition;

  /**
   * 終了を待機するための条件変数。
   */
  std::condition_variable _stopCondition;

  /**
   * 推論を実行するオブジェクト。
   */
  InferenceProcessor* _processor;

  /**
   * 詰み探索エンジンを管理するオブジェクト。
   */
  PnSearchManager _pnsearch;

  /**
   * スレッド管理オブジェクト。
   */
  ThreadPool _threadPool;

  /**
   * 探索状態を管理するスレッド。
   */
  std::thread _searchThread;

  /**
   * ノードの状態を更新するスレッド。
   */
  std::thread _updateThread;

  /**
   * 探索ノードを管理するオブジェクト。
   */
  MctsManager _manager;

  /**
   * ルートノード。
   */
  MctsNode* _root;

  /**
   * ノードの最大訪問回数。
   */
  int32_t _searchMaxVisits;

  /**
   * 長手数の詰み手筋の深さ。
   */
  int32_t _checkSearchDepth;

  /**
   * 詰み探索を行うノードの最大深さ。
   */
  int32_t _checkNodeDepth;

  /**
   * 探索回数を均等にするならtrue。
   */
  bool _searchEqually;

  /**
   * 候補手の探索幅。
   */
  int32_t _searchCandidateWidth;

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
   * 評価中のノードオブジェクトの一覧。
   */
  std::queue<MctsNode*> _evaluatingNodes;

  /**
   * 詰み探索を行うノードオブジェクトの一覧。
   */
  std::queue<MctsNode*> _checkingNodes;

  /**
   * 探索を実行する。
   */
  void _runSearch();

  /**
   * 探索木を展開する。
   */
  void _runExpand();

  /**
   * 詰み探索を実行する。
   * @param node 詰み探索を行うノードオブジェクト
   */
  void _runCheckmateSearch(MctsNode* node);

  /**
   * ノードの状態を更新する。
   */
  void _runUpdate();
};

}  // namespace deepshogi

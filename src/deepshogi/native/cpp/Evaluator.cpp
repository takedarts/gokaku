#include "Evaluator.h"

#define DIR_H (0)    // 移動:打
#define DIR_U (1)    // 移動:上
#define DIR_D (2)    // 移動:下
#define DIR_R (3)    // 移動:右
#define DIR_L (4)    // 移動:左
#define DIR_UR (5)   // 移動:右上
#define DIR_UL (6)   // 移動:左上
#define DIR_DR (7)   // 移動:右下
#define DIR_DL (8)   // 移動:左下
#define DIR_KR (9)   // 移動:桂馬右
#define DIR_KL (10)  // 移動:桂馬左

namespace deepshogi {

/**
 * 指定された着手のPolicyインデックスを取得する。
 * @param board 盤面
 * @param move 着手
 * @return Policyインデックス
 */
static int32_t getPolicyIndex(const Board* board, Move move) {
  // 駒番号と移動方向を計算する
  int32_t move_x = 0;
  int32_t move_y = 0;
  int32_t piece = 0;

  if (move.getSrcX() >= BOARD_SIZE) {
    move_x = 0;
    move_y = 0;
    piece = move.getSrcY() - PIECE_HAND_BEGIN;
  } else {
    move_x = move.getDstX() - move.getSrcX();
    move_y = move.getDstY() - move.getSrcY();
    piece = board->getPiece(move.getSrcX(), move.getSrcY());

    if (move.isPromote()) {
      piece += PIECE_PROMOTE;
    }

    if (PIECE_BLACK_BEGIN <= piece && piece < PIECE_BLACK_END) {
      piece -= PIECE_BLACK_BEGIN;
    } else if (PIECE_WHITE_BEGIN <= piece && piece < PIECE_WHITE_END) {
      piece -= PIECE_WHITE_BEGIN;
    } else {
      std::cerr << "Invalid piece: " << piece << std::endl;
      throw std::invalid_argument("Invalid piece");
    }
  }

  // 後手番の場合は移動座標を反転する
  if (board->getColor() == COLOR_WHITE) {
    move_x = -move_x;
    move_y = -move_y;
  }

  // 移動方向を判定する
  int32_t dir = 0;

  if (move_x == 0 && move_y == 0) {  // 打
    dir = DIR_H;
  } else if (move_x == 0 && move_y < 0) {  // 上
    dir = DIR_U;
  } else if (move_x == 0 && move_y > 0) {  // 下
    dir = DIR_D;
  } else if (move_x < 0 && move_y == 0) {  // 右
    dir = DIR_R;
  } else if (move_x > 0 && move_y == 0) {  // 左
    dir = DIR_L;
  } else if (move_x < 0 && move_y == move_x) {  // 右上
    dir = DIR_UR;
  } else if (move_x > 0 && move_y == -move_x) {  // 左上
    dir = DIR_UL;
  } else if (move_x < 0 && move_y == -move_x) {  // 右下
    dir = DIR_DR;
  } else if (move_x > 0 && move_y == move_x) {  // 左下
    dir = DIR_DL;
  } else if (move_x == -1 && move_y == -2) {  // 桂馬右
    dir = DIR_KR;
  } else if (move_x == 1 && move_y == -2) {  // 桂馬左
    dir = DIR_KL;
  } else {
    std::cerr << "Invalid move direction: " << move_x << ", " << move_y << std::endl;
    throw std::invalid_argument("Invalid move direction");
  }

  // Policyインデックスを計算する
  int32_t index = 0;

  if (piece == 0) {  // 歩
    index =
        0 + ((dir == DIR_U)   ? 0
             : (dir == DIR_H) ? 1
                              : -1);
  } else if (piece == 1) {  // 香車
    index =
        2 + ((dir == DIR_U)   ? 0
             : (dir == DIR_H) ? 1
                              : -3);
  } else if (piece == 2) {  // 桂馬
    index =
        4 + ((dir == DIR_KR)   ? 0
             : (dir == DIR_KL) ? 1
             : (dir == DIR_H)  ? 2
                               : -5);
  } else if (piece == 3) {  // 銀
    index =
        7 + ((dir == DIR_U)    ? 0
             : (dir == DIR_UR) ? 1
             : (dir == DIR_UL) ? 2
             : (dir == DIR_DR) ? 3
             : (dir == DIR_DL) ? 4
             : (dir == DIR_H)  ? 5
                               : -8);
  } else if (piece == 4) {  // 角
    index =
        13 + ((dir == DIR_UR)   ? 0
              : (dir == DIR_UL) ? 1
              : (dir == DIR_DR) ? 2
              : (dir == DIR_DL) ? 3
              : (dir == DIR_H)  ? 4
                                : -14);
  } else if (piece == 5) {  // 飛車
    index =
        18 + ((dir == DIR_U)   ? 0
              : (dir == DIR_D) ? 1
              : (dir == DIR_R) ? 2
              : (dir == DIR_L) ? 3
              : (dir == DIR_H) ? 4
                               : -19);
  } else if (piece == 6) {  // 金
    index =
        23 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_H)  ? 6
                                : -24);
  } else if (piece == 7) {  // 玉
    index =
        30 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -31);
  } else if (piece == 8) {  // と金
    index =
        38 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
                                : -39);
  } else if (piece == 9) {  // 成香
    index =
        44 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
                                : -45);
  } else if (piece == 10) {  // 成桂
    index =
        50 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_KR) ? 6
              : (dir == DIR_KL) ? 7
                                : -51);
  } else if (piece == 11) {  // 成銀
    index =
        58 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -59);
  } else if (piece == 12) {  // 馬
    index =
        66 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -67);
  } else if (piece == 13) {  // 龍
    index =
        74 + ((dir == DIR_U)    ? 0
              : (dir == DIR_D)  ? 1
              : (dir == DIR_R)  ? 2
              : (dir == DIR_L)  ? 3
              : (dir == DIR_UR) ? 4
              : (dir == DIR_UL) ? 5
              : (dir == DIR_DR) ? 6
              : (dir == DIR_DL) ? 7
                                : -75);
  } else {
    std::cerr << "Invalid piece: " << piece << std::endl;
    throw std::invalid_argument("Invalid piece");
  }

  if (index < 0) {
    std::cerr << "Invalid index: " << index << std::endl;
    throw std::invalid_argument("Invalid index");
  }

  // 番号を返す
  return index;
}

/**
 * 評価結果オブジェクトを作成する。
 * @param processor 推論を実行するオブジェクト
 */
Evaluator::Evaluator(Processor* processor)
    : _processor(processor),
      _policies(),
      _value(0.0),
      _evaluated(false) {
}

/**
 * モデルによる評価結果をクリアする。
 */
void Evaluator::clear() {
  _policies.clear();
  _value = 0.0;
  _evaluated = false;
}

/**
 * モデルによる評価を実行する。
 * @param board 評価対象の盤面
 */
void Evaluator::evaluate(Board* board) {
  // 評価済みなら何もしない。
  if (_evaluated) {
    return;
  }

  // 現在の盤面の評価を実行する。
  float inputs[MODEL_INPUT_SIZE];
  float outputs[MODEL_OUTPUT_SIZE];

  board->getInputs(inputs);
  _processor->execute(inputs, outputs, 1);

  // 候補手の一覧を作成する
  std::vector<Move> legal_moves = board->getLegalMoves();

  for (Move move : legal_moves) {
    // Policyのインデックスを計算する
    int32_t idx = getPolicyIndex(board, move);

    // 移動先の座標を計算する
    int32_t x = move.getDstX();
    int32_t y = move.getDstY();

    if (board->getColor() == COLOR_WHITE) {
      x = BOARD_SIZE - 1 - x;
      y = BOARD_SIZE - 1 - y;
    }

    // Policyを追加する
    int32_t index = ((idx * BOARD_SIZE * BOARD_SIZE) + (x * BOARD_SIZE + y));

    _policies.emplace_back(move, outputs[index], 0);
  }

  // 予測勝率を取得する。
  _value = outputs[MODEL_PREDICTIONS * BOARD_SIZE * BOARD_SIZE + 0] * 2 - 1;

  // 白番の場合は先後番の評価値を反転する。
  if (board->getColor() == COLOR_WHITE) {
    _value = -_value;
  }

  // 評価済みのフラグを立てる。
  _evaluated = true;
}

/**
 * モデルによる評価結果が設定されていればtrueを返す。
 * @return モデルによる評価結果が設定されていればtrue
 */
bool Evaluator::isEvaluated() {
  return _evaluated;
}

/**
 * モデルによる推論結果の予測候補手の一覧を取得する。
 * @return 予測候補手の一覧
 */
std::vector<Policy> Evaluator::getPolicies() {
  return _policies;
}

/**
 * モデルによる推論結果の予想勝率を取得する。
 * @return モデルによる推論結果の予想勝率
 */
float Evaluator::getValue() {
  return _value;
}

}  // namespace deepshogi

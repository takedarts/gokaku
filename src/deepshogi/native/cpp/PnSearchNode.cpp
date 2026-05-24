#include "PnSearchNode.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

#include "PnSearchEngine.h"

namespace deepshogi {

// PN探索ノードに設定する値の最大値
static constexpr int32_t MAX_VALUE = 0xffffff;

/**
 * PN探索ノードのオブジェクトを生成する。
 * 不詰みの末端ノードを表すノードとして初期化する。
 */
PnSearchNode::PnSearchNode()
    : _board(),
      _depth(MAX_VALUE),
      _children(),
      _pn(MAX_VALUE),
      _dn(0),
      _step(MAX_VALUE),
      _size(1) {
}

/**
 * 指定された盤面情報でこのノードを末端ノードとして初期化する。
 * @param board 盤面オブジェクト
 * @param depth ノードの深さ
 */
void PnSearchNode::initialize(const Board* board, int32_t depth) {
  // 盤面をコピーして不詰みの末端ノードとしての値を設定する
  _board.copyFrom(board);
  _depth = depth;
  _children.clear();
  _pn = MAX_VALUE;
  _dn = 0;
  _step = MAX_VALUE;
  _size = 1;

  // 最大手数であれば不詰みとする
  if (_board.getTurn() >= _board.getDrawTurn()) {
    return;
  }

  // PN/DN値を設定する
  if (_depth % 2 == 0) {
    // 王手をかける手番の場合
    // 王手をかけられる場合：PN=1, DN=合法手の数, 詰みまでの手数=最大値
    // 王手をかけられない場合：PN=最大値, DN=0, 詰みまでの手数=最大値
    std::vector<Move> legal_moves = _board.getLegalMoves(false, true);

    if (!legal_moves.empty()) {
      _pn = 1;
      _dn = (int32_t)legal_moves.size();
      _step = MAX_VALUE;
    } else {
      _pn = MAX_VALUE;
      _dn = 0;
      _step = MAX_VALUE;
    }
  } else {
    // 王手から逃げる手番の場合
    // 王手から逃げられる場合：PN=合法手の数, DN=1, 詰みまでの手数=最大値
    // 王手から逃げられない場合：PN=0, DN=最大値, 詰みまでの手数=1
    // 同じ座標へ持ち駒を打つ手は駒の種類が異なっていても1つの手としてカウントする
    std::vector<Move> legal_moves = _board.getLegalMoves(false, false);

    if (!legal_moves.empty()) {
      std::set<Position> unique_hand_moves;
      int32_t board_move_count = 0;

      for (Move& move : legal_moves) {
        if (move.getSrc().getX() == BOARD_SIZE) {
          unique_hand_moves.insert(move.getDst());
        } else {
          board_move_count++;
        }
      }

      _pn = board_move_count + (int32_t)unique_hand_moves.size();
      _dn = 1;
      _step = MAX_VALUE;
    } else {
      _pn = 0;
      _dn = MAX_VALUE;
      _step = 1;
    }
  }
}

/**
 * ノードを展開して子ノードを生成する。
 * @param engine DPFNエンジンのオブジェクト
 * @return ノードを展開できた場合はtrue
 */
bool PnSearchNode::expand(PnSearchEngine* engine) {
  // 子ノードをクリアする
  _children.clear();

  // 合法手の生成ルールを判別する
  // depthが偶数の場合：王手をかける手番（王手がかかる手のみを生成する）
  // depthが奇数の場合：王手から逃げる手番（王手以外の手も生成する）
  bool checkmate = (_depth % 2 == 0) ? true : false;

  // 子ノードを登録する
  Board board;

  for (Move& move : _board.getLegalMoves(false, checkmate)) {
    // 盤面を作成する
    board.copyFrom(&_board);
    board.play(move);

    // ノードオブジェクトを取得する
    // 同じ盤面のノードがキャッシュに存在する場合はそのノードを返す
    // 最大ノード数に達している場合はnullptrを返す
    PnSearchNode* child_node = engine->_getNode(&board, _depth + 1);

    if (child_node == nullptr) {
      return false;
    }

    // 子ノードの深さを設定する
    // ノードキャッシュに同じ盤面のノードが存在する場合、
    // そのノードの深さが`depth + 1`より大きい場合は深さを`depth + 1`に修正する
    if (child_node->_depth > _depth + 1) {
      child_node->_depth = _depth + 1;
    }

    // 子ノードリストに追加する
    _children.push_back(std::make_pair(move, child_node));
  }

  return true;
}

/**
 * このノードのPN/DN値を更新する。
 * @param depth_limit 深さの制限
 */
void PnSearchNode::update(int32_t depth_limit) {
  // 深さ制限に達した場合は不詰みとする
  if (_depth >= depth_limit) {
    _pn = MAX_VALUE;
    _dn = 0;
    _step = MAX_VALUE;
    _size = 1;
    return;
  }

  // 王手をかける手番の場合
  // PN値は子ノードのPN値の最小値、DN値は子ノードのDN値の合計、
  // 詰みまでの手数は子ノードの詰みまでの手数の最小値+1とする
  if (_depth % 2 == 0) {
    _pn = MAX_VALUE;
    _dn = 0;
    _step = MAX_VALUE;
    _size = 1;

    for (auto& child_pair : _children) {
      PnSearchNode* child = child_pair.second;

      // PN値は子ノードのPN値の最小値とする
      if (child->_pn < _pn) {
        _pn = child->_pn;
      }

      // DN値は子ノードのDN値の合計とする
      _dn = std::min(_dn + child->_dn, MAX_VALUE);

      // 詰みまでの手数は子ノードの詰みまでの手数の最小値+1とする
      if (_step > child->_step + 1) {
        _step = child->_step + 1;
      }

      // ノードのサイズは子ノードのサイズの合計とする
      _size = std::min(_size + child->_size, MAX_VALUE);
    }
  }
  // 王手から逃げる手番の場合
  // PN値は子ノードのPN値の合計、DN値は子ノードのDN値の最小値、
  // 詰みまでの手数は子ノードの詰みまでの手数の最大値+1とする。
  // 子ノードのPN値の合計を計算する際、同じ座標へ持ち駒を打つ手が複数ある場合は、
  // それらの子ノードのPN値のうち最大のものを合計に加算する。
  else {
    std::map<Position, int32_t> hand_move_pns;
    _pn = 0;
    _dn = MAX_VALUE;
    _step = 1;
    _size = 1;

    for (auto& [move, child] : _children) {
      // 同じ座標へ持ち駒を打つ手が複数ある場合は、PN値のうち最大のものを合計に加算する
      if (move.getSrc().getX() == BOARD_SIZE) {
        Position dst = move.getDst();

        if (hand_move_pns.find(dst) == hand_move_pns.end()) {
          _pn = std::min(_pn + child->_pn, MAX_VALUE);
          hand_move_pns[dst] = child->_pn;
        } else if (child->_pn > hand_move_pns[dst]) {
          _pn = std::min(_pn + child->_pn - hand_move_pns[dst], MAX_VALUE);
          hand_move_pns[dst] = child->_pn;
        }
      }
      // 盤上の駒を動かす手はそのままPN値を合計に加算する
      else {
        _pn = std::min(_pn + child->_pn, MAX_VALUE);
      }

      // DN値は子ノードのDN値の最小値とする
      if (child->_dn < _dn) {
        _dn = child->_dn;
      }

      // 詰みまでの手数は子ノードの詰みまでの手数の最大値+1とする
      if (_step < child->_step + 1) {
        _step = child->_step + 1;
      }

      // ノードのサイズは子ノードのサイズの合計とする
      _size = std::min(_size + child->_size, MAX_VALUE);
    }
  }
}

/**
 * 次に探索する子ノードを取得する。
 * このノードが末端ノードの場合はnullptrを返す。
 * 王手をかける手番の場合は「PN値+探索数の対数」が最小の子ノードを返し、
 * 王手から逃げる手番の場合は「DN値+探索数の対数」が最小の子ノードを返す。
 * 探索数を考慮した優先度を計算することで、探索の偏りを減らすことができ、
 * 手数の少ない詰み筋を発見できる可能性が高くなる。
 * @return 次に探索する子ノードのポインタ
 */
PnSearchNode* PnSearchNode::getNextNode() {
  PnSearchNode* next_node = nullptr;

  if (_depth % 2 == 0) {
    // 詰み/不詰みが確定していない子ノードの中からPN値が最小のノードを探す
    // 探索数を考慮するためPN値に探索数の対数を加算した値を優先度とする
    float max_priority = 0.0f;

    for (auto& [move, child] : _children) {
      if (child->_pn == 0 || child->_dn == 0) {
        continue;
      }

      float priority = 1.0f / (child->_pn + std::log((float)child->_size));

      if (priority > max_priority) {
        max_priority = priority;
        next_node = child;
      }
    }
  } else {
    // 詰み/不詰みが確定していない子ノードの中からDN値が最小のノードを探す
    // 探索数を考慮するためDN値に探索数の対数を加算した値を優先度とする
    float max_priority = 0.0f;

    for (auto& [move, child] : _children) {
      if (child->_pn == 0 || child->_dn == 0) {
        continue;
      }

      float priority = 1.0f / (child->_dn + std::log((float)child->_size));

      if (priority > max_priority) {
        max_priority = priority;
        next_node = child;
      }
    }
  }

  return next_node;
}

/**
 * 詰み手順の着手と子ノードを取得する。
 * 詰み手順となる子ノードが存在しない場合はnullptrを返す。
 * @return 詰み手順の着手と子ノードのペア
 */
std::pair<Move, PnSearchNode*> PnSearchNode::getCheckmateNode() {
  PnSearchNode* checkmate_node = nullptr;
  Move checkmate_move(MOVE_INVALID);

  if (_depth % 2 == 0) {
    // 王手をかける手番の場合はPN値が0で最小手数の子ノードを探す
    int32_t min_step = MAX_VALUE;

    for (auto& child_pair : _children) {
      PnSearchNode* child = child_pair.second;

      if (child->_pn == 0 && child->_step < min_step) {
        checkmate_node = child;
        checkmate_move = child_pair.first;
        min_step = child->_step;
      }
    }
  } else {
    // 王手から逃げる手番の場合はPN値が0で最大手数の子ノードを探す
    int32_t max_step = 0;

    for (auto& child_pair : _children) {
      PnSearchNode* child = child_pair.second;

      if (child->_pn == 0 && child->_step > max_step) {
        checkmate_node = child;
        checkmate_move = child_pair.first;
        max_step = child->_step;
      }
    }
  }

  return std::make_pair(checkmate_move, checkmate_node);
}

/**
 * 指定された子ノードを新しい子ノードに置き換える。
 * @param targetNode 置き換える子ノード
 * @param newNode 新しい子ノード
 */
void PnSearchNode::replaceChildNode(PnSearchNode* targetNode, PnSearchNode* newNode) {
  for (auto& [move, child] : _children) {
    if (child == targetNode) {
      child = newNode;
      return;
    }
  }
}

/**
 * ノードの情報を文字列として取得する。
 * @return ノードの情報の文字列
 */
std::string PnSearchNode::toString() const {
  std::stringstream ss;

  ss << "DFPN Node: depth=" << _depth << " pn=" << _pn << " dn=" << _dn
     << " step=" << _step << " size=" << _size << "\n"
     << _board << "\n";

  for (auto& child_pair : _children) {
    Move move = child_pair.first;
    PnSearchNode* child = child_pair.second;

    ss << "  Child Move: " << move
       << " pn=" << child->_pn
       << " dn=" << child->_dn
       << " step=" << child->_step
       << " size=" << child->_size
       << "\n";
  }

  return ss.str();
}

}  // namespace deepshogi

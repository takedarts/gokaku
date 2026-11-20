#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "BoardHash.h"
#include "PnSearchCache.h"
#include "PnSearchNode.h"
// Refer to search settings and node counts for comparison only, without changing the Python API.
#define private public
#include "PnSearchEngine.h"
#undef private

using namespace deepshogi;

/**
 * Measure search effort, time, and move legality for the first positions in an SFEN file.
 * @param argc Argument count.
 * @param argv Arguments (file, group correction 0/1, count, capacity, depth).
 * @return Exit code (0 on success, 1 for an illegal sequence, 2 for invalid arguments).
 */
int main(int argc, char** argv) {
  if (argc != 6) {
    return 2;
  }
  std::ifstream source(argv[1]);
  int variant = std::stoi(argv[2]);
  int count = std::stoi(argv[3]);
  int capacity = std::stoi(argv[4]);
  int depth = std::stoi(argv[5]);
  if (!source || variant < 0 || variant > 1 || count <= 0 || capacity < 0) {
    return 2;
  }
  PnSearchEngine engine(capacity);
#ifndef DFPN_BASELINE
  engine._useGroups = variant % 2 == 1;
#endif
  std::string sfen;
  // Exclude input reading and legality checks from the search time.
  for (int i = 0; i < count; i++) {
    if (!std::getline(source, sfen) || sfen.empty()) {
      return 2;
    }
    Board board;
    board.initialize(sfen, 0x7fff);
    std::string before = board.toString();
    auto start = std::chrono::steady_clock::now();
    auto moves = engine.getCheckmateMoves(&board, depth);
    double seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    bool valid = board.toString() == before;
    // Check every move without omitting unpromotions and the mate in the final position.
    for (size_t step = 0; step < moves.size(); step++) {
      auto legal = board.getLegalMoves(false, step % 2 == 0);
      if (std::find(legal.begin(), legal.end(), moves[step]) == legal.end()) {
        valid = false;
        break;
      }
      board.play(moves[step]);
    }
    if (!moves.empty() && !board.getLegalMoves(false, false).empty()) {
      valid = false;
    }
    std::cout << i + 1 << '\t' << !moves.empty() << '\t' << valid << '\t'
              << engine._nodeCount << '\t' << engine._cloneCount << '\t'
              << seconds << '\t' << moves.size() << std::endl;
    if (!valid) {
      return 1;
    }
  }
  return source.bad() ? 2 : 0;
}

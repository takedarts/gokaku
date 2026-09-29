# Engine port regression tests

Build the native extension before running the Python tests:

```sh
python src/build.py
PYTHONPATH=src python -m unittest discover -s src/tests -p '*_test.py' -v
MYPYPATH=src python -m mypy --explicit-package-bases src
```

The Python tests need NumPy and CPU PyTorch, but do not require a trained model or CUDA.
`inference_test.py` traces a small probe model in a temporary directory to check the model layout.
The threaded player lifecycle runs in a subprocess with a timeout to expose hangs.

Compile and run the standalone cache tests from the repository root:

```sh
c++ -std=c++20 -O2 -Isrc/deepshogi/native/cpp \
  src/tests/native_cache_test.cpp \
  src/deepshogi/native/cpp/{Board,BoardHash,BitBoard,Constant,Position,Move,MoveResult,InferenceCache,InferenceHash,PnSearchCache}.cpp \
  -o /tmp/gokaku_native_cache_test
/tmp/gokaku_native_cache_test
```

Use an equivalent C++20 compiler command on platforms without brace expansion.
This test covers LRU eviction, disabled caching, rule-dependent inference keys,
PN cache hash collisions, and generation invalidation.

`player_extends_test.py` is ported from DeepShogi with comments translated into English.
`pnsearch_test.py` adapts DeepShogi's PN regressions without importing its training or record modules.
`pnsearch_test.json` contains positions extracted from upstream records 001 through 004,
with the suffix indicating how many recorded moves remained before the end of each game.
The depth-extension position is also taken from upstream's `test_depth_extension`.

CUDA, FP16 on CUDA, TensorRT, and trained-model playing strength require separate environment checks.

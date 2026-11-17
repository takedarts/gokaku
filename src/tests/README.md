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

`usi_test.py` ports upstream's time allocation and asynchronous command regressions.
`player_stop_test.py` ports its Python and native stop/restart regressions, using a small
CPU model instead of the training model factory. It also checks changing draw limits.
`board_test.py` ports native board reinitialization checks without record dependencies.

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
`player_random_test.py` covers low-temperature policy normalization, zero policies,
and candidate filtering without requiring a model.
`player_stop_test.py` also checks failed moves and initialization, referee history,
and agreement between native and Python repetition penalties.
`settings_test.py` covers CLI, USI, Python, Cython, and C++ setting validation,
including overflow, invalid devices, and native processor lifetime under garbage collection.
`usi_test.py` also covers settings fixed per request, deferred player recreation,
standard ponder aliases, predicted replies, and periodic search statistics.
`psfen_test.py` checks multi-digit hand counts without external record fixtures.
`pnsearch_test.py` adapts DeepShogi's PN regressions without importing its training or record modules.
`pnsearch_test.json` contains positions extracted from upstream records 001 through 004,
with the suffix indicating how many recorded moves remained before the end of each game.
The depth-extension position is also taken from upstream's `test_depth_extension`.

`player_wait_native_test.cpp` ports upstream's native wait, shutdown, exception cleanup,
mate-limit, repetition, and PUCB regressions. After building the extension, compile it
against the same native objects and Torch configuration. On macOS or Linux with the
Makefile build, run the following from the repository root:

```sh
PYTHONPATH=src:src/tests python -B - <<'PY'
import shlex
import subprocess
import tempfile
from pathlib import Path

import torch
from deepshogi.config import MODEL_INPUT_SIZE
from settings_test import SettingsModel

root = Path.cwd()
native = root / 'src/deepshogi/native'
target = native / 'build/CMakeFiles/modules.dir'
flags = (target / 'flags.make').read_text().splitlines()
settings = {
    line.split(' = ', 1)[0]: shlex.split(line.split(' = ', 1)[1])
    for line in flags if ' = ' in line
}
link = shlex.split((target / 'link.txt').read_text())
libraries = [
    arg for arg in link
    if (arg.endswith(('.dylib', '.so', '.a', '.lib')) and arg != 'modules.so')
    or arg.startswith('-Wl,-rpath,')
]
with tempfile.TemporaryDirectory() as directory:
    executable = Path(directory) / 'player_wait_native_test'
    command = [
        link[0], *settings['CXX_FLAGS'], *settings['CXX_DEFINES'],
        *settings['CXX_INCLUDES'], '-I' + str(native / 'cpp'),
        str(root / 'src/tests/player_wait_native_test.cpp'),
        *map(str, sorted((target / 'cpp').glob('*.o'))),
        *libraries, '-o', str(executable),
    ]
    subprocess.run(command, check=True, timeout=180)
    model = Path(directory) / 'probe.pt'
    inputs = torch.zeros((1, MODEL_INPUT_SIZE))
    torch.jit.trace(SettingsModel().eval(), inputs).save(str(model))
    subprocess.run([str(executable), str(model)], check=True, timeout=90)
PY
```

CUDA, FP16 on CUDA, TensorRT, and trained-model playing strength require separate environment checks.

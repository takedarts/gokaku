# Gokaku
[English](./README.md) | [Japanese](./README_JP.md)

## Overview
Gokaku is a computer Shogi program developed using deep reinforcement learning from randomly generated game records. The deep learning model of Gokaku incorporates nested-bottleneck convolutions and multi-head attention, enabling it to efficiently grasp the overall state of the board. Its reinforcement learning procedure is inspired by approaches used in Katago and Gumbel AlphaZero, allowing it to efficiently learn a wide range of patterns.

Gokaku is a sibling program of the computer Go program [Maru](https://github.com/takedarts/maru). Gokaku shares the same deep learning model architecture, search algorithm, and reinforcement learning methodology as Maru.

You can check the improvement of Gokaku's playing strength through reinforcement learning on [this page](https://takeda-lab.jp/gokaku/).
The model files are available for download from [this release page](https://github.com/takedarts/gokaku/releases/tag/v2.1).

Gokaku version 2.1 will be the final release. From now on, only model file additions and bug fixes are planned.

## How to Run
Gokaku can be run using one of the following methods:

- [Running from Source Files](#running-from-source-files)
- [Running with Docker](#running-with-docker)

For Windows 11 (x64, CUDA 12) environments, you can also use the [executable file for Windows 11](https://drive.usercontent.google.com/download?id=1q0G8kxmmKC44Os8y8cFE0gY4fIGpkBXK).

## Running from Source Files
### Build Instructions
Since most parts of this program are written in Cython and C++, the program code must be compiled before it can be run.

First, install the required modules for compilation:
```
pip install numpy cython cmake
```

PyTorch is required for compilation, so please install the appropriate version of this module according to your execution environment, such as CUDA.
```
pip install torch
```

Next, run `src/build.py` to compile the Cython and C++ code.
On Linux or macOS environments, `make` is required.
On Windows environments, `MSBuild` is required (MSBuild is included with Visual Studio).
```
python src/build.py
```

If compilation is successful, the compiled Cython module will be generated in `src/deepgo/native`.

You can delete the generated files by running the following command:
```
python src/build.py --clean
```

### Running the Program
You can launch Gokaku by running the launch script `src/run.py`.
At runtime, you need to specify the model file as a command-line argument (You can download the model file from [here](https://github.com/takedarts/gokaku/releases/tag/v2.1)):
```
python src/run.py <model_file>
```


Gokaku operates via the USI (Universal Shogi Interface) protocol.
Here is a simple example of usage:
```
% python src/run.py b4c256-250.model
usi
id name Gokaku 2.1
id author Atsushi Takeda
option name Threads type spin default 16 min 1
option name CheckSearchDepth type spin default 31 min 1
option name CheckSearchNode type spin default 10000 min 1
option name CheckNodeDepth type spin default 4 min 1
option name NyugyokuRule type combo default 27 var 27 var 24
option name Visits type spin default 50 min 1
option name Playouts type spin default 0 min 0
option name Timelimit type spin default 120000 min 0
option name UseUCB1 type check default false
option name Ponder type check default false
option name ResignThreshold type spin default 2 min 0 max 100
option name ResignTurn type spin default 50 min 0
option name InitialTurn type spin default 4 min 0
usiok
isready
readyok
usinewgame
go
info multipv 1 nodes 1 score cp -23 pv 3i3h
bestmove 3i3h
```

Since Gokaku supports the USI protocol, you can also use GUI applications that support USI protocol, such as [ShogiHome](https://sunfish-shogi.github.io/shogihome/).

To see the available options, run the script with the --help flag:
```
python src/run.py --help
```

## Running with Docker
A Docker image is available for running Gokaku easily.
If you have CUDA installed on your system, you can retrieve and run the Gokaku docker image using the following commands  (You can download the model file from [here](https://github.com/takedarts/gokaku/releases/tag/v2.1)):
```
docker pull takedarts/gokaku:v2.1-cuda12.6
docker run -it --rm --gpus all -v .:/workspace takedarts/gokaku:v2.1-cuda12.6 /opt/run.sh <model_file>
```
Use the `--gpus` option to specify the GPUs to use, and mount the current directory to the container's `/workspace` using `-v .:/workspace`.
Place the model file in the current directory and specify its path as `<model_file>`.

To see the available options, use the `--help` flag:
```
docker run -it --rm --gpus all -v .:/workspace takedarts/gokaku:v2.1-cuda12.6 /opt/run.sh --help
```

For environments without GPU (CPU only), use the following commands:
```
docker pull takedarts/gokaku:v2.1-cpu
docker run -it --rm -v .:/workspace takedarts/gokaku:v2.1-cpu /opt/run.sh <model_file>
```

## Execution Options
When running the startup script `src/run.py`, you can specify the following options:

| Option                     | Description                                                    | Default Value       |
|----------------------------|----------------------------------------------------------------|---------------------|
| `--help`                   | Display a list of available options                            |                     |
| `--visits <N>`             | Number of searches (number of nodes in the search tree)        | 50                  |
| `--playouts <N>`           | Number of playouts (number of leaves in the search tree)       | 0                   |
| `--timelimit <N>`          | Maximum thinking time (in seconds)                             | 120                 |
| `--search-method <S>`      | Criterion for selecting search nodes (`ucb1` or `pucb`)        | `pucb`              |
| `--ponder`                 | Enable pondering                                               | False               |
| `--resign <R>`             | Predicted win rate threshold for resignation                   | 0.02                |
| `--min-turn <N>`           | Minimum number of turns before resignation is allowed          | 100                 |
| `--initial-turn <N>`       | Number of opening turns with random moves                      | 4                   |
| `--initial-width <N>`      | Number of candidates at opening random moves                   | 16                  |
| `--nyugyoku-rule <N>`      | Points for the entering-king rule (27 or 24)                   | 27                  |
| `--check-search-depth <N>` | Depth of checkmate search nodes                                | 31                  |
| `--check-search-node <N>`  | Number of checkmate search nodes                               | 10,000              |
| `--check-node-depth <N>`   | Depth of search nodes at which checkmate search is performed   | 4                   |
| `--client-name <S>`        | Client name to display                                         | `Gokaku`              |
| `--client-version <S>`     | Version information to display                                 | `2.1`               |
| `--client-author <S>`      | Author name to be displayed                                    | `Atsushi Takeda`    |
| `--threads <N>`            | Number of threads to use for search                            | 16                  |
| `--batch-size <N>`         | Maximum batch size for board evaluation                        | 2048                |
| `--gpu <N>`                | GPU ID(s) to use (comma-separated for multiple GPUs)           |                     |
| `--fp16`                   | Use half-precision floating point (FP16)                       | False               |
| `--verbose`                | Enable log output to standard error                            | False               |


#### Relationship between Number of Visits, Number of Playouts, and Thinking Time
The termination condition of the search is determined by the values specified with the `--visits`, `--playouts`, and `--timelimit` options. The search ends either when both the number of visits and the number of playouts exceed their specified values, or when the elapsed thinking time exceeds the specified number of seconds.

## Execution Examples
To start Gokaku using the model file `b4c256-250.model`, run the following command:
```
python src/run.py b4c256-250.model
```

To start Gokaku with the number of visits set to 1000 and the maximum thinking time set to 5 seconds, run the following command:
```
python src/run.py b4c256-250.model --visits 1000 --timelimit 5
```

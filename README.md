# Gokaku
[English](./README.md) | [Japanese](./README_JP.md)

## Overview
Gokaku is a computer Shogi program developed using deep reinforcement learning from randomly generated game records. The deep learning model of Gokaku incorporates nested-bottleneck convolutions and multi-head attention, enabling it to efficiently grasp the overall state of the board. Its reinforcement learning procedure is inspired by approaches used in Katago and Gumbel AlphaZero, allowing it to efficiently learn a wide range of patterns.

Gokaku is a sibling program of the computer Go program [Maru](https://github.com/takedarts/maru). Gokaku shares the same deep learning model architecture, search algorithm, and reinforcement learning methodology as Maru.

You can check the improvement of Gokaku's playing strength through reinforcement learning on [this page](https://takeda-lab.jp/gokaku/).
The model files are available for download from [this release page](https://github.com/takedarts/gokaku/releases/tag/v2.2).

## How to Run
Gokaku can be run using one of the following methods:

- [Running from Source Files](#running-from-source-files)
- [Running with Docker](#running-with-docker)

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
At runtime, you need to specify the model file as a command-line argument (You can download the model file from [here](https://github.com/takedarts/gokaku/releases/tag/v2.2)):
```
python src/run.py <model_file>
```

Gokaku operates via the USI (Universal Shogi Interface) protocol.
Here is a simple example of usage:
```
% python src/run.py b4c256-700.model
usi
id name Gokaku 2.2
id author Atsushi Takeda
option name Threads type spin default 16 min 1
option name CheckSearchDepth type spin default 21 min 1
option name CheckSearchNode type spin default 20000 min 1
option name CheckNodeDepth type spin default 2 min 0
option name UcbConstant type spin default 140 min 0
option name PucbConstantInit type spin default 120 min 0
option name PucbConstantBase type spin default 18200 min 0
option name NyugyokuRule type combo default 27 var 27 var 24
option name DrawTurn type spin default 512 min 1
option name Visits type spin default 50 min 1
option name Playouts type spin default 0 min 0
option name Timelimit type spin default 120000 min 0
option name Ponder type check default false
option name Algorithm type combo default pucb var ucb var pucb
option name Criterion type combo default value var value var minimax var visits
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
You can pull and run the Gokaku Docker image by executing the following command in an environment where CUDA is available.  (You can download the model file from [here](https://github.com/takedarts/gokaku/releases/tag/v2.2)):
```
docker run -i --rm --gpus all -v .:/workspace -q takedarts/gokaku:v2.2-cuda12.6 /opt/run.sh <model_file>
```
Use the `--gpus` option to specify the GPUs to use, and mount the current directory to the container's `/workspace` using `-v .:/workspace`.
Place the model file in the current directory and specify its path as `<model_file>`.

The Docker image is about 4 GB in size because it includes the CUDA and PyTorch runtime environments.
When running it for the first time, an internet connection is required to download the image, and the download may take some time.

If you want to download the Docker image before running it, execute the following command.
```
docker pull takedarts/gokaku:v2.2-cuda12.6
```

You can also specify options after the execution command.
If you add `--help` to the execution command, a list of available options will be displayed.
```
docker run -i --rm --gpus all -v .:/workspace -q takedarts/gokaku:v2.2-cuda12.6 /opt/run.sh --help
```

A Docker image intended for CPU execution is also available (its image size is smaller than that of the CUDA version).
If you want to run computations on the CPU, execute the following command:
```
docker run -i --rm -v .:/workspace -q takedarts/gokaku:v2.2-cpu /opt/run.sh <model_file>
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
| `--client-version <S>`     | Version information to display                                 | `2.2`               |
| `--client-author <S>`      | Author name to be displayed                                    | `Atsushi Takeda`    |
| `--threads <N>`            | Number of threads to use for search                            | 16                  |
| `--batch-size <N>`         | Maximum batch size for board evaluation                        | 2048                |
| `--gpus <N>`               | GPU ID(s) to use (comma-separated for multiple GPUs)           |                     |
| `--fp16`                   | Use half-precision floating point (FP16)                       | False               |
| `--verbose`                | Enable log output to standard error                            | False               |


#### Relationship between Number of Visits, Number of Playouts, and Thinking Time
The termination condition of the search is determined by the values specified with the `--visits`, `--playouts`, and `--timelimit` options. The search ends either when both the number of visits and the number of playouts exceed their specified values, or when the elapsed thinking time exceeds the specified number of seconds.

## Execution Examples
To start Gokaku using the model file `b4c256-700.model`, run the following command:
```
python src/run.py b4c256-700.model
```

To start Gokaku with the number of visits set to 1000 and the maximum thinking time set to 5 seconds, run the following command:
```
python src/run.py b4c256-700.model --visits 1000 --timelimit 5
```

## Compatibility with Gokaku Version 2.1
Because the input signals differ between Gokaku version 2.2 and model files for version 2.1 or earlier, model files created for version 2.1 or earlier will not work correctly with version 2.2.
If you want to use model files for version 2.1 or earlier, please use Gokaku version 2.1 or an earlier program.

## License
Starting with Gokaku version 2.2, the license has been changed to the MIT License.
- Gokaku version 2.1 and earlier: GPL-3.0 License
- Gokaku version 2.2 and later: MIT License

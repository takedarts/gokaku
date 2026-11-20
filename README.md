# Gokaku
[English](./README.md) | [Japanese](./README_JP.md)

## Overview
Gokaku is a computer Shogi program developed using deep reinforcement learning from randomly generated game records. The deep learning model of Gokaku incorporates nested-bottleneck convolutions and multi-head attention, enabling it to efficiently grasp the overall state of the board. Its reinforcement learning procedure is inspired by approaches used in Katago and Gumbel AlphaZero, allowing it to efficiently learn a wide range of patterns.

Gokaku is a sibling program of the computer Go program [Maru](https://github.com/takedarts/maru). Gokaku shares the same deep learning model architecture, search algorithm, and reinforcement learning methodology as Maru.
You can track Gokaku's playing strength improvements on [this page](https://takeda-lab.jp/gokaku/).

A model file is required to run Gokaku.
Gokaku runs an ExportedProgram model (`.ep`) or a PT2 package (`.pt2`) compiled from an ExportedProgram with `src/compile.py`.
Model files are published on the [releases page](https://github.com/takedarts/gokaku/releases).

**Note:** Gokaku version 2.4 changes the model specification, including its output format,
so models from Gokaku version 2.3 or earlier cannot be used.

## How to Run
Gokaku can be run using one of the following methods:

- [Running from Source Files](#running-from-source-files)
- [Running with Docker](#running-with-docker)

Since no binary files are provided, please either compile and run the source code yourself or run it using the Docker image.
However, if you want to run Gokaku with TensorRT, you need to build from source files.

## Running from Source Files
### Build Instructions
Most of this program is written in Cython and C++.
To run Gokaku, you need to build the program to create modules for search and board evaluation.

First, install the required modules for building and running:
```
pip install numpy cython cmake
```

In addition to the above modules, PyTorch is required for building and running.
Check the CUDA version and other details of your environment, and install a suitable version of PyTorch.
Even in environments with ROCm installed, you should be able to run Gokaku by installing a ROCm-compatible version of PyTorch (ROCm environment operation has not been verified, and TensorRT cannot be used in ROCm environments).
```
pip install torch
```

If you want to run Gokaku using TensorRT, please also install Torch-TensorRT.
If Torch-TensorRT is not installed, a module that does not support TensorRT will be built.
```
pip install torch-tensorrt
```

Next, run `src/build.py` to compile the Cython and C++ code.
On Linux or macOS environments, `make` is required.
On Windows environments, `MSBuild` is required (MSBuild is included with Visual Studio).
```
python src/build.py
```

If compilation is successful, the compiled Cython module will be generated in `src/deepshogi/native`.

You can delete the generated files by running `src/build.py` with the `--clean` option:
```
python src/build.py --clean
```

### Running the Program
You can launch Gokaku by running the launch script `src/run.py`.
Specify an ExportedProgram model (`.ep`) or a PT2 package (`.pt2`) created with `src/compile.py` as the argument.
```
python src/run.py <model_file>
```

The model is loaded when the first `isready` command is received.
When an ExportedProgram is specified, it is compiled for the execution device at that time, which requires a C++ compiler and takes some time.
To avoid compiling at every startup, compile the model in advance with `src/compile.py` (see [Compiling the Model](#compiling-the-model)).

Gokaku operates via the USI (Universal Shogi Interface) protocol.
Here is a simple example of usage:
```
% python src/run.py b10c512-1000.ep
usi
id name Gokaku 2.4
id author Atsushi Takeda
option name Threads type spin default 16 min 1
option name CheckSearchDepth type spin default 21 min 1
option name CheckSearchNode type spin default 20000 min 1
option name CheckNodeDepth type spin default 2 min 0
option name PucbConstantInit type spin default 160 min 0
option name PucbConstantBase type spin default 3200 min 0
option name PucbMinVisitsRate type spin default 0 min 0
option name MaxVisits type spin default 1000000 min 1
option name NyugyokuRule type combo default 27 var 27 var 24
option name DrawTurn type spin default 512 min 1
option name Visits type spin default 50 min 1
option name Extends type spin default 0 min 0
option name Timelimit type spin default 120000 min 0
option name Ponder type check default false
option name MultiPV type spin default 1 min 1
option name Criterion type combo default value var value var visits
option name ResignThreshold type spin default 2 min 0 max 100
option name ResignTurn type spin default 50 min 0
option name InitialTurn type spin default 4 min 0
option name InitialWidth type spin default 16 min 1
option name InitialTemperature type spin default 100 min 0
option name InitialValueDelta type spin default 1 min 0
option name InitialWhiteOnly type check default false
usiok
isready
readyok
usinewgame
go
info multipv 1 nodes 1 score cp -23 pv 3i3h
bestmove 3i3h
```

Since Gokaku complies with the USI protocol, you can use it as a Shogi engine for a GUI that supports USI, such as [ShogiHome](https://sunfish-shogi.github.io/shogihome/).

To see the available options, run the script with the `--help` flag:
```
python src/run.py --help
```

### Compiling the Model
You can compile an ExportedProgram model into a PT2 package for the execution device in advance with `src/compile.py`:
```
python src/compile.py <exported-program-file> <pt2-file>
```

The backend is selected with the `--backend` option (`auto`, `aoti`, or `tensorrt`).
With `auto` (the default), TensorRT is used when Torch-TensorRT is available on Linux with an NVIDIA CUDA GPU; otherwise AOTInductor is used.
The target device is selected with the `--gpu` option (`-1` for the CPU); by default, CUDA/ROCm GPU 0, MPS GPU 0, or the CPU is used in this order.
The device type, GPU runtime, precision, and batch size are stored in the PT2 package, and `src/run.py` reads them when loading the model, so you do not need to specify them when launching Gokaku.
A PT2 package can only be used on the same kind of device and runtime as the one it was compiled for.

You can launch Gokaku with the created PT2 package specified as an argument:
```
python src/run.py <pt2-file>
```

If the ExportedProgram was exported with a dynamic batch dimension, you can choose any precision (`--fp16`) and batch size (`--batch-size`) when compiling.
If it was exported with a fixed batch size, the precision and batch size must match those stored in the ExportedProgram.
The following example compiles a model for TensorRT on GPU 0 and then launches Gokaku using that model (compilation only needs to be done once):
```
python src/compile.py --backend tensorrt --gpu 0 b10c512-1000.ep b10c512-1000.pt2
python src/run.py b10c512-1000.pt2
```

## Running with Docker

### Running in CUDA-enabled Environments
A Docker image for running Gokaku is available, which makes it easy to use Gokaku (TensorRT is not supported).

To run Docker version of Gokaku using GPU, you need to have NVIDIA drivers compatible with CUDA Version 12.6 or later and NVIDIA Container Toolkit installed.
If you can access the GPU from Docker by running the following command, you can run Docker version of Gokaku using GPU:
```
docker run --rm -i --gpus all pytorch/pytorch:2.12.0-cuda12.6-cudnn9-runtime nvidia-smi
```

When running it for the first time, you need to download the Docker image.
The Docker image intended for use with CUDA, `takedarts/gokaku:v2.4-cuda12.6`, is about 4 GB in size, so the download may take some time.
I recommend downloading the Docker image in advance by running the following command:
```
docker pull takedarts/gokaku:v2.4-cuda12.6
```

You can run the Gokaku Docker image by executing the following command in an environment where CUDA is available:
```
docker run -iq --rm --gpus all -v .:/workspace takedarts/gokaku:v2.4-cuda12.6 /opt/run.sh <model_file>
```
Use the `--gpus` option to specify the GPUs to use, and mount the current directory to the container's `/workspace` using `-v .:/workspace`.
Place the model file in the current directory or a subdirectory, and specify its path as `<model_file>`.

You can also specify options after the execution command.
If you add `--help` to the execution command, a list of available options will be displayed:
```
docker run -iq --rm --gpus all -v .:/workspace takedarts/gokaku:v2.4-cuda12.6 /opt/run.sh --help
```

### Running on CPU
A Docker image intended for CPU (AMD64) execution, `takedarts/gokaku:v2.4-cpu`, is also available (its image size is smaller than the CUDA version).
If you want to run computations on the CPU, execute the following command:
```
docker run -iq --rm -v .:/workspace takedarts/gokaku:v2.4-cpu /opt/run.sh <model_file>
```

If you want to run on an ARM64 CPU architecture, use the Docker image `takedarts/gokaku:v2.4-arm`:
```
docker run -iq --rm -v .:/workspace takedarts/gokaku:v2.4-arm /opt/run.sh <model_file>
```

## Execution Options
When running the startup script `src/run.py`, you can specify the following options:

| Option                      | Description                                                    | Default Value         |
|-----------------------------|----------------------------------------------------------------|-----------------------|
| `--help`                    | Display a list of available options                            |                       |
| `--visits <N>`              | Search visits (number of nodes in the search tree)        | 50                    |
| `--extends <N>` | Maximum search extensions | 0 |
| `--max-visits <N>` | Maximum search visits | 1,000,000 |
| `--timelimit <N>`           | Maximum thinking time (in seconds)                             | 120                   |
| `--criterion <S>`           | Criterion for selecting moves (`value` or `visits`)            | `value`               |
| `--ponder`                  | Enable pondering                                               | False                 |
| `--resign <R>`              | Predicted win rate threshold for resignation                   | 0.02                  |
| `--min-turn <N>`            | Minimum number of turns before resignation is allowed          | 50                   |
| `--initial-turn <N>`        | Number of opening turns with random moves                      | 4                     |
| `--initial-width <N>`       | Number of candidates for opening random moves                  | 16                    |
| `--initial-temperature <N>` | Temperature parameter for opening random moves                 | 1.0                   |
| `--initial-value-delta <R>` | Allowed win-probability drop for random moves | 0.01 |
| `--initial-white-only` | Use random opening moves only for White | False |
| `--nyugyoku-rule <N>`       | Points for the entering-king rule (27 or 24)                   | 27                    |
| `--draw-turn <N>` | Move count for a draw | 512 |
| `--check-search-depth <N>`  | Depth of checkmate search nodes                                | 21                    |
| `--check-search-node <N>`   | Number of checkmate search nodes                               | 20,000                |
| `--check-node-depth <N>`    | Depth of search nodes at which checkmate search is performed   | 2                     |
| `--pucb-constant-init <N>`  | Initial value of PUCB constant term                            | 1.6                   |
| `--pucb-constant-base <N>`  | Base value of PUCB constant term                               | 3200.0                |
| `--pucb-min-visits-rate <R>` | Minimum visit ratio prioritized by PUCB | 0.0 |
| `--client-name <S>`         | Client name to display                                         | `Gokaku`              |
| `--client-version <S>`      | Version information to display                                 | `2.4`                 |
| `--threads <N>`             | Number of threads to use for search                            | 16                    |
| `--gpus <N>`       | GPU ID(s) to use (comma-separated for multiple GPUs, -1 for the CPU); a `.ep` model is compiled on the first one | All GPUs of the same model as the first GPU for `.ep`, all compatible devices for `.pt2` |
| `--threads-per-gpu <N>`     | Number of inference threads per GPU                            | 2                     |
| `--cache-size <N>`          | Cache size for inference results                               | visits |
| `--verbose`                 | Enable log output to standard error                            | False                 |

### Visits, search extensions, and time limits

- Search ends when any of the following conditions is met:
  - Visits reach the target specified by `--visits`.
  - Visits reach the maximum specified by `--max-visits`.
  - A checkmate is found.
  - The time limit specified by `--timelimit` is reached.
- Search ends early when the most visited child exceeds 60% of the target specified by `--visits`.
- With `--extends N`, search can continue up to N more times when the selected move's win probability is strictly between 5% and 95% and another candidate has at least two thirds of its visits. Each extension increases the target visits by half the original requested value. No extension is performed when less than one second remains.
- The search tree is reused after moves.
- When the remaining time is given by `go`, a safety margin is subtracted from the thinking time: 2 seconds if byoyomi is at least 2 seconds, otherwise 10 seconds.

## Execution Examples
To start Gokaku using the model file `b10c512-1000.ep`, run the following command:
```
python src/run.py b10c512-1000.ep
```

To start Gokaku with the number of visits set to 1000 and the maximum thinking time set to 5 seconds, run the following command:
```
python src/run.py b10c512-1000.ep --visits 1000 --timelimit 5
```

## Tests
After building, run the CPU board, inference, and search tests with:
```
PYTHONPATH=src python -m unittest discover -s src/tests -p '*_test.py' -v
MYPYPATH=src python -m mypy --explicit-package-bases src
```

The tests generate and compile a small model, so no trained model or GPU is required (CMake and a C++ compiler are required).
The C++ tests are built with CMake and run as part of the Python tests.
See [src/tests/README.md](src/tests/README.md) for details.

## License
Starting with Gokaku version 2.2, the license has been changed to the MIT License.

- Gokaku version 2.1 and earlier: GPL-3.0 License
- Gokaku version 2.2 and later: MIT License

Some of the cmake build scripts use scripts provided under the Apache License 2.0.

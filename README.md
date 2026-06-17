# Gokaku
[English](./README.md) | [Japanese](./README_JP.md)

## Overview
Gokaku is a computer Shogi program developed using deep reinforcement learning from randomly generated game records. The deep learning model of Gokaku incorporates nested-bottleneck convolutions and multi-head attention, enabling it to efficiently grasp the overall state of the board. Its reinforcement learning procedure is inspired by approaches used in Katago and Gumbel AlphaZero, allowing it to efficiently learn a wide range of patterns.

Gokaku is a sibling program of the computer Go program [Maru](https://github.com/takedarts/maru). Gokaku shares the same deep learning model architecture, search algorithm, and reinforcement learning methodology as Maru.

You can check the improvement of Gokaku's playing strength through reinforcement learning on [this page](https://takeda-lab.jp/gokaku/).
The model files are available for download from [this release page](https://github.com/takedarts/gokaku/releases/tag/v2.3).

## How to Run
Gokaku can be run using one of the following methods:

- [Running from Source Files](#running-from-source-files)
- [Running with Docker](#running-with-docker)

Since no binary files are provided, please either compile and run the source code yourself or run it using the Docker image.
However, if you want to run Gokaku with TensorRT, you need to build from source files.

## Running from Source Files
### Build Instructions
Since most parts of this program are written in Cython and C++, the program code must be compiled before it can be run to create modules for search and board evaluation.

First, install the required modules for building and running:
```
pip install numpy cython cmake
```

In addition to the above modules, PyTorch is required for building and running. Please check the CUDA version installed in your environment and install the appropriate version of PyTorch for your execution environment.
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
At runtime, you need to specify the model file as a command-line argument (You can download the model file from [here](https://github.com/takedarts/gokaku/releases/tag/v2.3)):
```
python src/run.py <model_file>
```

Gokaku operates via the USI (Universal Shogi Interface) protocol.
Here is a simple example of usage:
```
% python src/run.py b4c256-900.model
usi
id name Gokaku 2.3
id author Atsushi Takeda
option name Threads type spin default 16 min 1
option name CheckSearchDepth type spin default 21 min 1
option name CheckSearchNode type spin default 20000 min 1
option name CheckNodeDepth type spin default 2 min 0
option name PucbConstantInit type spin default 160 min 0
option name PucbConstantBase type spin default 3200 min 0
option name NyugyokuRule type combo default 27 var 27 var 24
option name DrawTurn type spin default 512 min 1
option name Visits type spin default 50 min 1
option name Playouts type spin default 0 min 0
option name Timelimit type spin default 120000 min 0
option name Ponder type check default false
option name MultiPV type spin default 1 min 1
option name Criterion type combo default value var value var visits
option name ResignThreshold type spin default 2 min 0 max 100
option name ResignTurn type spin default 50 min 0
option name InitialTurn type spin default 4 min 0
option name InitialWidth type spin default 16 min 1
option name InitialTemperature type spin default 100 min 0
usiok
isready
readyok
usinewgame
go
info multipv 1 nodes 1 score cp -23 pv 3i3h
bestmove 3i3h
```

Since Gokaku supports the USI protocol, you can also use GUI applications that support USI protocol, such as [ShogiHome](https://sunfish-shogi.github.io/shogihome/).

To see the available options, run the script with the `--help` flag:
```
python src/run.py --help
```

### Running with TensorRT
If you build in an environment where Torch-TensorRT is installed, you can run Gokaku using TensorRT.
First, to use TensorRT, you need to compile Gokaku's inference model into a TensorRT model.
Run the following command to compile the model file into a TensorRT model:
```
python src/compile.py <torch-script-file> <tensorrt-file>
```

For `<torch-script-file>`, specify the model file that can be downloaded from [here](https://github.com/takedarts/gokaku/releases/tag/v2.3).
When you run the above command, a TensorRT format model file will be generated at `<tensorrt-file>`.

You can launch Gokaku using TensorRT by running the startup script with the created TensorRT model file specified as an argument:
```
python src/run.py <tensorrt-file>
```

When running Gokaku with TensorRT, you need to use the same compilation options for the TensorRT model and the execution options for `run.py`.
If you specify the `--fp16` or `--batch-size` options when compiling the TensorRT model, specify the same options in the `run.py` execution command as well.
The following example shows how to compile a TensorRT model with half-precision floating point (FP16) and batch size 16, and then launch Gokaku using that model (TensorRT model compilation only needs to be done once):
```
python src/compile.py --fp16 --batch-size 16 b4c256-900.model b4c256-900.rt.model
python src/run.py --fp16 --batch-size 16 b4c256-900.rt.model
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
The Docker image intended for use with CUDA, `takedarts/gokaku:v2.3.1-cuda12.6`, is about 4 GB in size, so the download may take some time.
I recommend downloading the Docker image in advance by running the following command:
```
docker pull takedarts/gokaku:v2.3.1-cuda12.6
```

You can run the Gokaku Docker image by executing the following command in an environment where CUDA is available (You can download the model file from [here](https://github.com/takedarts/gokaku/releases/tag/v2.3)):
```
docker run -iq --rm --gpus all -v .:/workspace takedarts/gokaku:v2.3.1-cuda12.6 /opt/run.sh <model_file>
```
Use the `--gpus` option to specify the GPUs to use, and mount the current directory to the container's `/workspace` using `-v .:/workspace`.
Place the model file in the current directory and specify its path as `<model_file>`.

You can also specify options after the execution command.
If you add `--help` to the execution command, a list of available options will be displayed:
```
docker run -iq --rm --gpus all -v .:/workspace takedarts/gokaku:v2.3.1-cuda12.6 /opt/run.sh --help
```

### Running on CPU
A Docker image intended for CPU (AMD64) execution, `takedarts/gokaku:v2.3.1-cpu`, is also available (its image size is smaller than the CUDA version).
If you want to run computations on the CPU, execute the following command:
```
docker run -iq --rm -v .:/workspace takedarts/gokaku:v2.3.1-cpu /opt/run.sh <model_file>
```

If you want to run on an ARM64 CPU architecture, use the Docker image `takedarts/gokaku:v2.3.1-arm`:
```
docker run -iq --rm -v .:/workspace takedarts/gokaku:v2.3.1-arm /opt/run.sh <model_file>
```

## Execution Options
When running the startup script `src/run.py`, you can specify the following options:

| Option                      | Description                                                    | Default Value         |
|-----------------------------|----------------------------------------------------------------|-----------------------|
| `--help`                    | Display a list of available options                            |                       |
| `--visits <N>`              | Number of searches (number of nodes in the search tree)        | 50                    |
| `--playouts <N>`            | Number of playouts (number of leaves in the search tree)       | 0                     |
| `--timelimit <N>`           | Maximum thinking time (in seconds)                             | 120                   |
| `--criterion <S>`           | Criterion for selecting moves (`value` or `visits`)            | `value`               |
| `--ponder`                  | Enable pondering                                               | False                 |
| `--resign <R>`              | Predicted win rate threshold for resignation                   | 0.02                  |
| `--min-turn <N>`            | Minimum number of turns before resignation is allowed          | 100                   |
| `--initial-turn <N>`        | Number of opening turns with random moves                      | 4                     |
| `--initial-width <N>`       | Number of candidates for opening random moves                  | 16                    |
| `--initial-temperature <N>` | Temperature parameter for opening random moves                 | 1.0                   |
| `--nyugyoku-rule <N>`       | Points for the entering-king rule (27 or 24)                   | 27                    |
| `--check-search-depth <N>`  | Depth of checkmate search nodes                                | 21                    |
| `--check-search-node <N>`   | Number of checkmate search nodes                               | 20,000                |
| `--check-node-depth <N>`    | Depth of search nodes at which checkmate search is performed   | 2                     |
| `--pucb-constant-init <R>`  | Initial value of PUCB constant term                            | 1.6                   |
| `--pucb-constant-base <R>`  | Base value of PUCB constant term                               | 3200.0                |
| `--client-name <S>`         | Client name to display                                         | `Gokaku`              |
| `--client-version <S>`      | Version information to display                                 | `2.3`                 |
| `--threads <N>`             | Number of threads to use for search                            | 16                    |
| `--batch-size <N>`          | Batch size for board evaluation                                | 32                    |
| `--gpus <N,...>`       | GPU ID(s) to use (comma-separated for multiple GPUs)           | All available GPUs    |
| `--fp16`                    | Use half-precision floating point (FP16)                       | False                 |
| `--threads-per-gpu <N>`     | Number of inference threads per GPU                            | 2                     |
| `--cache-size <N>`          | Cache size for inference results                               | max(visits, playouts) |
| `--verbose`                 | Enable log output to standard error                            | False                 |

### Relationship between Visits, Playouts, and Timelimit
The termination condition of the search is determined by the values specified with the `--visits`, `--playouts`, and `--timelimit` options. The search ends either when both the number of visits and the number of playouts exceed their specified values, or when the elapsed thinking time exceeds the specified number of seconds.

### Visits and Playouts
In Gokaku, the number of visits is defined as "the number of nodes in the search tree," and the number of playouts is defined as "the number of leaves in the search tree."
The definition of the number of visits is the same as in other programs such as LeelaZero and KataGo.
On the other hand, the definition of the number of playouts differs from other programs.

In situations where the number of candidate moves is small, the number of leaves tends not to increase even when expanding nodes.
Therefore, when controlling the size of the search tree based on the number of playouts, a larger search tree tends to be created compared to when controlling based on the number of visits.
This is because in situations where a determined sequence of moves continues, it is less important to evaluate situations in the middle of the sequence, and it is more important to evaluate situations after the sequence breaks.
Compared to specifying the number of visits, specifying the same value for the number of playouts tends to result in longer search times. However, because deeper search trees can be created, this may lead to improved playing strength.

Note that Gokaku always reuses the search tree. Therefore, even when running a search with the number of playouts specified, if the game progresses as expected, the search may finish in a short time.

## Execution Examples
To start Gokaku using the model file `b4c256-900.model`, run the following command:
```
python src/run.py b4c256-900.model
```

To start Gokaku with the number of visits set to 1000 and the maximum thinking time set to 5 seconds, run the following command:
```
python src/run.py b4c256-900.model --visits 1000 --timelimit 5
```

## Compatibility with Gokaku Version 2.2 and Earlier
- Model files for Gokaku version 2.1 and earlier cannot be used with Gokaku version 2.3.
- Model files for Gokaku version 2.2 do not support compilation to TensorRT models.

## License
Starting with Gokaku version 2.2, the license has been changed to the MIT License.
- Gokaku version 2.1 and earlier: GPL-3.0 License
- Gokaku version 2.2 and later: MIT License

Some of the cmake build scripts use scripts provided under the Apache License 2.0.

# Gokaku
[English](./README.md) | [Japanese](./README_JP.md)

## 概要
Gokakuはランダム着手の棋譜からの深層強化学習を用いて作成されたコンピュータ将棋プログラムです。Gokakuの深層学習モデルはNested-Bottleneck構造のConvolutionやMulti-Head Attentionを含んでおり、盤面全体の状況を効率よく把握できる構造となっています。また、Gokakuの強化学習手順はKatagoやGumbel AlphaZeroを参考にして設計されており、効率よく多くのパターンを学習できるように設計されています。

Gokakuはコンピュータ囲碁プログラム[Maru](https://github.com/takedarts/maru)の兄弟プログラムです。Gokakuの深層学習モデル・探索アルゴリズム・強化学習手順はMaruと同じ手法を用いています。

強化学習によるGokakuの棋力向上を[こちらのページ](https://takeda-lab.jp/gokaku/)で確認できます。
また、モデルファイルは[こちらのリリースページ](https://github.com/takedarts/gokaku/releases/tag/v2.3)からダウンロードできます。

## 実行方法
Gokakuは以下のいずれかの方法で実行できます。
- [ソースファイルからの実行](#ソースファイルからの実行)
- [Dockerを使用した実行](#dockerを使用した実行)

バイナリファイルは提供していないため、ソースファイルからビルドして実行するか、Dockerイメージを使用して実行してください。
ただし、TensorRTを使用してGokakuを実行する場合は、ソースファイルからビルドする必要があります。

## ソースファイルからの実行
### ビルド方法
このプログラムの大部分はCythonとC++によって記述されています。
Gokakuを実行するためにはプログラムをビルドして、探索や盤面評価を実行するモジュールを作成する必要があります。

まず、ビルドして実行するために必要となるモジュールをインストールします。
```
pip install numpy cython cmake
```

上記のモジュールに加えて、ビルドと実行のためにPyTorchが必要となります。
環境にインストールされているCUDAのバージョンなどを確認し、実行環境に応じたPyTorchをインストールしてください。
ROCmがインストールされている環境でも、ROCm対応のPyTorchをインストールすることでGokakuを実行できるはずです（ROCm環境での動作検証は行っていません・ROCm環境ではTensorRTを使用できません）。
```
pip install torch
```

TensorRTを使用してGokakuを実行する場合は、Torch-TensorRTもインストールしてください。
Torch-TensorRTがインストールされていない場合は、TensorRTをサポートしないモジュールがビルドされます。
```
pip install torch-tensorrt
```

次に、`src/build.py`を実行してCythonとC++のコードをビルドします。
Linux環境やMacOS環境では`make`が必要となります。
Windows環境では`MSBuild`が必要となります（MSBuildはVisual Studioに含まれています）。
```
python src/build.py
```

コンパイルに成功すると`src/deepshogi/native`にコンパイルされたCythonモジュールが生成されます。

なお、オプション`--clean`を指定して`src/build.py`を実行すると生成されたファイルを削除します。
```
python src/build.py --clean
```

### 実行方法
起動スクリプト`src/run.py`を実行することでGokakuを起動できます。
このとき、引数としてモデルファイルを指定する必要があります（モデルファイルは[こちら](https://github.com/takedarts/gokaku/releases/tag/v2.3)からダウンロードできます）。
```
python src/run.py <model_file>
```

Gokakuの操作はUSI（Universal Shogi Interface）プロトコルを介して行います。
以下に簡単な操作例を示します。
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

GokakuはUSIプロトコルに準拠しているため、[ShogiHome](https://sunfish-shogi.github.io/shogihome/)などのUSIプロトコルに対応したGUIを使用することもできます。

起動スクリプトに`--help`オプションを指定すると、指定可能なオプションの一覧が表示されます。
```
python src/run.py --help
```

### TensorRTを使用した実行
Torch-TensorRTがインストールされた環境でビルドした場合、TensorRTを使用してGokakuを実行できます。
まず、TensorRTを使用するために、Gokakuの推論モデルをTensorRTモデルへコンパイルする必要があります。
以下のコマンドを実行して、モデルファイルをTensorRTモデルへコンパイルしてください。
```
python src/compile.py <torch-script-file> <tensorrt-file>
```

`<torch-script-file>`には[こちら](https://github.com/takedarts/gokaku/releases/tag/v2.3)からダウンロードできるモデルファイルを指定してください。
上記のコマンドを実行すると、`<tensorrt-file>`にTensorRT形式のモデルファイルが生成されます。

作成されたTensorRTモデルファイルを引数に指定して起動スクリプトを実行することで、TensorRTを使用してGokakuを起動できます。
```
python src/run.py <tensorrt-file>
```

ただし、TensorRTを使用してGokakuを実行する場合、TensorRTモデルのコンパイルオプションと`run.py`の実行オプションを同じものにする必要があります。
TensorRTモデルをコンパイルする際に`--fp16`や`--batch-size`オプションを指定した場合、`run.py`の実行コマンドにも同じオプションを指定してください。
以下はTensorRTモデルを半精度浮動小数点数（FP16）でバッチサイズ16に設定してコンパイルし、そのモデルを使用してGokakuを起動する例です（TensorRTモデルのコンパイルは1度だけで十分です）。
```
python src/compile.py --fp16 --batch-size 16 b4c256-900.model b4c256-900.rt.model
python src/run.py --fp16 --batch-size 16 b4c256-900.rt.model
```

## Dockerを使用した実行
### CUDAを使用できる環境での実行
Gokakuを実行できるDockerイメージが用意されており、これを使用することで簡単にGokakuを実行できます（TensorRTはサポートしていません）。

GPUを使用してDocker版のGokakuを実行するためには、Version 13.0以降のCUDAに対応したNVIDIAドライバとNVIDIA Container Toolkitがインストールされている必要があります。
以下のコマンドを実行してDockerからGPUにアクセスできるのであれば、GPUを使用してDocker版のGokakuを実行できます。
```
docker run --rm -i --gpus all pytorch/pytorch:2.12.0-cuda13.0-cudnn9-runtime nvidia-smi
```

最初に実行する際にはDockerイメージをダウンロードする必要があります。
CUDAを使用することを想定したDockerイメージ `takedarts/gokaku:v2.3-cuda13.0` は、イメージサイズが約3GBとなっているため、ダウンロードに時間がかかる場合があります。
以下のコマンドを実行して、あらかじめDockerイメージをダウンロードしておくことをおすすめします。
```
docker pull takedarts/gokaku:v2.3-cuda13.0
```

CUDAを使用できる環境で以下のコマンドを実行することで、GokakuのDockerイメージを実行できます（モデルファイルは[こちら](https://github.com/takedarts/gokaku/releases/tag/v2.3)からダウンロードできます）。
```
docker run -iq --rm --gpus all -v .:/workspace takedarts/gokaku:v2.3-cuda13.0 /opt/run.sh <model_file>
```
オプション`--gpus`で使用するGPUを指定し、`-v .:/workspace`でカレントディレクトリをコンテナ内の`/workspace`にマウントします。
モデルファイルをカレントディレクトリ以下に置き、`<model_file>`にそのファイルパスを指定してください。

実行コマンドに続けてオプションを指定することもできます。
実行コマンドに`--help`を指定すると、指定可能なオプションの一覧が表示されます。
```
docker run -iq --rm --gpus all -v .:/workspace takedarts/gokaku:v2.3-cuda13.0 /opt/run.sh --help
```

### CPUでの実行
CPU（AMD64）での実行を想定したDockerイメージ `takedarts/gokaku:v2.3-cpu` も用意されています（イメージサイズはCUDA版よりも小さくなっています）。
CPUで計算を実行する場合は以下のコマンドを実行してください。
```
docker run -iq --rm -v .:/workspace takedarts/gokaku:v2.3-cpu /opt/run.sh <model_file>
```

ARM64アーキテクチャのCPUで実行する場合は `takedarts/gokaku:v2.3-arm` のDockerイメージを使用してください。
```
docker run -iq --rm -v .:/workspace takedarts/gokaku:v2.3-arm /opt/run.sh <model_file>
```

## 実行オプション
起動スクリプト`src/run.py`を実行する際に以下のオプションを指定できます。

| オプション                  | 説明                                              | デフォルト値          |
|-----------------------------|---------------------------------------------------|-----------------------|
| `--help`                    | 利用可能なオプション一覧を表示                    |                       |
| `--visits <N>`              | 探索回数（探索木のノード数）                      | 50                    |
| `--playouts <N>`            | プレイアウト回数（探索木のリーフ数）              | 0                     |
| `--timelimit <N>`           | 思考時間の上限を指定（秒）                        | 120                   |
| `--criterion <S>`           | 着手を選択する基準（`value`または`visits`）       | `value`               |
| `--ponder`                  | 先読みを有効にする                                | False                 |
| `--resign <R>`              | 投了するときの予想勝率                            | 0.02                  |
| `--min-turn <N>`            | 投了するときの最小ターン数                        | 100                   |
| `--initial-turn <N>`        | ランダムに着手する初期ターン数                    | 4                     |
| `--initial-width <N>`       | ランダムに着手するときの候補手の数                | 16                    |
| `--initial-temperature <N>` | ランダムに着手するときの温度パラメータ            | 1.0                   |
| `--nyugyoku-rule <N>`       | 入玉ルールの点数（27または24）                    | 27                    |
| `--check-search-depth <N>`  | 詰み探索ノードの深さ                              | 21                    |
| `--check-search-node <N>`   | 詰み探索ノードの数                                | 20,000                |
| `--check-node-depth <N>`    | 詰み探索を実行する探索ノードの深さ                | 2                     |
| `--pucb-constant-init <N>`  | PUCBの定数項の初期値                              | 1.6                   |
| `--pucb-constant-base <N>`  | PUCBの定数項の基数                                | 3200.0                |
| `--client-name <S>`         | 表示するクライアント名                            | `Gokaku`              |
| `--client-version <S>`      | 表示するバージョン情報                            | `2.3`                 |
| `--threads <N>`             | 探索に使用するスレッド数                          | 16                    |
| `--batch-size <N>`          | 盤面評価のバッチサイズ                            | 32                    |
| `--gpus <N>`                | 使用するGPUのID（複数指定する場合はコンマ区切り） | 使用可能なすべてのGPU |
| `--fp16`                    | 半精度浮動小数点数（FP16）を使用する              | False                 |
| `--threads-per-gpu <N>`     | GPUごとに使用する推論スレッド数                   | 2                     |
| `--cache-size <N>`          | 推論結果のキャッシュサイズ                        | max(visits, playouts) |
| `--verbose`                 | 標準エラー出力へのログ出力を有効にする            | False                 |

### 訪問回数・プレイアウト回数・思考時間の関係
`--visits`オプション・`--playouts`オプション・`--timelimit`オプションのそれぞれで指定された値によって探索の終了条件が決まります。探索は「訪問回数とプレイアウト回数の両方が指定された回数を超えた場合」もしくは「思考時間が指定された秒数を超えた場合」のいずれかが満たされた時点で終了します。

### 訪問回数とプレイアウト回数
Gokakuでは、訪問回数を「探索木のノード数」、プレイアウト回数を「探索木のリーフ数」として定義しています。
訪問回数の定義は、LeelaZeroやKataGoなどの他のプログラムと同様です。
一方で、プレイアウト回数の定義は、他のプログラムとは異なります。

候補手の数が少ない局面では、ノードを展開したとしてもリーフ数が増加しにくくなります。
そのため、プレイアウト回数を基準に探索木の大きさを制御すると、訪問回数を基準とした場合よりも大きな探索木が作成されやすくなります。
このようにしているのは、決まった手順が続く局面では、その手順の途中にある局面を評価する重要性は低く、手順が途切れた後の局面を評価することが重要だと考えているためです。
訪問回数を指定した場合と比べると、同じ値のプレイアウト回数を指定した場合は探索時間が長くなる傾向があります。
一方で、より深い探索木を作成できるため、棋力の向上につながる可能性があります。

なお、Gokakuでは常に探索木を再利用します。そのため、プレイアウト回数を指定して探索を実行した場合でも、局面が予想どおりに進行したときは、短時間で探索が終了することがあります。

## 実行例
モデルファイル`b4c256-900.model`を使用してGokakuを起動する場合、以下のコマンドを実行します。
```
python src/run.py b4c256-900.model
```

訪問回数を1000回に設定し、思考時間の上限を5秒に設定してGokakuを起動する場合、以下のコマンドを実行します。
```
python src/run.py b4c256-900.model --visits 1000 --timelimit 5
```

## Gokaku version 2.2以前との互換性
- Gokaku version 2.1以前のモデルファイルはGokaku version 2.3では使用できません。
- Gokaku version 2.2のモデルファイルはTensorRTモデルへのコンパイルをサポートしていません。

## ライセンス
Gokaku version 2.2から、ライセンスをMIT Licenseに変更しました。
 - Gokaku version 2.1以前：GPL-3.0 License
 - Gokaku version 2.2以降：MIT License

ただし、cmakeのビルドスクリプトの一部はApache License 2.0で提供されているスクリプトを使用しています。


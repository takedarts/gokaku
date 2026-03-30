# Gokaku
[English](./README.md) | [Japanese](./README_JP.md)

## 概要
Gokakuはランダム着手の棋譜からの深層強化学習を用いて作成されたコンピュータ将棋プログラムです。Gokakuの深層学習モデルはNested-Bottleneck構造のConvolutionやMulti-Head Attentionを含んでおり、盤面全体の状況を効率よく把握できる構造となっています。また、Gokakuの強化学習手順はKatagoやGumbel AlphaZeroを参考にして設計されており、効率よく多くのパターンを学習できるように設計されています。

Gokakuはコンピュータ囲碁プログラム[Maru](https://github.com/takedarts/maru)の兄弟プログラムです。Gokakuの深層学習モデル・探索アルゴリズム・強化学習手順はMaruと同じ手法を用いています。

強化学習によるGokakuの棋力向上を[こちらのページ](https://takeda-lab.jp/gokaku/)で確認できます。
また、モデルファイルは[こちらのリリースページ](https://github.com/takedarts/gokaku/releases/tag/v2.2)からダウンロードできます。

## 実行方法
Gokakuは以下のいずれかの方法で実行できます。
- [ソースファイルからの実行](#ソースファイルからの実行)
- [Dockerを使用した実行](#dockerを使用した実行)

## ソースファイルからの実行
### ビルド方法
このプログラムの大部分はCythonとC++によって記述されているため、プログラムを動作させるためにはプログラムコードをコンパイルする必要があります。

まず、コンパイルするために必要となるモジュールをインストールします。
```
pip install numpy cython cmake
```

コンパイルするためにPyTorchが必要となりますが、このモジュールはcudaなどの実行環境に応じたものをインストールしてください。
```
pip install torch
```

次に、`src/build.py`を実行してCythonとC++のコードをコンパイルします。
Linux環境やMacOS環境では`make`が必要となります。
Windows環境では`MSBuild`が必要となります（MSBuildはVisual Studioに含まれています）。
```
python src/build.py
```

コンパイルに成功すると`src/deepgo/native`にコンパイルされたCythonモジュールが生成されます。

なお、以下のコマンドを実行すると生成されたファイルを削除します。
```
python src/build.py --clean
```

### 実行方法
起動スクリプト`src/run.py`を実行することでGokakuを起動できます。
このとき、実行コマンドの引数としてモデルファイルを指定する必要があります（モデルファイルは[こちら](https://github.com/takedarts/gokaku/releases/tag/v2.2)からダウンロードできます）。
```
python src/run.py <model_file>
```

Gokakuの操作はUSI（Universal Shogi Interface）プロトコルを介して行います。
以下に簡単な操作例を示します。
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

GokakuはUSIプロトコルに準拠しているため、[ShogiHome](https://sunfish-shogi.github.io/shogihome/)などのUSIプロトコルに対応したGUIを使用することもできます。

起動スクリプトに`--help`オプションを指定すると、指定可能なオプションの一覧が表示されます。
```
python src/run.py --help
```

## Dockerを使用した実行
Gokakuを実行できるDockerイメージが用意されており、これを使用することで簡単にGokakuを実行できます。
CUDAを使用できる環境で以下のコマンドを実行することで、GokakuのDockerイメージを取得して実行できます（モデルファイルは[こちら](https://github.com/takedarts/gokaku/releases/tag/v2.2)からダウンロードできます）。
```
docker run -i --rm --gpus all -v .:/workspace -q takedarts/gokaku:v2.2-cuda12.6 /opt/run.sh <model_file>
```
オプション`--gpus`で使用するGPUを指定し、`-v .:/workspace`でカレントディレクトリをコンテナ内の`/workspace`にマウントします。
モデルファイルをカレントディレクトリ以下に置き、`<model_file>`にそのファイルパスを指定してください。

Dockerのイメージは、CUDAやPyTorchの実行環境がインストールされているため、約4GBのサイズとなっています。
最初に実行する際にはDockerイメージのダウンロードが必要となるため、インターネット接続が必要となり、ダウンロードには時間がかかる場合があります。

もし実行前にDockerイメージをダウンロードしておきたい場合は、以下のコマンドを実行してください。
```
docker pull takedarts/gokaku:v2.2-cuda12.6
```

実行コマンドに続けてオプションを指定することもできます。
実行コマンドに`--help`を指定すると、指定可能なオプションの一覧が表示されます。
```
docker run -i --rm --gpus all -v .:/workspace -q takedarts/gokaku:v2.2-cuda12.6 /opt/run.sh --help
```

CPUでの実行を想定したDockerイメージも用意されています（イメージサイズはCUDA版よりも小さくなっています）。
CPUで計算を実行する場合は以下のコマンドを実行してください。
```
docker run -i --rm -v .:/workspace -q takedarts/gokaku:v2.2-cpu /opt/run.sh <model_file>
```

## 実行オプション
起動スクリプト`src/run.py`を実行する際に以下のオプションを指定できます。

| オプション                 | 説明                                              | デフォルト値        |
|----------------------------|---------------------------------------------------|---------------------|
| `--help`                   | 利用可能なオプション一覧を表示                    |                     |
| `--visits <N>`             | 探索回数（探索木のノード数）                      | 50                  |
| `--playouts <N>`           | プレイアウト回数（探索木のリーフ数）              | 0                   |
| `--timelimit <N>`          | 思考時間の上限を指定（秒）                        | 120                 |
| `--search-method <S>`      | 探索ノードを選ぶ基準（`ucb1`または`pucb`）        | `pucb`              |
| `--ponder`                 | 先読みを有効にする                                | False               |
| `--resign <R>`             | 投了するときの予想勝率                            | 0.02                |
| `--min-turn <N>`           | 投了するときの最小ターン数                        | 100                 |
| `--initial-turn <N>`       | ランダムに着手する初期ターン数                    | 4                   |
| `--initial-width <N>`      | ランダムに着手するときの候補手の数                | 16                  |
| `--nyugyoku-rule <N>`      | 入玉ルールの点数（27または24）                    | 27                  |
| `--check-search-depth <N>` | 詰み探索ノードの深さ                              | 31                  |
| `--check-search-node <N>`  | 詰み探索ノードの数                                | 10,000              |
| `--check-node-depth <N>`   | 詰み探索を実行する探索ノードの深さ                | 4                   |
| `--client-name <S>`        | 表示するクライアント名                            | `Gokaku`            |
| `--client-version <S>`     | 表示するバージョン情報                            | `2.2`               |
| `--client-author <S>`      | 表示する作者名                                    | `Atsushi Takeda`    |
| `--threads <N>`            | 探索に使用するスレッド数                          | 16                  |
| `--batch-size <N>`         | 盤面評価のバッチサイズの最大値                    | 2048                |
| `--gpus <N>`               | 使用するGPUのID（複数指定する場合はコンマ区切り） |                     |
| `--fp16`                   | 半精度浮動小数点数（FP16）を使用する              | False               |
| `--verbose`                | 標準エラー出力へのログ出力を有効にする            | False               |

#### 訪問回数・プレイアウト回数・思考時間の関係
`--visits`オプション・`--playouts`オプション・`--timelimit`オプションのそれぞれで指定された値によって探索の終了条件が決まります。探索は「訪問回数とプレイアウト回数の両方が指定された回数を超えた場合」もしくは「思考時間が指定された秒数を超えた場合」のいずれかが満たされた時点で終了します。

## 実行例
モデルファイル`b4c256-700.model`を使用してGokakuを起動する場合、以下のコマンドを実行します。
```
python src/run.py b4c256-700.model
```

訪問回数を1000回に設定し、思考時間の上限を5秒に設定してGokakuを起動する場合、以下のコマンドを実行します。
```
python src/run.py b4c256-700.model --visits 1000 --timelimit 5
```

## Gokaku version 2.1との互換性
Gokaku version 2.2とversion 2.1以前のモデルファイルでは入力信号が異なるため、version 2.1以前のモデルファイルをversion 2.2で使用すると正しく動作しません。
version 2.1以前のモデルファイルを使用する場合はGokaku version 2.1以前のプログラムを使用してください。

## ライセンス
Gokaku version 2.2から、ライセンスをMIT Licenseに変更しました。
 - Gokaku version 2.1以前：GPL-3.0 License
 - Gokaku version 2.2以降：MIT License

# Efficient and High-quality HCA Decoder

高速なデコードと内部 64 bit による高品質な処理を求めた HCA デコーダ。

## ビルド

make でビルドできる。

[libsndfile](http://www.mega-nerd.com/libsndfile/) のインストールが必須。

make 時に pkg-config を呼ぶため、pkg-config がない場合は Makefile を書き換えること。

`make install` でインストールできる。  
デフォルトでは `$HOME/.local` にインストールするので、システムにインストールしたい場合には次のようにする。

    sudo make install PREFIX=/usr/local

## 使い方

    hca -o output.wav -k key input.hca

### 鍵

`-k` オプションは 10 進数入力になる。0x を先頭につけると 16 進数で入力できる。

多くの HCA デコーダと同じように `-a` および `-b` を用いても指定可能。

サブキーにも対応しているが動作は未確認。

### 音量

デフォルトでは HCA に書かれているボリュームを適用する。  
`--ignore-rva` オプションで無視できる。

他のデコーダと同様 `-v` オプションで出力時ボリュームを指定できる。  
`--ignore-rva` オプションが指定されていない場合、両方の値の積を出力ボリュームとする。

HCA は浮動小数点フォーマットであるためクリッピングが発生しうる。  
`-n` オプションをつけると、クリッピングが起きないように全体の音量を調整する。  
クリッピングがもともと起きない音声については何も行わない。

### 出力フォーマット

出力ファイル名に合わせてフォーマットを推定する。  
`-t` オプションで上書きできる。指定できるものは以下の通り。

- wav
- raw
- flac
- ogg

`-f` オプションで出力のビット形式を指定できる。次の形式で指定できる。

- u8: unsigned 8bit
- s8: signed 8bit（wav 不可）
- s16: signed 16bit（wav / flac のデフォルト）
- s24: signed 24bit
- s32: signed 32bit（flac 不可）
- f32: float 32bit（flac 不可・raw のデフォルト）
- f64: float 64bit（flac 不可）

また s32le のように末尾に le / be を加えるとエンディアンを指定できる。

なお ogg は -f オプションを無視する。

### ループ

フロントエンドとしては実装していない。

### その他のオプション

`-h` オプションを付けて実行すると詳細なヘルプを出力できる。

```
usage: hca [options] input.hca

options:
  -o, --output      output filename
  -t, --type        output file type (wav / raw / flac / ogg)
  -f, --bits        output file format (e.g. u8 / s16 / f32)
  -q, --quality     output file quality for flac / ogg
                    from 0.0 (lower bitrate) to 1.0 (higher bitrate)

  -a                set key (lower bits)
  -b                set key (higher bits)
  -k, --key         set key (in decimal)
  -s, --sub-key     set sub key (in decimal)
  -v, --volume      set volume
      --ignore-rva  ignore relative volume info in hca file
  -n, --normalize   normalize waveforms not to be clipped

  -i, --info        show info about input file and quit
      --verbose     verbose output
  -h, --help        show this help message
      --version     show version info
```

一部のオプションは、多くの HCA デコーダにある実装と合わせてある。

## libhca

HCA デコーダを他のプログラムに組み込めるよう、libhca.a として提供している。

include/hca 内のヘッダファイルを用いるとよい。

## オリジナルソースコード

オリジナルは [2ch 実装](http://medaka.5ch.net/test/read.cgi/gameurawaza/1485136997/682) による。

## 本実装の差分

### 64bit 処理

すべての値を 64bit で処理している。

各定数については float の範囲で誤差がない計算法を適用している。

なお MDCT の窓関数については不明なため 32bit になっている。

### 高速化

環境や音声にもよるが、およそ [KinoMyu/FastHCADecoder](https://github.com/KinoMyu/FastHCADecoder) のマルチスレッド版程度の速度が出る。  
当然本実装は単一スレッドのみで処理している。

高速化はおもに修正離散コサイン変換（MDCT）による貢献が大きい。  
計算には[京都大学・大浦先生の FFT ライブラリ](http://www.kurims.kyoto-u.ac.jp/~ooura/fft-j.html)（通称大浦 FFT）を用い、HCA の 256 点 MDCT 向けに若干高速化している。

デコード処理はすべて書き直しているが、ほとんど高速化に貢献するような書き換えはない。

### 他の HCA デコーダとの出力結果の差

64bit 処理と MDCT の書き換えにより -130 dB 程度の差が生じるようだが、聴覚上の差にはならない。

~~そもそも不可逆圧縮のデコード結果に対する不一致性など議論するようなものではない。~~

~~ノイズ部分など特に音量が小さい部分については少しまろやかな音声が出ている気がする。~~

なお、浮動小数点処理か最適化の影響か不明だが、環境によって結果が異なる場合がある（ようである）。

### オリジナル実装の誤り

本家デコーダの結果との比較により、本リポジトリの実装が正しいことを確認している。

#### fmt チャンクの処理

fmt チャンクの最後の 2 つの 16bit の値は、最初と最後の無視するサンプル数を表す。

これを無視した他の実装では 1024 の倍数のサンプル数でしか処理できないうえ、最初のブロックの MDCT の結果による不正なサンプルを排除できない。  
一部音声については、実際に不正なサンプルが出力されることを確認している。

本実装では不正なサンプル問題・1024 の倍数問題を解決している。

なお loop チャンクも同様と思われる。

#### ビット数が足りないデータの処理

多くのデコーダでは、ブロックデータ内のビット数が足りない場合に 0 を返す処理をしている。  
この処理を行う場合、最後に詰められているデータを破棄することになり妥当でない。

実際にデコードを行うと 1bit 足りない場合が多く発生し、本家の実装ではどうやらこれを無視していないようである。

#### デコードステップ 4（ステレオ音声の高域復元処理）

多くの実装は `f2 = f1 - 2` としているが、`f2 = 2 - f1` が正しい。

## ライセンス

MIT ライセンスによる。license.txt を参照のこと。

### libsndfile

これは Erik de Castro Lopo の著作物であって、LGPL によるライセンスが適用される。

フロントエンドのみに用いられるため、libhca のみを用いる場合には libsndfile は含まれない。

### 大浦 FFT

これは京都大学・大浦先生の著作物であり、本実装に用いる MDCT に対して最適化を行っている。

## 既知の問題

### 浮動小数点の wav を書き出すと不正な fmt チャンクが出力される

libsndfile の問題。

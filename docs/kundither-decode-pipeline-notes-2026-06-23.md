# k_undither decode pipeline notes (2026-06-23)

このメモは、`sixel2png -d k_undither` を通常 decode に近い速度へ寄せる
ための議論を整理したものです。主対象は、Floyd-Steinberg 誤差拡散された
16-80 色程度の SIXEL 画像です。

## 議論の流れ

最初の問題意識は、`sixel2png -d k_undither` の逆量子化処理が通常 decode
から大きく遅れていることでした。まず代表的な画像で遅さを測り、最終的には
pipeline 化、band 並列化、SIMD 化で通常 decode に近づける方針でした。

その後、`--direct` と `-d k_undither` が併用できない制限は、技術的な制約
というより実装上の未対応だと整理しました。そこで作業順は、まず
`--direct` でも undither を通せるようにし、次に速度改善へ進む形にしました。

`k_undither` は indexed pixels と palette を入力に取りますが、処理そのものは
RGB の局所 filter です。palette は近傍色の similarity と threshold 推定の
ヒントとして使われます。このため direct 出力でも、内部的に indexed decode を
保てるなら、palette-aware な filter を適用してから RGBA へ昇格できます。

計算量については、pixel 数に対しては近傍数を定数とする 1 次処理です。
palette size に対する処理も cache でき、今回の主戦場である 32-80 色では、
O(n^2) 的な支配ではなく、全画面 pass の追加、memory traffic、cache locality、
同期待ちが主なコストになります。

品質を少し削って速度を取る案として、Floyd-Steinberg 前提の causal
neighborhood を試すことになりました。比較対象は 2x2 の 3-neighbor、
2x3 系の 4-neighbor、現行 3x3 の 8-neighbor です。edge protection は
最初の fast path では削ってよく、MS-SSIM で品質差を確認します。

Cocoa backend では 2x bilinear scaling によって dithering noise がある程度
隠れる可能性があります。そのため、元サイズだけでなく 2x bilinear 後の品質も
比較対象に入れます。

parallel decode については、現状は worker が band を decode しても request が
全 worker を join してから返るため、上側 band を早く decode してもその場では
消費されません。したがって、単に split を偏らせるだけでは pipeline 化に
なりません。上側 band を早く消費できる fused decode+undither があって初めて、
top-light split や overlap の意味が出ます。

入力全体を先に scan して `-` の band boundary を集める案は却下しました。
この用途では追加 pass が重く、すでに遅い方向だと分かっています。代わりに、
既存の局所 boundary search を保ったまま、decode band に halo を持たせ、その
overlap の中で undither も終える方向を第一候補にします。

テスト面では、画像破損を防ぐために現新一致を固定します。ただし、これは
fast mode の品質を現行 8-neighbor と完全一致させるという意味ではありません。
reference になる scalar fast mode を作り、parallel/fused の出力がそこに一致
することを 1 case 1 file のテストで守ります。

## 目標

単に現在の full-image post-process を速くするだけではなく、できるだけ
decode 中に有効な処理を済ませ、追加の全画面 pass を減らすことを目標に
します。

実用上の主戦場は 32-80 色です。ただし 16 色も重要です。色数が少ないほど
palette が強いヒントになり、比較的小さい処理量で見た目の改善が大きく
なります。

## 現状

`--direct -d k_undither` は、技術的に不可能な組み合わせではありません。
現在の direct+undither 経路は、まず indexed pixels と palette を得て、
palette-aware な k_undither を実行し、その後 RGB を opaque RGBA へ昇格
させます。これにより palette similarity heuristic を維持しつつ、
`--direct` の RGBA 出力契約も守れます。

ただし、現在の `k_undither` はまだ post-decode step です。

1. `sixel_decoder_decode()` が入力全体を読みます。
2. dequantize が要求されている場合は indexed image として decode します。
3. decode 後に `sixel_dequantize_k_undither()` を実行します。
4. direct 出力が要求されている場合は RGB から RGBA へ昇格します。

no-edge の parallel k_undither は row-band parallel ですが、full indexed
image が完成するまで待ちます。palette similarity cache も全体画像順で
準備し、出力 row を worker に分配して RGB を書きます。つまり decode が
終わった後の別段処理です。

現在の parallel decoder は byte span ベースです。

1. `sixel_decoder_parallel_fill_spans()` が payload を byte 数で分割します。
2. index 0 以外の worker は、自分の開始位置から次の `-` band boundary へ
   局所的に進みます。
3. worker は anchor から実効開始位置まで scan し、row offset と現在の
   color index を復元します。
4. worker-local pixels は worker index 順で global image へ copy されます。
5. `sixel_decoder_parallel_request_start()` は全 worker を join してから
   戻ります。

そのため、上側 band が早く decode できても、現状では pipeline 的には活用
できていません。

## Palette 再定義

parallel decoder は color selection (`#n`) を許しますが、palette 定義または
再定義 (`#n;...`) は worker fast path では扱いません。worker が palette
再定義を見つけると fallback を要求し、parser は serial decode を続けます。

これは提案中の fast path には都合がよいです。parallel decode+undither 側は
stable palette を前提にでき、palette 再定義は conservative な fallback 条件
として維持できます。

## 計算量

filter 自体は pixel に対する 1 次の局所 filter です。pixel work は
O(width * height * neighbors) です。palette similarity lookup は palette size
で bounded であり cache できます。対象の 16-80 色では、漸近的な支配要因
というより、追加の全画面 pass、cache locality、memory traffic、同期の方が
実際の重さになります。

## 品質実験

16-80 色の代表的な Floyd-Steinberg 入力を用意し、MS-SSIM で比較しました。
no-edge の実験 variant は次の通りです。

- 3-neighbor: 2x2 / left, up, up-left。
- 4-neighbor: 2x3 系の causal neighborhood。
- 8-neighbor: 現行の 3x3 full neighborhood。

8-neighbor の実験実装は、sample set 上で `lsqa -d k_undither` と完全一致
しました。

結果:

| sample | colors | plain | 3-neighbor | 4-neighbor | 8-neighbor |
| --- | ---: | ---: | ---: | ---: | ---: |
| snake | 16 | 0.927401 | 0.964161 | 0.969023 | 0.980536 |
| egret | 32 | 0.945571 | 0.973930 | 0.976935 | 0.984588 |
| vimperator | 48 | 0.998130 | 0.998049 | 0.998172 | 0.998519 |
| autumn | 64 | 0.994301 | 0.994004 | 0.994583 | 0.996942 |
| fisheye | 80 | 0.991353 | 0.993413 | 0.994019 | 0.995018 |

filter-only timing では、3-neighbor は 8-neighbor より約 2.32 倍高速、
4-neighbor は 8-neighbor より約 1.82 倍高速でした。

2x bilinear scaling 後の平均値:

| mode | average MS-SSIM |
| --- | ---: |
| plain 2x | 0.953007 |
| 3-neighbor 2x | 0.977896 |
| 4-neighbor 2x | 0.980906 |
| 8-neighbor 2x | 0.987422 |

Cocoa backend では 2x bilinear scaling が入るため、そこで dithering noise が
ある程度隠れる可能性があります。それでも 4-neighbor は速度と品質の妥協点
として強いです。

## Decode Split 実験

`SIXEL_PARALLEL_SKEW` は byte span を偏らせ、前半 worker を小さく、後半
worker を大きくできます。これにより上側 band は早く終わりますが、現在の
join 型 decode では早い結果を消費できません。

`autumn32.six`、`SIXEL_THREADS=4`、`-D` での測定:

| skew | worker 0 decode finish | last copy finish |
| ---: | ---: | ---: |
| 0 | 13.6 ms | 14.9 ms |
| 10 | 12.1 ms | 17.1 ms |
| 20 | 10.1 ms | 18.4 ms |

したがって通常 decode の default skew は balanced のままがよいです。top-light
split は、上側 band を即座に消費できる pipeline がある時だけ意味を持ちます。

入力全体を pre-scan してすべての `-` band boundary を集める案は却下します。
この用途にはすでに遅いことが分かっています。split 調整をする場合も、既存の
局所 boundary search model を維持します。

## 却下または優先度が低い方向

### Full band pre-scan

却下します。scheduling 情報は綺麗になりますが、入力への追加 pass が発生し、
この用途では遅いことが分かっています。

### Separate row-ready undither stage only

有用ではありますが、現在の第一候補ではありません。ready row を halo 付きで
処理する別 stage は作れますが、decode 後にもう一段 pipeline stage を置く形に
近くなります。現在の議論では、undither を decode worker 内へ畳み込む方向を
優先します。

### 全 parallel decode で default skew を入れる

現時点では却下します。上側 band の latency は改善しますが、early output を
消費する consumer がいない場合は total time が悪化します。

## 第一候補の方向

現在の第一候補は、decode-band overlap の中で undither も一緒に行う fused
decode+undither です。

各 worker は body range と、それより広い local decode range を持ちます。

```text
body rows:
  [y0, y1)

local decode rows:
  [y0 - top_halo, y1 + bottom_halo)

local undither input:
  halo rows を含む worker-local indexed buffer

global output:
  filtered body rows [y0, y1) のみ
```

fast 4-neighbor mode:

```text
top_halo = 6 rows
bottom_halo = 0 rows
```

この variant は causal neighbors だけを使うため、次 band を待たずに worker 内で
undither まで完了できます。必要なのは 1 pixel の上隣接ですが、encoded stream
の自然単位が 6-pixel SIXEL band なので、overlap は 1 band、つまり 6 rows に
なります。

exact 寄りの 8-neighbor mode:

```text
top_halo = 6 rows
bottom_halo = 6 rows
```

下方向の neighbor も読めますが、完了が遅れ、overlap decode 量も増えます。
最初の fast path というより、品質比較用または reference 寄りの mode として
扱う方がよいです。

## 実装スケッチ

worker context では body span と decode span を分けます。

- `body_start_offset`, `body_end_offset`: この worker の出力 body に対応する
  byte 範囲。
- `decode_start_offset`, `decode_end_offset`: body に halo band を足した
  byte 範囲。
- `body_row_start`, `body_row_end`: final output へ copy する global rows。
- `decode_row_start`: local row 0 に対応する global row。

worker の流れ:

1. `decode_start_offset` の parser state を復元します。
2. final RGBA へ直接書かず、local indexed buffer へ decode します。
3. palette 再定義、raster reset、unsupported control、unsafe bounds では
   serial fallback します。
4. local rows 上で fast no-edge undither を実行します。
5. body rows だけを global RGB または RGBA output へ copy します。

`--direct -d k_undither` の fused path でも、local decode は indexed value として
行い、palette-aware filter を適用した後に RGBA output へ書きます。

最初に実装するなら 4-neighbor variant がよいです。前 band の halo だけで済み、
「decode 中に undither まで終える」構造に最も合っています。8-neighbor variant
は、その後に品質比較または reference mode として追加します。

## Test Strategy

pixel-exact tests は 1 case 1 file を維持します。既存の exact parallel/scalar
checks は現行 8-neighbor behavior を守ります。新しい fused fast-mode では、
少なくとも次を固定します。

- direct+undither の output shape と alpha。
- palette 再定義 fallback。
- worker boundary が `-` の直後から始まる case。
- fused 4-neighbor output と scalar 4-neighbor reference の一致。
- unsupported token での serial fallback equivalence。

quality tests では MS-SSIM threshold を 0.95 未満に下げない方針を維持します。
fast mode では 16 色と 32 色の sample が特に重要です。palette hint による見た目
の改善が大きいからです。

benchmark は、variant 間で同じ `SIXEL_THREADS` を使って比較します。最低限、
次を含めます。

- `k_undither` なしの通常 decode。
- 現行 post-decode `k_undither`。
- fused 4-neighbor decode+undither。
- optional な fused 8-neighbor decode+undither。

latency analysis では total time と upper-band readiness の両方を記録します。
worker 0 を早く終わらせる split は、fused undither が全 worker join 前にその
結果を消費できる場合にだけ意味があります。

`SIXEL_THREADS=auto` は、実際に何 thread に解決されたかを benchmark log に
残します。parallel 化の baseline は、同じ実効 thread 数で比較しないと意味が
ありません。

## Current Branch State

この branch では、fast 4-neighbor mode を `lso_undither:Vlight` として
明示的に選べるようにし、parallel decoder worker 内で decode-band overlap と
undither を同時に行う fused path を追加中です。

- `sixel_dequantize_k_undither_fast4()` は internal API です。
- 近傍 table を parameter 化し、現行 8-neighbor と fast 4-neighbor が同じ
  no-edge filter backend を使えるようにします。
- fast 4-neighbor の scalar 出力と parallel 出力が byte-for-byte 一致する
  regression test を追加します。
- `sixel_decoder_parallel_request_start()` に optional な undither context を渡し、
  depth 1 の local decode buffer から body rows だけを direct RGB/RGBA output
  へ書きます。
- worker は body band の 1 つ前の SIXEL band を top halo として局所 decode
  します。fast4 は causal neighborhood だけを使うため bottom halo は持ちません。
- palette 再定義などで parallel decode が失敗した場合は、従来通り serial raw
  decode に戻り、その後 `sixel_dequantize_k_undither_fast4()` を実行します。
- `k_undither` の既存 8-neighbor 挙動は変更しません。

レビューで見えた注意点:

- parallel decode の開始時点で `image->ncolors` だけを見て palette size を
  固定すると、worker 内で初めて出る `#Pc` selection により、実際に使う色が
  `ncolors` の外へ出ることがあります。
- 逆に `SIXEL_PALETTE_MAX_DECODER` をそのまま dequantize の `ncolors` として
  渡すのは避けます。similarity cache は palette size に対して重く、32-80 色
  が主戦場の今回の fast path では、全 65536 色扱いにすると fused path の利点を
  潰します。
- 正しい方向は、palette storage 自体は広く持ちつつ、worker が観測した最大
  color index と初期 `image->ncolors` から active color count を決めることです。
  ただし default palette 参照では 256 色までは暗黙に有効なので、active count
  には 256 色 floor を置きます。その active count を scalar reference と fused
  worker の両方で揃えます。
- 256 を超える高い color index は active count を引き上げるため、Vlight でも
  similarity cache の `ncolors * ncolors` cost は残ります。32-80 色を主戦場に
  する限り問題は小さいですが、公開 option としての残リスクです。根本対応は
  active palette remap または similarity cache 構造の変更です。
- `-d` は `-Qk:Ia` と同じ suboption schema parser に載せます。実装上も
  `src/options.c` の `sixel_option_parse_dequantize_argument()` に schema と
  解決処理を集約し、decoder と `lsqa` は同じ helper を呼びます。これにより
  `lso_undither:Vlight` は `l:Vl`、`lso_undither:Vfs` は `l:Vf` と短縮
  できます。
- ambiguous prefix については、既存ユーザーが使っていた `-dk_` を
  `k_undither` として残します。

ここまでの検証:

```sh
make -C src -j4 libsixel.la
make -C tests -j4 test_runner
make -C tests \
  TESTS='processing/decoder/0014_decoder_kundither_fast4_parallel_matches_scalar.t' \
  check
make -C tests \
  TESTS="processing/decoder/0013_decoder_kundither_parallel_matches_scalar.t \
processing/decoder/0014_decoder_kundither_fast4_parallel_matches_scalar.t" \
  check
make -C tests \
  TESTS="processing/decoder/0007_decoder_ormode_parallel_request.t \
processing/decoder/0013_decoder_kundither_parallel_matches_scalar.t \
processing/decoder/0014_decoder_kundither_fast4_parallel_matches_scalar.t \
processing/decoder/0015_decoder_kundither_fast4_fused_matches_scalar.t \
cli/core/0016_basic_dequantize_vlight_alias.t \
cli/core/0022_basic_dequantize_legacy_k_prefix.t \
cli/core/0023_basic_dequantize_vlight_threads_quality.t \
cli/core/0024_basic_dequantize_vfs_alias.t \
cli/core/0025_basic_lsqa_dequantize_vlight_alias.t \
cli/core/0012_basic_direct_with_dequantize.t" \
  check
shellcheck -x -P "$PWD" \
  tests/processing/decoder/0014_decoder_kundither_fast4_parallel_matches_scalar.t \
  tests/processing/decoder/0015_decoder_kundither_fast4_fused_matches_scalar.t \
  tests/cli/core/0016_basic_dequantize_vlight_alias.t \
  tests/cli/core/0022_basic_dequantize_legacy_k_prefix.t \
  tests/cli/core/0023_basic_dequantize_vlight_threads_quality.t \
  tests/cli/core/0024_basic_dequantize_vfs_alias.t \
  tests/cli/core/0025_basic_lsqa_dequantize_vlight_alias.t
git diff --check -- \
  src/decoder.c \
  src/decoder.h \
  src/decoder-parallel.c \
  src/decoder-parallel.h \
  src/fromsixel.c \
  src/sixel_decode_pixels.h \
  tests/Makefile.am \
  tests/Makefile.in \
  tests/test_runner.c \
  tests/processing/decoder/0014_decoder_kundither_fast4_parallel_matches_scalar.c \
  tests/processing/decoder/0014_decoder_kundither_fast4_parallel_matches_scalar.t \
  tests/processing/decoder/0015_decoder_kundither_fast4_fused_matches_scalar.c \
  tests/processing/decoder/0015_decoder_kundither_fast4_fused_matches_scalar.t
```

Autotools は `$TOP_SRCDIR/.local/bin` を優先して使います。今回 `automake` は
`m4` 側で長時間止まったため、`tests/Makefile.in` は最小差分で手動同期し、
`./config.status tests/Makefile` で generated Makefile を更新しました。

## Implementation Benchmark

`SIXEL_THREADS=4`、`sixel2png -D`、hyperfine 7 runs / warmup 2 で測定しました。
入力は `img2sixel -d fs -p <colors>` で作った一時 SIXEL です。PNG 出力まで
含む end-to-end time なので、decode filter 単体より保守的な比較です。

| sample | size | colors | direct | lso_undither:Vfs | lso_undither:Vlight |
| --- | ---: | ---: | ---: | ---: | ---: |
| snake | 600x450 | 16 | 78.1 ms | 112.6 ms | 94.7 ms |
| egret | 600x450 | 32 | 77.9 ms | 116.2 ms | 94.5 ms |
| vimperator | 582x746 | 48 | 54.6 ms | 63.5 ms | 57.9 ms |
| autumn | 2912x1464 | 64 | 626.8 ms | 1.828 s | 1.494 s |
| fisheye | 2048x1720 | 80 | 463.3 ms | 1.056 s | 851.2 ms |

| sample | Vfs / direct | Vlight / direct | Vlight improvement vs Vfs |
| --- | ---: | ---: | ---: |
| snake | 144% | 121% | 16% |
| egret | 149% | 121% | 19% |
| vimperator | 116% | 106% | 9% |
| autumn | 292% | 238% | 18% |
| fisheye | 228% | 184% | 19% |

Vlight は小中規模では direct decode にかなり近づきます。一方、大きい画像では
worker ごとの local RGB 生成と per-worker similarity setup がまだ重く、通常
decode との差は大きく残っています。次の高速化候補は local buffer flatten と
RGB temporary allocation の削減、similarity cache の共有または事前固定化、
大画像向けの row copy/undither 統合です。

## 未決事項

- fast fused path を起動時オプションまたは設定値で選ぶのか、mode switch として
  見せるのか。
- 4-neighbor を 32 色以下など小 palette だけの default にするか。
- `SIXEL_PARALLEL_SKEW` を手動 tuning のままにするか、fused decode+undither が
  active な時だけ internal heuristic にするか。
- 既存ユーザーにとって `k_undither` の挙動が驚きにならないよう、品質/速度の
  選択肢をどう見せるか。
- `SIXEL_THREADS=auto` の実効 thread 数をどこで user-visible にするか。
- 先行研究調査は未完了です。error diffusion inverse filtering、
  palette-aware de-dithering、causal neighborhood reconstruction あたりの
  キーワードで確認します。

# R Chromium (x86)

[English](README.md) | **日本語** | [Italiano](README.it.md) | [한국어](README.ko.md)

32 ビット Haiku (i386) 向けの Chromium ベースのウェブブラウザです。Qt を使わず、
Haiku 自身のウィンドウシステム上で動作します。JavaScript を含む最新のサイト
(google.com、news.naver.com、news.google.co.kr) を描画し、アイコンのみの
戻る / 進む / 再読み込みボタンとアドレス欄を備えたネイティブツールバーを持ち、
日付ごとにまとめられた検索可能なブックマークを保存し、青い Chromium アイコンで
デスクトップにインストールされます。

このファイルはエンドユーザー向けのインストールガイドです。ソースからのビルド、
移植メモ、その他開発者向けの内容は [`AGENTS.md`](AGENTS.md) にあります。

![VAIO P 上の R Chromium で描画された韓国語版 Wikipedia](docs/screenshots/x86-wikipedia.png)

![日付ごとにまとめられたブックマークウィンドウ](docs/screenshots/x86-bookmarks.png)


## 必要条件

- 32 ビット x86 の Haiku (Sony VAIO P でテスト済み: Intel Atom Z520、RAM 2 GB)。
- Haiku 標準フォント (`/boot/system/data/fonts` 以下の `NotoSans*` と
  `NotoSansCJKjp-VF.otf`)。ハングルは CJK フォントで描画されます。
- ビルド済みの R Chromium (`content_shell` バイナリと、同じ場所にある
  `content_shell.pak`、`icudtl.dat`、`locales/`)。このリポジトリのビルドツリーが
  あればすでに含まれています。ない場合は [`AGENTS.md`](AGENTS.md) の説明に
  従ってビルドしてください。

## インストール

Haiku マシン上で、このリポジトリのチェックアウトからワンショットインストーラを
実行します:

```sh
sh install.sh
```

これだけです。Haiku に欠けている fontconfig ファイルを用意し、ブラウザの
バイナリを検証 (このマシンのリンカが壊した場合は修復) し、R Chromium を
`/boot/home/RChromium/` にコピーし、青い Chromium アイコン付きの
**R Chromium** ランチャーをデスクトップに置きます。

ビルドが既定以外の場所にある場合は、そのディレクトリを渡してください:

```sh
sh install.sh /path/to/dir/with/content_shell
```

(ソースからそのビルドを作る作業は別の、はるかに長い作業です --
[`AGENTS.md`](AGENTS.md) を参照。インストーラはビルド済みバイナリを
インストールします。)

## 実行

デスクトップの **R Chromium** をダブルクリックします。Google が開きます。
上部の欄にアドレスを入力して Enter を押してください -- `news.naver.com` のように
ホスト名だけ入力すると `https://news.naver.com/` になります。

シェルから:

```sh
"/boot/home/Desktop/R Chromium" https://news.naver.com/
```

## 使い方

- **戻る / 進む / 再読み込み** は左側の 3 つのアイコンボタンです。ページの
  読み込み中は再読み込みボタンが停止ボタンに変わります。
- **ブックマーク**: 星 (★) で現在のページをブックマークし、一覧 (≡) で
  ブックマークウィンドウを開きます。日付ごと (今日、昨日、それ以降は日付) に
  まとめられ、検索欄に入力するとタイトルと URL で即時に絞り込まれます。項目を
  ダブルクリックすると開きます。同じページを再度ブックマークすると、重複せず
  「今日」に移動します。ブックマークはプレーンテキストファイル
  `~/config/settings/RChromium/bookmarks` に、1 件につき 1 行
  `<unix 秒> <url> <タイトル>` (タブ区切り) で保存されるため、再インストール後も
  残り、手で編集やバックアップができます。
- **ウィンドウ**: ウィンドウの角をドラッグしてサイズを変えると、ページが新しい
  サイズに合わせて再レイアウトされます。新しいウィンドウを開くリンクは、現在の
  ウィンドウからずらした位置に別の R Chromium ウィンドウとして開きます。
  ウィンドウを閉じるとそのページが閉じ、最後のウィンドウを閉じると R Chromium
  が終了します。

## 既知の制限

- **ウェブストレージは起動間で保持されません。** Cookie、localStorage、
  サイトのログインは 1 セッションのみ有効です。これは意図的な動作です
  (ブラウザはストレージをメモリ上で扱います)。ブックマークは影響を受けません。
- **重いページは Atom では遅いです。** news.naver.com は 1.5-2.5 秒、
  news.google.co.kr は JavaScript が 1.33 GHz コアで CPU 律速となるため
  6-8 秒かかります。ハードウェアの限界であり、バグではありません。
- ハードウェアアクセラレーションはありません。すべてソフトウェア描画です
  (Haiku には Chromium が使える GL がありません)。ランチャーが `--disable-gpu`
  を渡しているのはそのためです。
- これは Chromium 87 の非公式移植です。Chromium のスケジュールに沿った上流の
  セキュリティ更新は受けません。重要なアカウントには使わないでください。

## トラブルシューティング

- **インストーラが "embedded blob verification FAILED -- not installing" と
  表示する。** 指定したバイナリは壊れたリンク出力です (このマシンのリンカは
  V8 の一部を壊すことがあります)。検証済みビルドからインストールしてください --
  `/boot/home/content_shell.last-good` は常に検証済みです -- または検証付き
  リンクスクリプトで再ビルドしてください (`AGENTS.md`)。
- **文字が表示されない / ページに文字が出るとすぐブラウザが終了する。**
  `/boot/home/rchromium-fonts.conf` がないか読めません。インストーラを再実行
  するか、`assets/rchromium-fonts.conf` を手でそこにコピーしてください。
- **起動直後にページが空白のまま。** デスクトップのランチャー、またはランチャーの
  フラグ付きで起動したか確認してください。特に `--disable-gpu-compositing` は
  このバックエンドで必須です。
- **Qt 非依存の確認:** `readelf -d /boot/home/RChromium/content_shell | grep NEEDED`
  の出力に `libbe.so` などが並び、`libQt5*` がないことを確認します。

## AI に関する開示

このプログラムは Claude と共に書かれました。

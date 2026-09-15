# R Chromium (x86)

**English** | [日本語](README.ja.md) | [Italiano](README.it.md) | [한국어](README.ko.md)

A Chromium-based web browser for 32-bit Haiku (i386), built on Haiku's own
window system with no Qt. It renders modern sites with JavaScript (google.com,
news.naver.com, news.google.co.kr), has a native toolbar with icon-only
Back / Forward / Reload buttons and an address field, keeps date-grouped,
searchable bookmarks, and installs on the Desktop with the blue Chromium icon.

This file is the end-user install guide. Building from source, porting notes
and everything else for developers live in [`AGENTS.md`](AGENTS.md).

![Korean Wikipedia rendered in R Chromium on the VAIO P](docs/screenshots/x86-wikipedia.png)

![The Bookmarks window, grouped by date](docs/screenshots/x86-bookmarks.png)

## Requirements

- Haiku on 32-bit x86 (tested on a Sony VAIO P: Intel Atom Z520, 2 GB RAM).
- The standard Haiku fonts (`NotoSans*` and `NotoSansCJKjp-VF.otf` under
  `/boot/system/data/fonts`). Hangul renders through the CJK face.
- A built R Chromium (the `content_shell` binary with `content_shell.pak`,
  `icudtl.dat` and `locales/` beside it). If you have this repository's build
  tree it is already there; otherwise build it as described in
  [`AGENTS.md`](AGENTS.md).

## Install

On the Haiku machine, from a checkout of this repository, run the one-shot
installer:

```sh
sh install.sh
```

That is all. It provisions the fontconfig file Haiku lacks, verifies the
browser binary (repairing it if this machine's linker damaged it), copies
R Chromium into `/boot/home/RChromium/`, and puts the **R Chromium** launcher
with the blue Chromium icon on your Desktop.

If your build is somewhere other than the default, pass its directory:

```sh
sh install.sh /path/to/dir/with/content_shell
```

(Producing that build from source is a separate, much longer job -- see
[`AGENTS.md`](AGENTS.md). The installer installs an already-built binary.)

## Run

Double-click **R Chromium** on the Desktop. It opens Google; type an address
in the field at the top and press Enter -- a bare host like `news.naver.com`
becomes `https://news.naver.com/`.

From a shell:

```sh
"/boot/home/Desktop/R Chromium" https://news.naver.com/
```

## Using it

- **Back / Forward / Reload** are the three icon buttons on the left. The
  Reload button turns into Stop while a page is loading.
- **Bookmarks**: the star (★) bookmarks the current page; the list (≡) opens
  the Bookmarks window, grouped by date (Today, Yesterday, then dates) with a
  search field that filters by title and URL as you type. Double-click an
  entry to open it. Bookmarking a page again moves it to Today instead of
  duplicating it. They are stored in the plain text file
  `~/config/settings/RChromium/bookmarks`, one `<unix seconds> <url> <title>`
  line per bookmark (tab-separated), so they survive reinstalls and can be
  edited or backed up by hand.
- **Windows**: drag the resize corner and the page re-lays out to the new
  size. A link that opens a new window gets its own R Chromium window, offset
  from the current one. Closing a window closes that page; closing the last
  window quits R Chromium.

## Known limits

- **Web storage does not persist between launches.** Cookies, localStorage
  and site logins last for one session only. This is deliberate (the browser
  runs its storage in memory); bookmarks are unaffected.
- **Heavy pages are slow on the Atom.** news.naver.com loads in 1.5-2.5 s;
  news.google.co.kr takes 6-8 s because its JavaScript is CPU-bound on the
  1.33 GHz core. That is the hardware, not a bug.
- No hardware acceleration: everything is software-rendered (Haiku has no GL
  for Chromium to use), which is why the launcher passes `--disable-gpu`.
- This is an unofficial port of Chromium 87. It does not receive upstream
  security updates on Chromium's schedule; do not use it for sensitive
  accounts.

## Troubleshooting

- **The installer says "embedded blob verification FAILED -- not installing".**
  The binary you pointed it at is a broken link output (this machine's linker
  can corrupt part of V8). Install from a verified build instead --
  `/boot/home/content_shell.last-good` is always one -- or rebuild with the
  verified link script (`AGENTS.md`).
- **Text does not appear / the browser exits as soon as a page shows text.**
  `/boot/home/rchromium-fonts.conf` is missing or unreadable. Re-run the
  installer, or copy `assets/rchromium-fonts.conf` there by hand.
- **The page stays blank right after launch.** Make sure you started it via
  the Desktop launcher or with the launcher's flags; `--disable-gpu-compositing`
  in particular is required on this backend.
- **Confirm it is Qt-free:** `readelf -d /boot/home/RChromium/content_shell | grep NEEDED`
  lists `libbe.so` and friends and no `libQt5*`.

## AI disclosure

This program was written with Claude.

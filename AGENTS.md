# R Chromium Native Port — Agent Handoff

## Non-negotiable goal

Complete an independent Chromium-based browser port for the physical Haiku
32-bit i386 machine without Qt. The final application must use Haiku APIs
directly, display Google in a real Haiku window, provide icon-only
Back/Forward/Reload controls, an address field, and date-grouped searchable
bookmarks, then be installed on the Haiku Desktop with the blue Chromium icon.

- Do not push or commit anything to the Haiku source tree.
- Keep all port changes in this repository as an attachable overlay and
  ordered patch series.
- Do not use QEMU for R Chromium build or runtime verification. Build and test
  on the physical `user@haiku` x86 machine.
- Do not claim completion from the probe or from GN success. Completion needs
  a running browser, rendered Google page, final UI, Desktop install, and
  runtime/link evidence that Qt is absent.

## Source provenance and Qt boundary

The current bootstrap source is the Chromium 87 snapshot distributed by the
QtWebEngine project because the existing HaikuPorts Chromium OS patchset is
based on that exact snapshot. This is source provenance only: R Chromium is
built directly as Chromium `content_shell` with `use_qt=false`, a custom
`OzonePlatformHaiku`, and no Qt headers or libraries.

Pinned inputs:

- qtwebengine-chromium base commit:
  `4d8433e345aa23bfb55be66f0aa13656ee92fa38`
- HaikuPorts commit:
  `3fb92032fd276268f38665ddfff1eddc670419fa`
- HaikuPorts patch SHA-256:
  `8ddfb1575779a8cd3162eb5a571764d414a01b1a042ae16f4d96e8a6177803bb`
- transferred patched source archive SHA-256:
  `605551dde5cc3eb96c5a18c6ee3fc0a8fe23eac9a409332900b38f0d5160315f`
- exact Chromium engine revision: `38a74c624ca48a6acb2a6f427998be599b504eed`
  (`87.0.4280.144`)
- official Content Shell supplement archive SHA-256:
  `cc10261322147315393991af1940e93af4a01999b0f8687772c3e9c0bc52e710`

Run `make -C /boot/home/rchromium-native verify-no-qt` on Haiku after every
overlay update. At final handoff, also inspect the executable with Haiku's
runtime dependency tool and reject any Qt dependency.

## Project purpose and limits

The purpose of this project is to run Chromium on a **32-bit Haiku (i386)
machine with the best performance that hardware allows**. On old, low-end x86
hardware that means removing the Qt layer and wiring Chromium directly to the
Haiku API, and that goal takes priority over everything else.

This is not, and does not try to be, a general or official Chromium port for
Haiku as a whole. Other architectures, a wide range of hardware, every Haiku
release, and full Chromium feature compatibility are out of scope; features or
portability may be sacrificed for i386 performance.

It is also a small, unofficial port, so tracking Chromium upstream and applying
its security fixes may lag or lapse. Chromium ships security fixes quickly; a
build without the latest upstream security patches must not be considered safe
for general browsing or for sensitive accounts. Anyone using a build must check
its base revision and the date of its last security update.

The production boundary is strict: code under `probe/` and `chromium_overlay/`
may use Chromium and Haiku APIs but must not include or link Qt.
`make verify-no-qt` enforces that rule.

## Status (2026-09-14)

The x86 port is a working browser: Chromium 87 `content_shell` on a native
Haiku Ozone backend (no Qt) with a BeAPI toolbar. Verified on the VAIO P
(Atom Z520, 2 GB): google.com, news.naver.com and news.google.co.kr render
with JavaScript, including via address-bar re-navigation; icon-only
Back/Forward/Reload; address field (bare hosts default to https://);
date-grouped, searchable bookmarks; Desktop launcher with the blue Chromium
icon; `readelf -d` lists libbe and friends, no libQt5*. 2026-09-15: window
resize relayouts the page, links that open a new window get their own BWindow
(one WindowTreeHost per Shell; viz gets the real widget via patch 0089), and
closing a window closes its Shell. Details, root causes
and every fix are in `docs/browser-ui-plan.md`. Known limits: web storage is
session-only (off-the-record, by design -- see the docs), and Google News
takes 6-8 s on the Atom (renderer CPU-bound; naver loads in 1.5-2.5 s).

2026-09-20: `https://x.com/i/flow/login` renders too -- the X logo, the
phone/Google/Apple buttons and the email field, from the binary installed by
`install.sh` (`docs/screenshots/x86-x-com-login.png`). Getting there took a
thread-stack fix (patch 0090), a relink through
`scripts/linkretry-verified.sh`, and restoring
`/boot/home/rchromium-fonts.conf`; the section below has the whole story.
Web fonts still do not load -- `remote_font_face_source.cc(356)` and
`computed_style.cc(1683)` `NOTREACHED()` both say
`GetLastResortFallbackFont()` returned null -- so pages render in Noto rather
than their own typeface. Adding Arial/Times/Courier aliases to the fontconfig
file does not change it; the cause is further in, and this is the next thing
to look at.

## Physical Haiku machine

- SSH: `user@haiku`. **Never issue `shutdown` or `reboot` from an attached SSH
  session** — it panics the machine in sshd teardown instead of stopping
  cleanly, which endangers the build output. See the 2026-08-18 checkpoint.
- System observed: Haiku R1 beta6, `BePC x86`, `x86_gcc2` hybrid
- CPU observed: one Intel Atom Z520 core at 1.33 GHz. `sysinfo -cpu` reported a
  single CPU until 2026-08-18 and now reports two, with `HTT` in the feature
  list: this is still one physical core, exposed as two logical CPUs. Chromium
  Ninja runs at `-j2` since that change; the earlier blanket `-j1` rule and its
  SSH-unresponsiveness rationale were measured again and reversed, see the
  2026-08-18 checkpoint.
- Chromium source: `/boot/home/rchromium-chromium87-fast`
- Chromium root: `/boot/home/rchromium-chromium87-fast/chromium`
- Overlay: `/boot/home/rchromium-native`
- Generated output: `chromium/out/rchromium_native`
- Native GN bootstrap: `/boot/home/rchromium-gn-build/gn/out/gn`
- Copied GN used by Chromium: `/boot/home/rchromium-chromium87-fast/gn/out/gn`

Do not use an output directory containing the word `haiku`; Chromium 87 has a
source-assignment assertion that rejects platform names in output paths.

Installed build prerequisites added during bring-up:

- `nss_x86_devel-3.126-1`
- `nspr_x86_devel-4.39-1`
- `/boot/home/config/non-packaged/bin/python` symlink to
  `/boot/system/bin/python3`

For x86 pkg-config discovery export:

```sh
PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig
export PKG_CONFIG_PATH
```


## Where the build stands

`content_shell` for Haiku x86_gcc2, `use_qt=false`, building on the VAIO P
itself. Ask the machine rather than this file -- anything written here about
progress is stale by the time it is read:

```sh
ssh user@haiku 'sh /boot/home/rchromium-native/memcheck.sh'
```

That prints liveness, supervision and pressure in that order. The only authority
on progress is `.ninja_log` in the output directory: its **line count** (edges
ever recorded, monotonic) and its **mtime**. The `[n/m]` counter in the text log
restarts from 1 on every invocation and the text log is truncated on every
watchdog restart, so neither is comparable across runs.

The build is supervised by `scripts/watchdog_native_content_shell87.sh`, which
restarts the pipeline on its own. Read `watchdog.state` and `watchdog.log`
before touching anything; if the watchdog is alive, let it do the restarting.

## Build configuration

- Output directory `out/rchromium_native`; the pinned Chromium 87 tree lives in
  `/boot/home/rchromium-chromium87-fast/chromium`.
- Key GN arguments: `use_qt = false`, `use_jumbo_build = true`,
  `toolkit_views = false`, `use_kerberos = false`,
  `rchromium_target = "//content/shell:content_shell"`.
- The Haiku x86 toolchain must be the **secondary** ABI -- `gcc-x86`, `g++-x86`,
  `ar-x86`, `nm-x86`, `readelf-x86`. Plain `gcc` is the GCC2 primary ABI and
  fails with "cannot specify -o with -c or -S and multiple compilations".
- `scripts/lowmem-toolchain/{gcc,g++}-x86` wrap those with GCC garbage-collector
  parameters; without them `-j2` cannot be sustained.
- Patches live in `chromium87_overlay/patches/` -- 67 of them, plus 8 in
  `chromium87_overlay/patches-upstream/` against the vendored Content Shell
  sources, applied by
  `scripts/attach_chromium87_overlay.sh`. The directory is the inventory; do not
  duplicate the list here.
- `gperf` is a build prerequisite and is not installed by default.
- `enable_plugins = false` and `enable_pdf = false` (added 2026-08-30, together
  -- setting the first alone fails `gn gen` with `//chrome/common/BUILD.gn:22:
  assert(enable_plugins, "PDF support requires plugins be enabled.")`, because
  `gn gen` evaluates the whole `//` graph regardless of which target ninja is
  later asked to build, and `chrome/` is part of that graph here). PPAPI's font
  matcher pulls in `//third_party/fontconfig`, which is not installed and
  irrelevant to rendering a page -- PPAPI is the Pepper plugin API (Flash-era),
  not needed here, and neither is PDF viewing. `args.gn` itself is not tracked
  in this repo; it lives only at `out/rchromium_native/args.gn` on the machine,
  so this line is the record of it. Appending to `args.gn` after it already
  exists changes ninja's build graph the same way any GN arg change does --
  `gn gen` reruns and every target gated on the new value's default is added or
  dropped -- but it does not touch compiler command strings for targets
  unaffected by the flags, so the objects already built for those stay valid.

## Building and linking on the VAIO

The end-user README only installs a finished build. Producing that build on
the machine itself:

### One-time machine setup

```sh
# helper scripts the link pipeline and the installer call by absolute path
cp scripts/verify_embedded_blob.py scripts/repair_embedded_blob.py \
   scripts/linkretry-verified.sh scripts/install_to_desktop.sh /boot/home/
# fontconfig has no /etc/fonts on Haiku; Blink needs this or it aborts on the
# first text it lays out (font_cache.cc). The installer also copies it if absent.
cp assets/rchromium-fonts.conf /boot/home/rchromium-fonts.conf
# the low-memory g++ wrapper the link goes through (see the file's comments)
mkdir -p /boot/home/config/non-packaged/bin-lowmem
cp scripts/lowmem-toolchain/g++-x86 /boot/home/config/non-packaged/bin-lowmem/
```

Fonts: the system `NotoSans*` and `NotoSansCJKjp-VF.otf` under
`/boot/system/data/fonts/{ttfonts,otfonts}` are what `rchromium-fonts.conf`
lists; Hangul renders through the CJK face.

### Link (on a freshly booted machine)

```sh
sh /boot/home/linkretry-verified.sh        # log: /boot/home/linkretry-verified.log
```

Why this and not plain `ninja`: this machine's GNU ld 2.17 needs
`--no-keep-memory --reduce-memory-overheads` to fit the 188 MB link in 2 GB at
all, and with those flags it silently overwrites the interior of V8's 1 MB
embedded-builtins blob with stale bytes from earlier inputs. The result renders
static pages and takes SIGILL on the first JavaScript. The blob has no
relocations, so the script repairs it by copying `embedded.o`'s `.text` back,
then byte-verifies it against the object and only then keeps the binary as
`/boot/home/content_shell.last-good`. Link right after a reboot: a heavy
recompile beforehand leaves ld no memory and it is killed at ~141 MB of output.


Then install with `install_to_desktop.sh` as described in `README.md`. The
overlay attach / Chromium 87 bootstrap / bring-up order that produce the
objects the link consumes are the sections below.

## Attaching to a synced Chromium checkout

```sh
./scripts/attach_overlay.sh /absolute/path/to/chromium/src
gn gen out/haiku --args='use_ozone=true ozone_platform="haiku" ozone_auto_platforms=false ozone_extra_path="//haiku_port/ozone_extra.gni"'
```

The attachment is an untracked symbolic link. Updating Chromium remains a
normal `gclient sync`; no change is made to Haiku itself and no upstream push
is performed.

## Chromium 87 bootstrap

The first engine bring-up uses the Chromium 87 source already ported by
HaikuPorts. Both inputs are pinned and checksum-verified; this avoids silently
following a moving patch URL:

- qtwebengine-chromium commit `4d8433e345aa23bfb55be66f0aa13656ee92fa38`
- HaikuPorts commit `3fb92032fd276268f38665ddfff1eddc670419fa`

QtWebEngine's archive omits the Content Shell implementation directories even
though its `BUILD.gn` still references them. The overlay therefore vendors the
82 unmodified files from Chromium `87.0.4280.144` revision
`38a74c624ca48a6acb2a6f427998be599b504eed` under
`chromium87_overlay/upstream/content_shell`. The original archive checksum and
URL are recorded in `chromium87_overlay/upstream/README.md`; the attach script
links the missing directories into the checkout without copying them into the
upstream source tree.

`scripts/prepare_chromium87.sh` applies the existing OS, process, threading,
V8, networking and toolchain work. `scripts/build_content_shell87.sh` first
builds a Qt-free, headless `content_shell`. That target isolates engine/build
failures from the new native window backend. It is an intermediate diagnostic
binary, not the final browser.

After the headless engine succeeds, `scripts/attach_chromium87_overlay.sh`
attaches the native backend and `scripts/build_native_content_shell87.sh`
builds it. The initial native compositor is intentionally software-only and
must be launched with `--single-process --disable-gpu --in-process-gpu
--disable-gpu-compositing` (the run script and the Desktop launcher carry
them); Skia raster pixels are copied directly into a Haiku `BBitmap` and
presented by `BView`.

After the binary exists, launch the first Google render with:

```sh
./scripts/run_native_content_shell87.sh \
  /boot/home/rchromium-chromium87-fast https://www.google.com/
```

## Bring-up order

1. Port Chromium `base` process, thread, file, time and message-pump primitives
   to `OS_HAIKU` for both Haiku ABIs.
2. Implement `PlatformWindow` with `BWindow`, translating mouse, keyboard,
   focus, move and resize events.
3. Implement `SurfaceOzoneCanvas` with a software `BBitmap` swap path.
4. Add screen, clipboard, cursor and IME implementations.
5. Start `content_shell`, then enable the minimal R Chromium browser UI.
6. Add Mesa/EGL acceleration only after the software path is stable.

The overlay intentionally does not pretend to be complete: Chromium `main`
does not currently recognize Haiku as a target OS, so Ozone alone cannot be
linked until the `base` and build-config port in step 1 exists.

## Relationship to the arm64 port

This is the x86 (Chromium 87) port. There is a sibling arm64 (Chromium 154)
port at `../rchromium-native-arm64/`. Each platform carries its **own** native
BeAPI toolbar implementation -- they cannot share source verbatim because
content_shell's Shell/aura APIs differ between 87 and 154. What they share is
the design: a widget-keyed bridge so content_shell never names a BeAPI type
(all BeAPI-derived classes live in an RTTI-compiled `beapi_views` target),
an explicit `platform_->aura->ShowWindow()` from the shell delegate, and the
same delegate-hook mapping (address bar, load state, title, bookmarks).

Both toolbars deliver the same feature set: icon-only Back/Forward/Reload, an
address field, date-grouped searchable bookmarks, and a Desktop launcher with
the blue Chromium icon. If you are about to add a toolbar here, it already
exists (`chromium87_overlay/ozone/haiku_beapi_views.cc`,
`haiku_browser_chrome.h`) -- do not duplicate it.

The x86-specific rendering-reliability work (fixes and diagnosis) is in
`docs/browser-ui-plan.md`.
## Rules that cost something to learn

- **Never run a system update** (`pkgman update`) while the port is unfinished.
  One did, and recovering the packages took the rest of a day. Targeted local
  file installs are fine, checked before and after.
- **Never run a second ninja** in the same output directory. Two writers corrupt
  the build state. Check `ps` in a *separate* call whose result decides the next
  one -- never `ps ...; start ...` in one command, because the answer arrives
  after the damage.
- **Never `shutdown -r` from an attached SSH session** on this machine: it
  reliably panics the kernel. Reboots are done at the machine.
- **`-Wno-error` rather than fixing warnings one at a time.** Every
  content_shell failure so far has been a GCC 13.3 diagnostic promoted by
  `-Werror`, never a real defect, and each fix in the global config changes every
  compile command and rebuilds the whole tree: two such patches cost a run that
  had reached edge 1233 and about 19 hours. Flag order was verified on a real
  command line so the override genuinely takes effect. Warnings still print;
  real errors still stop the build.
- **A clean compile log is not evidence that anything works.** It says the
  compiler was satisfied, nothing more.

## Documentation discipline

Write a section here when something was **learned** -- a mechanism understood, a
measurement taken, a rule that cost time to discover. Give it a title that
states the finding, so a reader can tell from the heading whether they need it.

Do not write status checkpoints. Twenty of them accumulated between 2026-08-13
and 2026-08-20, 1,815 lines describing states that no longer hold -- which `-j`
was in force that evening, which PID was running, which phase had just started.
They were written for a handoff and were obsolete within a day, and they buried
the sections that still mattered. Progress belongs to `.ninja_log` and
`memcheck.sh`; this file is for what stays true.

Keep `README.md` user-facing.

## Oversized jumbo translation units wedge the kernel, they do not merely stall (2026-08-21)

The earlier read of this failure -- "jumbo TU size wedges the machine" -- was
right about the trigger and wrong about the mechanism, and the difference decides
how to detect it. Measured this time, on `blink/renderer/platform`:

| unit | files merged | outcome |
| --- | --- | --- |
| `platform_jumbo_2.o` | 10 | finished in 1.8 min, 138 KB |
| `platform_jumbo_1.o` | 78 | **0 bytes after 3h36m** |
| `platform_jumbo_3.o` | 50 | **0 bytes after 3h36m** |

Both stuck units had `g++-x86` alive with a `cc1plus` and an `as` child (`-pipe`,
which is why the `.o` exists at zero length from the start -- `as` creates it and
waits on the pipe). What makes this a wedge and not slowness:

- `top` showed **no compiler thread at any CPU percentage** -- the busiest thread
  on the machine was Deskbar at 0.3%. The processes were blocked, not computing.
- Memory was not the constraint: 1.65 GB of 2.14 GB free, swap 2.11 GB of
  2.14 GB free, `needed memory: 0`. `listarea` on the `cc1plus` team showed an
  ordinary address space, nothing near 32-bit exhaustion.
- **`kill -9` did not remove them.** `ninja` died to SIGKILL; the two `g++-x86`
  drivers and their four `cc1plus`/`as` children survived it. Uninterruptible in
  the kernel.
- Afterwards every write to `/boot` blocked forever -- `cp`, `echo >`, even
  `find -delete`. Read-only work still answered instantly from cache (`date`,
  `ps`, `tail`, `listarea`) and the box still replied to ping. So the machine
  looks alive to every cheap probe while being unable to make progress.

Detection rules that follow from this, since the cheap probes all lie:

1. Do not conclude "still compiling" from live `g++-x86`/`cc1plus` processes.
   Check `top` for actual CPU, and check whether the target `.o` is still zero
   length. A zero-length `.o` whose mtime is hours old is the signature.
2. Do not conclude "no compiler running" from `ps | grep cc1plus`. Haiku's `ps`
   truncates the command column at about 63 characters and the real path
   (`/boot/system/develop/tools/x86/bin/../lib/gcc/i586-pc-haiku/13.`) fills it
   completely, so `cc1plus` never appears. Grep `i586-pc-haiku` instead. This
   mistake has now been made twice in this port.
3. The watchdog cannot recover this. It restarts only when nothing is running,
   and here everything is running -- forever. A wedge of this kind needs a human
   or an external timeout, not the watchdog.

The fix is to stop generating units this large. `build/config/jumbo.gni` sets
`jumbo_file_merge_default = 50` and uses `jumbo_file_merge_goma = 8` when goma is
enabled, so 8 is a limit upstream already considers reasonable; the 10-file unit
above compiled in under two minutes. Adding `jumbo_file_merge_limit = 8` to
`args.gn` regroups every jumbo target. That invalidates the 86 good jumbo objects
built so far (88 existed, 2 were the zero-length stuck pair), which is a bounded
and acceptable cost -- and small units also mean a panic or wedge costs one short
compile instead of four hours.

That change could not be applied while the machine was in this state, because
writing `args.gn` is a write. It has to be the first thing done after the reboot,
before the pipeline is started.

## What the wedge is, and what it is not (2026-08-22)

The wedge described above was chased to a wrong conclusion first, so both the
negative result and the reasoning that produced it are worth keeping. The
conclusion drawn at the time -- that `-j1` was the only answer -- did not hold:
see the GCC garbage-collector wrapper below, which brought `-j2` back.

**What the wedge actually is.** Two concurrent *large* translation units hang the
machine. The same two blink platform units wedged it twice at `-j2`
(`platform_jumbo_71`, 14 merged files, and `platform_jumbo_72`, which carries the
four big generated tables -- `character_property_data`, `color_data`,
`font_family_names`, `runtime_enabled_features`). Compiled **one at a time** both
finished normally: 346,472 and 459,356 bytes, `cc1plus` at 49% CPU throughout. So
it is not the individual unit, and not memory -- 1.7 GB free, `needed memory: 0`,
swap untouched every time.

Note also that `jumbo_file_merge_limit` is an average, not a cap.
`merge_for_jumbo.py` does path-aware chunking: it starts from uniform chunks and
then moves the boundaries to path-rank boundaries, so individual units overshoot
the limit. With the limit at 8, unit 71 still got 14 files. Setting the limit
does not bound the worst case.

**The hypothesis that failed.** The morning's kernel panic was
`PAGE_ASSERT(page->State() != PAGE_STATE_MODIFIED)` in `vm_page_free_etc()`,
reached from `VMCache::Delete()`, in a `cc1plus` thread unmapping a 32 MiB range.
Upstream fixed exactly that in `a964c2cdaf` (bug #20240). A concurrency-dependent
hang plus a panic in the same VM path made "one bug, two faces" look compelling.
It was tested directly: the full `haiku` package set was upgraded to a kernel
carrying that fix (plus `vm.cpp` +42 and `vm_page_writer.cpp` +8), the two
offending objects were deleted, and they were rebuilt concurrently at `-j2` --
the exact workload that had wedged the old kernel twice.

It wedged again, identically. Twelve minutes of both cores at 49%, then 0% CPU,
both objects at zero length, `cc1plus` surviving SIGKILL, every write to `/boot`
blocked. **So the panic and the wedge are separate defects.** The new kernel does
fix the panic -- no assert has fired since -- and it boots cleanly with two CPUs,
wireless, and the no-logo loader, so it was kept.

**What to do about it.** `RCHROMIUM_JOBS=1` in
`scripts/continue_native_content_shell87.sh` (original saved as `.bak-j2`). Half
speed, but it runs unattended, and the watchdog cannot recover a wedge -- every
process is alive, so its "nothing is running" restart condition never fires. Each
wedge costs a reboot, manual recovery, and whatever writes the filesystem had not
flushed; that is worse than the lost parallelism.

The next lead, for whoever picks this up: the kernel's `page daemon` thread keeps
running through the wedge (measured at 1.0% and 4.2% while every `cc1plus` sat at
0%), and swap grows slightly even with 1.7 GB free. Something in the paging path
is making progress while the faulted threads never resume. That is where to look,
not in `VMCache::Delete`.

**A second trigger, same defect (2026-08-30).** The wedge recurred -- total
freeze, no panic screen, mouse cursor dead, unrecoverable short of a hard power
cycle -- this time in `content/browser`'s jumbo units, not blink platform's.
Every `browser_jumbo_N.o` that reached completion in `.ninja_log` took between
2.4 and 35.7 minutes on its own; `jumbo_file_merge_limit = 8` did not save it,
because content/browser's own files are large enough alone (`browser_main_loop.cc`
is 57 KB and one of the heaviest single translation units anywhere in Chromium).
Two of these landing on the machine's two `-j2` slots at once reproduced the
exact same symptom already described above.

First fix tried: `never_build_jumbo = is_haiku` on the whole `browser`
`jumbo_static_library` (same technique as the V8 targets in `0050`-`0052`).
It worked, but cost far more than expected -- un-merging turned 109 jumbo
units into 833 individual compiles, and per-file times measured afterward
showed why jumbo existed in the first place: most content/browser files pay
several minutes each just parsing the same heavy Chromium/Mojo headers
repeatedly (`appcache_request_handler.cc` alone: 12.4 min; ten sampled files
averaged over 6 min each). Total throughput for this one target dropped
roughly 3x. Reverted.

**What replaced it (patch `0061`).** Went back to `.ninja_log`'s per-merged-
group history to separate a genuine single-file outlier from a diffusely
heavy group. `browser_jumbo_13` (35.7 min) merged only three files, two of
them a few KB each, so its time is essentially `browser_main_loop.cc` alone --
a real outlier worth excluding on its own. The appcache group
(`browser_jumbo_2`-`6`, 21-33 min each merging ~8 files) is different: not
one bad file dragging light partners, but many moderately-heavy files (6-12
min each) compounding together. Of those, `appcache_internals_ui.cc` and
`appcache_request_handler.cc` are the two confirmed individually heaviest
(12.3 and 12.4 min alone). `0061` excludes exactly these three files via
`jumbo_excluded_sources`, gated on `is_haiku`, leaving `jumbo_file_merge_limit
= 8` and jumbo itself intact for everything else in content/browser. This
does not guarantee no two-heavy-file pairing can ever recur in the appcache
group, but every remaining unit is in the 6-12 min range that never
reproduced a wedge, not the 21-35 min range that did -- without the 3x
throughput cost of disabling jumbo for the whole target.

**A third occurrence, a better diagnostic this time (2026-08-31).** Two
consecutive watchdog restarts both failed to progress past
`browser_jumbo_45.o` -- not slow, genuinely stuck: no toolchain process
running, `.ninja_log` flat for 30-45 minutes each time, watchdog eventually
parked. Running ninja by hand under direct observation caught it live: free
memory fell from 1.9 GB to 46 MB in under two minutes while
`browser_jumbo_46.o` and `browser_jumbo_47.o` compiled concurrently at `-j2`,
aborted with SIGINT just ahead of what very likely would have been another
full wedge.

Rather than guess again, all 14 files across those two jumbo groups were
compiled standalone, one at a time, with peak memory sampled every second.
Eleven were unremarkable (265-577 MB). Three were not:
`media/audio_stream_monitor.cc` (490 s, 1.35 GB), `media/flinging_renderer.cc`
(444 s, 1.27 GB), and `media/forwarding_audio_stream_factory.cc` (493 s, 1.37
GB) -- all wrapping heavy audio-service Mojo IPC interfaces, none of them
large as raw source (13 KB at most). Any two of these summing in the same
`-j2` window exceeds this machine's 2 GB of RAM outright. Patch `0062` adds
exactly these three to the same `is_haiku`-gated `jumbo_excluded_sources`
list `0061` started; the other eleven stay in jumbo.

The general lesson: this class of outlier isn't tied to raw file size or to
any one subsystem (browser_main_loop.cc, appcache, now media/audio) -- it's
wherever a file pulls in unusually heavy generated bindings. It will very
likely surface again in some other content/browser subdirectory. When it
does, this same measure-each-file-standalone method is what found the actual
culprits both times, faster and more precisely than guessing from merged-
group times alone.

Two process notes from this episode. Repeated resets during POST are the SMP
reroll re-tossing for the second CPU, up to eight times, roughly four minutes;
they were twice mistaken for a boot failure and caused two pointless rollbacks --
`grep -iE 'reroll|early wake' /var/log/syslog` settles it in one command. And
running `ninja` by hand needs the pipeline's environment: without
`PKG_CONFIG_PATH=/boot/system/develop/lib/x86/pkgconfig`, `gn gen` fails on `nss`
and ninja refuses to build anything.

## The wedge is an upstream Haiku bug, and it stays a local patch

For the record, since it changes how the fix should be treated: the modified-page
quota deadlock documented above is **upstream Haiku's bug, not something this port
introduced.** It arrived with `de0a0420a8` (2026-04-14, "Implement modified page
quotas and wait for them in the file cache"), is present in every kernel from
April 2026 onward, and upstream master as of `c8c92a655d` still has it -- the only
later commit touching `vm_page_writer.cpp` is `fbe4749097`, a rename.

Reporting it upstream is out of scope here and should not be proposed. The fix
lives in this patch set and travels with `vaio-p-patches.diff`; treat it as
permanently ours and re-apply it whenever the base moves, the same as every other
patch here. Note it will keep looking like a VAIO-specific fault to anyone who has
not read this: the quota is denominated in estimated write *duration*, so only a
disk slow enough to inflate that estimate crosses the threshold.

## Measured: QEMU on this host is slower than the VAIO, so the build stays put

The idea was that emulating x86 on an Apple M2 with four vCPUs would beat a
single-core 1.33 GHz Atom, and that a fast virtual disk would sidestep the
modified-page quota deadlock. The second half is probably true. The first half is
not, and it was measured rather than assumed.

`qemu-system-i386` on Apple Silicon has **no hardware acceleration** -- `-accel
help` lists `tcg` only -- so every x86 instruction is translated. The cost is much
larger than the M2's advantage over the Atom.

Same template-heavy 43-line C++ file, same `g++-x86 -std=gnu++14 -O2`, same
secondary ABI, on both machines:

| | wall time | per object |
| --- | --- | --- |
| VAIO, 1 compile (its build running alongside, so contended) | 98.4 s | 98.4 s |
| QEMU 4 vCPU, 1 compile, machine otherwise idle | 256.2 s | 256.2 s |
| QEMU 4 vCPU, 4 concurrent compiles | 516.0 s | **129.0 s** |

So one emulated core is about 2.6x slower than one contended Atom core, and four
of them together still finish an object more slowly than the Atom does while
sharing itself with a running build. Four concurrent compiles gave only a 2x
aggregate speedup over one, not 4x -- MTTCG does not scale linearly here. Against
an *unloaded* VAIO the gap is wider, and wider again on the boots where the second
CPU comes up.

`gn gen` told the same story earlier and more cheaply: 1040 s in the VM against
152 s on the VAIO, 6.8x, on a workload that is mostly single-threaded.

**Conclusion: build on the VAIO.** Do not revisit QEMU as a build host on an ARM
Mac unless x86 hardware acceleration becomes available.

The VM is still worth keeping for what it is good at -- it boots in ~36 s to a
working sshd, has four CPUs that always come up, and cannot wedge the way the
VAIO does -- so it is the right place to smoke-test `content_shell` once it links,
and to try anything that risks making a machine unbootable. Its disk image is
`~/haiku-qemu/target.img` (40 GB, installed and provisioned: `g++-x86`,
python 3.14, ninja 1.13.2, the x86 dev headers, and a 2.5 GB copy of the source
tree with `out/` excluded). Boot it with the same qemu line recorded in the
session log; the ssh key is already in place on port 2222.

Two notes on getting it built, since they cost time: the `haiku_devel` and
`haiku_x86_devel` packages are hard-pinned to the exact `haiku` version, so they
have to come from the same local build that produced the image (`_115` from
`generated.vaio-master`), not from HaikuPorts. And a hand-rolled install needs the
packagefs shine-through directories -- `system/{cache,non-packaged,var}` and the
`home/config` equivalents -- or the system mounts read-only and init dies with
"Launching init /tmp failed: Read-only file system".

## The final link

Never reached yet, and the largest unknown in every estimate, so it was probed
ahead of time rather than waited for. The edge is in
`obj/content/shell/content_shell.ninja`, not `build.ninja`.

**Scale.** One `link` edge with **6,376 inputs** -- 5,479 `.o` and 897 `.a` --
written out as a 480,819-byte command line. The 5,217 objects that exist today
total 139 MB, so the complete input set should land near 170-190 MB. The
archives are small (605 of them come to 13 MB): most code is named directly as
objects rather than collected into libraries. It is a single static link,
`is_component_build` is false, driven by `g++-x86` over GNU ld (BFD) 2.45.1,
32-bit -- and that ld is single-threaded, rejects `--threads`, and there is no
gold, lld or mold on the machine. The link cannot be parallelised.

The ldflags GN generates:

```
-Wl,--fatal-warnings -Wl,--build-id -fPIC -Wl,-z,noexecstack -Wl,-z,relro
-Wl,-z,now -Wl,-z,defs -Wl,--as-needed -m32 -Werror -Wl,-O2 -Wl,--gc-sections
-Wl,--disable-new-dtags -Wl,-rpath=$ORIGIN -L/packages/nss_x86...
libs = -latomic -lsmime3 -lnss3 -lnssutil3 -lplds4 -lplc4 -lnspr4 -lnetwork
       -lm -luuid -lbe -lmedia -lexecinfo
```

with the whole input list inside `--start-group`/`--end-group`.

**Why memory is the question.** `--gc-sections` builds a whole-program section
reachability graph on top of everything ld already holds mapped, `-Wl,-O2` adds
its own pass, and the group brackets make ld rescan the archives until the
symbol set settles. Against 1.4 GB free on a 2,037 MB machine, with the linker
itself a 32-bit process, a link over ~190 MB of input is plausible but not
comfortable: GNU ld commonly peaks at several times its input size with
`--gc-sections`.

**The one large mitigation is already in place.** `symbol_level = 0`,
`blink_symbol_level = 0` and `v8_symbol_level = 0`, so none of these objects
carry debug information -- the single biggest factor in Chromium link memory.
Without it this link would not be worth attempting here, and it also means there
is nothing left to strip as a fallback.

**The machinery is known to work.** Rather than guess, a small executable was
linked with the same rule and flags: `build_utf8_validator_tables`, 279 inputs
including `base`, produced a 589,816-byte binary in 77 s. So on Haiku with this
toolchain `--fatal-warnings` does not trip, `-z,defs` resolves, `--gc-sections`
works, and the whole `-lbe -lnetwork -lmedia -lexecinfo` set links. What remains
unknown for `content_shell` is **scale** and **whether every symbol resolves
across the full set** -- not whether the link rule is viable.

Held in reserve if it runs out of address space, both confirmed supported by
this ld: `-Wl,--no-keep-memory` and `-Wl,--reduce-memory-overheads` (available
opt-in through `RCHROMIUM_LINK_LOWMEM=1` in the toolchain wrapper). Dropping
`--gc-sections` is the next lever, and `is_component_build = true` removes the
giant link entirely at the cost of shipping many shared objects. Do not apply
any of these pre-emptively: they would mask the real failure mode, and the flags
are known good at small scale.

**What was left to compile** when this was measured, from `ninja -n
content_shell`: 4,911 commands, of which `third_party` is 1,618 (the rest of
Blink, V8, Skia); the remaining 3,293 are `services` 654, `content` 634,
`components` 627, `media` 168, `ui` 139, `ppapi` 130, `gpu` 100, `device` 76,
`mojo` 54, `printing` 23, `storage` 22. Two thirds of the remaining work is
therefore ordinary Chromium C++ rather than Blink core, which is the part that
runs at 0.2-0.4 edges per minute. Estimates made from inside Blink core are the
pessimistic end, not the average.

## `--gc-sections` is load-bearing, not a memory lever (2026-09-22)

The section above lists "dropping `--gc-sections`" as the next lever after
`--no-keep-memory` and `--reduce-memory-overheads`, and
`scripts/linkretry-verified.sh` acted on that from attempt 3 onwards. It cannot
work. Four attempts on 2026-09-22 with the flag dropped all failed, and none of
them failed for memory:

	metrics_jumbo_2.o: in function
	  `metrics::DriveMetricsProvider::GetDriveMetricsOnBackgroundThread(int)':
	undefined reference to
	  `metrics::DriveMetricsProvider::HasSeekPenalty(base::FilePath const&, bool*)'

`components/metrics` only defines `HasSeekPenalty` in its per-OS files --
`drive_metrics_provider_{linux,win,mac,...}.cc` -- and this port compiles none
of them. The reference has therefore been unresolved since the first day this
target was attempted; `--gc-sections` discards the section that makes it before
ld ever has to resolve anything. `scripts/lowmem-toolchain/g++-x86` already
recorded exactly this happening to `protozero_plugin` and `wait4`, and the
lesson simply was not carried across to the retry ladder.

Two things follow. The six retries were never six tries at one thing: attempts
1-2 asked a memory question and attempts 3-6 could not have succeeded at any
amount of free memory, so a run that "failed six times" was really a run that
failed twice. And the levers that are actually available are the ones that cost
ld memory without costing correctness, in this order: `-Wl,-O2` (a string
tail-merge pass), then `--build-id` (ld hashes the finished ~188 MB image and
nothing in this port reads one), then symbols, last, because
`scripts/resolve_haiku_stack.py` needs `.symtab` to turn a Haiku backtrace into
names. The wrapper exposes all three (`RCHROMIUM_GC_NO_O2`,
`RCHROMIUM_NO_BUILDID`, `RCHROMIUM_STRIP`) and the ladder now walks them.

### The `.rodata` zero-page gate earns its keep

`scan_zero_pages.py`'s own docstring warns that a `.rodata`-only failure can be
a genuinely zero const object and says to look the address up before believing
it. Done, on the 2026-09-22 13:35 link, which the gate rejected for four
consecutive zero pages at `0x06ff4000` after the embedded blob had been
repaired and verified:

	0x06f68ae0 +0x0e6e1f  net::(anonymous namespace)::kPreloadedHSTSData

That is the HSTS preload trie, Huffman-coded data with no reason to hold 16 kB
of zeros -- and the known-good binary's `.rodata` has four zero pages that are
each **isolated**, never consecutive, which is the pattern the run limit was
written to allow. So the damage was real, it was outside the V8 blob, and
repairing the blob was not enough. Keep both gates.

## Non-main threads get a 256 kB stack, and V8 runs off it (fixed 2026-09-20)

x.com killed the renderer. The Haiku crash report said it plainly:

	thread 508: Chrome_InProcRendererThread
		state: Exception (Segment violation)
		0x33454b1  Builtins_IncHandler + 0x11
			Frame memory: Unavailable (Bad address)
		0x3273740  Builtins_InterpreterEntryTrampoline + 0xc0
		...         (several hundred more of the same frame)

-- a JavaScript call chain deep enough to run off the stack, with the frame
below the fault already unmapped.

Haiku gives the main thread 64 MB (`USER_MAIN_THREAD_STACK_SIZE`) and every
other thread 256 kB (`USER_STACK_SIZE`). V8 sets its own stack limit from
`--stack-size`, about 1 MB by default, so its guard never fires on a 256 kB
stack. `platform_thread_haiku.cc` returned 0 from
`GetDefaultThreadStackSize()`, meaning "keep the platform default"; patch
0090 returns 4 MB instead -- 4 rather than the 8 MB of the 64-bit ports
because this is a 32-bit machine with 2 GB of RAM and content_shell runs ~40
threads. Haiku commits stack pages on demand, so this costs address space,
not memory. The arm64 port had the identical hole.

After the fix the interpreter no longer overflows, but x.com still stops: the
process now halts in `VizCompositorThread`, which is the same thread that the
13:50 crash report from before any of these changes named. That one is
untouched and still open.

**Linking needs `util_linux_x86_devel`** for `libuuid`, and on this 2 GB
machine `ld` gets killed by the low-memory handler (`signal 21 [Kill
Thread]`) partway through. `-Wl,--no-keep-memory -Wl,--reduce-memory-overheads`
with `-Wl,-O0` completes; plain `-Wl,-O2` with `--no-keep-memory` fails
differently, with `.text is too large`.

## After the stack fix: V8's builtin code reads as zeros (2026-09-20, it was the link)

With the 256 kB stack fixed (patch 0090), x.com no longer overflows the
interpreter stack -- and stops somewhere else instead. Run content_shell with
`--disable-in-process-stack-traces` so Chromium's own handler does not call
`_exit()` first and Haiku's debug_server writes a real report; that is what
produced both this and the stack-overflow finding.

The faulting thread is `Chrome_InProcRendererThread`, and the report says:

	0x78767418	0x330e620	Builtins_JSEntry + 0
		Disassembly:
			Builtins_JSEntry:
			0x0330e620:             0000  addb %al, (%eax) <--

`Builtins_JSEntry` is **all zeros**. V8's embedded builtin code is not there,
and the first instruction executed out of that page is a zero word. An
earlier run without the debugger died as `SEGV_ACCERR` -- a mapped page whose
permissions refused the access -- which fits the same picture from the other
side.

**RESOLVED the same day: it was a bad link, not V8 and not a mapping.** The
zeros are in the file on disk. `nm-x86` puts `Builtins_JSEntry` at vaddr
`0x01d77620`; the text segment loads at vaddr 0, so that is also its file
offset, and `dd` reads 64 zero bytes there straight out of the binary. No
process, no mapping, no V8 involved.

A page scan of the whole first PT_LOAD found 142 zero pages in four
page-aligned runs:

	file 0x00780000   7 pages   .text
	file 0x01d75000  59 pages   .text  (V8's embedded blob; JSEntry is here)
	file 0x06f6a000  67 pages   .rodata
	file 0x07065000   9 pages   .rodata

The Sep 15 binary kept as `/boot/home/content_shell.last-good` has no run
longer than two pages, so this is damage, not layout. Page-aligned runs of
zeros in a file `ld` reported no error writing are a lost writeback, not a
linker decision -- the same machine, the same low-memory link flags, and a
failure mode one step worse than the stale-bytes corruption that
`verify_embedded_blob.py` was written for in the first place.

The lesson is about the gate, not about the linker: `verify_embedded_blob.py`
compares V8's 1 MB blob and nothing else, so it would have caught the 59-page
run and missed the other three. `scripts/scan_zero_pages.py` rejects any whole
zero page in `.text` and any run of four in `.rodata`, and
`linkretry-verified.sh` now runs both gates. **A link done outside that script
is not trustworthy on this machine** -- the Sep 20 00:15 link was, and cost a
day of reading a corrupt binary as a V8 defect.

Relinking through the script (2026-09-20 11:45) produced a binary that passes
both gates -- the blob came out corrupt again and was repaired from
`embedded.o`, which is routine here. With that binary x.com gets all the way
to layout, and the next stop was a second, unrelated omission:
`/boot/home/rchromium-fonts.conf` was not on the machine, so the bundled
fontconfig loaded no configuration, `FontFallbackIterator` ran out of fallback
fonts on the first text it shaped, and `font_cache.cc(472) Check failed: false`
aborted the renderer. `cp assets/rchromium-fonts.conf /boot/home/` -- it is in
`install.sh` and in every README's troubleshooting section, and this machine
had lost it.

With both fixed, **`https://x.com/i/flow/login` renders**: the X logo, "See
what's happening", the phone/Google/Apple buttons with their icons, and the
email field, laid out and shaped correctly at 800x600. The process stays up.
Remaining noise in the log, none of it fatal: `remote_font_face_source.cc(356)`
and `computed_style.cc(1683)` `NOTREACHED()` (web fonts are not loading, so the
page falls back to system fonts), and `No net_fetcher` for AIA/OCSP/CRL.

Two notes for whoever reads a report like the one above. `Builtins_JSEntry` at
`0x330e620` was inside `content_shell_seg0rx` (`0x01597000`-`0x09a85000`) in
the same report's area list, which already said it was the executable's own
file-backed text and not a V8 `CodeRange` mmap; checking that first would have
skipped the whole detour. And the crashing thread varying between runs
(`Chrome_InProcRendererThread`, `VizCompositorThread`, `NetworkService`) was
read as evidence of a bad shared mapping when it is equally what four
scattered holes in the binary look like.

## Half the machine goes to a poll() spin on dead descriptors (2026-09-20)

Symptom, reported from the chair: on x.com, typing a character into a text
field leaves the browser frozen for more than a minute. Measured with an
injected keystroke (see the section below) it did not arrive at all inside 86
seconds.

`top` says why in one line:

	THID   TOTAL    USER   KERNEL  %CPU  THREAD NAME
	 518  4974.43  340.00 4633.00  49.7  NetworkService

Those are milliseconds in a five-second window on a two-CPU machine: one whole
logical CPU, 89% of it in the kernel. `profile -a -k` puts the time in poll()'s
own machinery -- `select_fd`, `deselect_fd`, `create_sem_etc`,
`delete_sem_internal`, `socket_request_notification` -- which is expensive here
because Haiku builds and tears down a semaphore and a select entry per
descriptor per call.

`strace -i -e poll <thread>` names the cause: **203 of 232 poll() calls come
back POLLNVAL** on five descriptors that have been closed while still
registered. Later, with more of the page loaded, 8357 calls in six seconds on
six descriptors, fd 0 among them -- the same descriptor the 2026-09-15
`close(0)` bug closed underneath the browser. Something on Haiku closes
descriptors this process still owns.

libevent's poll backend has no case for POLLNVAL: `poll_dispatch()` maps only
POLLHUP and POLLERR onto EV_READ/EV_WRITE, and a slot nothing is watching has
no `r_ev` or `w_ev` to dispatch to anyway. So it does nothing and calls poll()
again, which returns immediately, forever. `about:blank` never triggers it; it
takes x.com's connection churn. Patch
`0091-stop-libevent-spinning-on-dead-descriptors-on-haiku.patch`.

**Haiku's poll() ORs POLLERR|POLLHUP|POLLNVAL into the caller's `events`
field.** Every slot therefore has a non-zero `events` after the first call, and
"is anything still registered on this descriptor?" has to be asked as "is
POLLIN or POLLOUT set?". The first attempt at the patch tested `events != 0`,
which is always true, and changed nothing -- the measurement afterwards looked
exactly like no patch at all.

## Typing into web pages: three layers, found one at a time (2026-09-21)

"Typing into x.com freezes for a minute" turned out to be three separate
faults stacked on each other. Each one had to be fixed before the next was
visible, and each was measured rather than guessed.

**1. The machine was half gone.** The POLLNVAL spin above. Fixed first because
nothing else can be measured on a machine with one CPU in a kernel loop.

**2. Keys never reached Blink at all.** With the spin gone, a test page said:

	activeElement = i          <- Blink's own focus was right
	NO KEY YET                 <- and no keydown ever arrived

`SetContents` in `shell_platform_delegate_aura.cc` never focused the web
contents. Upstream's aura delegate is the web-test one, where focus comes from
the test runner; every configuration with a real window goes through
`shell_platform_delegate_views`, which this port does not build. Clicking the
page hid it, because `RenderWidgetHostViewEventHandler` calls
`SetKeyboardFocus()` on mouse-down -- so this was invisible to anyone using a
mouse, and total for anyone typing. The arm64 port hit exactly this on
2026-09-19 and its fix ports over: focus the contents from `SetContents`, and
retry on later turns of the loop, because `RenderWidgetHostViewAura::Focus()`
does nothing until the view has a focus client and its window can take focus,
and neither is settled while `SetContents` is still running. Chromium 87's
`WebContents` has no `GetWeakPtr()`, so the liveness check across the retries
is `Shell::windows()`.

**3. The character never got inserted.** After (2), the page saw
`KEYDOWN keyCode=73 key=i` and `document.hasFocus()=true` -- and three
`KEYPRESS charCode=105` for one keystroke, with the field still empty.

`InputMethodMinimal::DispatchKeyEvent()` already calls
`GetTextInputClient()->InsertChar()` from the keystroke event when
`event->GetCharacter()` is non-zero. Posting a second, character event on top
of it is not redundant, it is wrong: three char events reached Blink and
nothing was typed. One event per key.

That event needs a DomKey, and **only when the keymap produced a character**.
Filling in a DomCode and DomKey for every key -- deriving them from the key
code, including for keys with no character -- stopped keydown reaching the
page at all, a worse state than before the change. `haiku_beapi_views.cc` now
sets both from the character and leaves them `NONE` otherwise, which is the
shape the arm64 port uses.

Two process notes, both learned the hard way here. The first attempt changed
the keystroke event and added a character event in one edit, so when keydown
disappeared there was no way to tell which half did it; splitting them took a
whole build cycle to recover. And a `fprintf` in `DispatchKey` printing the
type, key code, DomCode and character settled in one run what three rounds of
reading the code had not: the BView receives every key correctly, so
everything above is downstream of it.

### Where it stands, and the two things still wrong (2026-09-22)

Measured with injected keystrokes and a page that mirrors what it receives
into a `<p>`, because nothing in an editable element is painted (see below):

	keydowns=5 last=o
	ce mirror: [hello] len=5        <- contenteditable took the text
	value mirror: [] len=0          <- the same keys into an <input>

**The keyboard road is open.** input_server -> app_server -> BView ->
`DispatchKey` -> ozone -> the input method -> Blink -> inserted text. An
`fprintf` in `DispatchKey` confirms the view gets every key with the right
code and character.

**Two defects remain, and neither is about keys.**

*Text form controls do not take typed text.* `<input>` and `<textarea>` see
the keydowns and the keypresses, and their value stays empty, while a
`contenteditable` div on the same page takes them. `execCommand("insertText")`
does reach an `<input>` -- it returned true and the value changed -- so the
editing machinery works and it is the keypress default action that does not
land there. `window.getSelection()` after `i.focus()` reports its anchor as
BODY rather than anything inside the control.

*Nothing in an editable element is painted.* The `contenteditable` div holds
"hello" and renders as an empty box. An `<input>` with its value set from
JavaScript renders empty too, and measures 622x14 -- 14 pixels tall for a 22px
font with 5px of padding, with or without an explicit font-family. Ordinary
`<p>` text on the same page paints correctly. Worth trying against the font
fallback gap above: `getComputedStyle` says the default form-control font is
Arial, which this machine does not have and `GetLastResortFallbackFont()`
cannot resolve.

## Every font family lookup fails, and that is why form controls collapse (FIXED 2026-09-22)

This is the one open fault behind "typed text does not show up". It is not a
keyboard problem and not a form-control problem; it is that
`FontCache::GetFontPlatformData()` returns null for **every** family name.

Measured by instrumenting `font_cache.cc` and `font_cache_skia.cc`:

	GetFontPlatformData family=Helvetica      size=20 result=NULL
	GetFontPlatformData family=Arial          size=20 result=NULL
	GetFontPlatformData family=sans-serif     size=20 result=NULL
	GetFontPlatformData family=Sans           size=20 result=NULL
	GetFontPlatformData family=Noto Sans      size=20 result=NULL
	GetFontPlatformData family=DejaVu Sans    size=20 result=NULL
	GetFontPlatformData family=Inter          size=22 result=NULL

The chain from there is short and fully confirmed: no family resolves ->
`FontFallbackList` has no primary font -> `ComputedStyle::GetFontHeight()`
takes its `NOTREACHED()` and returns a zero `FontHeight` -> every line box has
height zero -> an `<input>` is padding and border around a zero-height inner
editor (`294x14` for a 22px font with 5px padding; `294x44` if a
`line-height:30px` is named, which is what proves the font-derived height is
what is missing).

Why the page is not blank anyway: exactly one `SimpleFontData` is built for a
whole page, at the body size, family **Inter** -- and there is no Inter file in
any of the configured font directories, so it does not come through fontconfig
at all. Body text paints from that one font, and `<p>` elements look correctly
spaced because their margins space them, not their line boxes.

What has been ruled out, each by measurement rather than argument:

- *Not the last-resort list.* Patch 0092 adds families Haiku has; they are
  tried (see Noto Sans and DejaVu Sans above) and return null like the rest.
- *Not fontconfig aliases.* Twenty substitution rules, including strong
  `binding="same"` edits for Arial, Helvetica, Times and Courier, changed
  nothing.
- *Not missing or unreadable fonts.* `/boot/system/data/fonts/ttfonts` holds
  100 files, mode `-r--r--r--`, and the process runs as uid 0. Parsing the
  name table of `NotoSans-Regular.ttf` gives family "Noto Sans" exactly.
- *Not an unscanned fontconfig.* Pointed at a fresh `<cachedir>`, fontconfig
  writes a 125 kB cache for that directory on the first run, so it does scan
  and does find the fonts.
- *Not `font_manager_`.* It is null on this port, so `CreateTypeface()` goes
  to `SkTypeface_Factory::FromFamilyNameAndFontStyle()`, and that is what
  returns null. (Note for whoever edits `CreateTypeface`: when `font_manager_`
  *is* set, upstream returns its answer even when that answer is null, without
  falling through to the legacy path. Not the cause here, but a trap.)
- *Not font metrics.* Where a font does resolve the metrics are right:
  `size=20 rawAsc=-19.375 rawDesc=4.82422 -> ascent=19 descent=5 fam=Inter`.

### The probe was silent, and the silence was the answer (2026-09-22)

The paragraph this replaces said the failure was inside
`SkFontConfigInterfaceDirect::matchFamilyName()` and that a probe would split
it into "an empty `FcFontSort`" or "a rejecting `MatchFont`". The probe finally
linked and printed **neither**, in a run where `font_cache_skia.cc`'s own
`LOG(ERROR)` lines came out fine, so the logging works and the function is
simply never called. fontconfig is not involved at all.

`skia::CreateDefaultSkFontMgr()` builds its `SkFontMgr` from
`SkFontConfigInterface::RefGlobal()`, and on this platform that global is not
the direct implementation. `RendererBlinkPlatformImpl`'s constructor
(`content/renderer/renderer_blink_platform_impl.cc:172`) sets it, under a guard
this port already widened to include Haiku:

	#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_HAIKU)
	  RenderThreadImpl::current()->BindHostReceiver(
	      font_service.InitWithNewPipeAndPassReceiver());
	  font_loader_ = sk_make_sp<font_service::FontLoader>(...);
	  SkFontConfigInterface::SetGlobal(font_loader_);
	#endif

So every family lookup is a mojo call to the font service. And the browser
side of that pipe is not connected -- by this port's own decision, thirteen
lines above the bind in
`content/browser/renderer_host/render_process_host_impl.cc:1330`:

	// Haiku is excluded deliberately, for now. ConnectToFontService()
	// constructs a FontServiceApp, which brings up the bundled fontconfig,
	// and that crashes on this platform inside FcFreeTypeLangSet():
	// FcCharSetSubtractCount() faults partway through iterating the charset
	// built from a font file...

	#if defined(OS_LINUX) || defined(OS_CHROMEOS)
	  if (auto font_receiver = receiver.As<font_service::mojom::FontService>())

**That is the whole fault.** The renderer asks for a `FontService`, the
browser declines to bind it, `FontLoader` fails every call, `SkFontMgr` has
nothing to answer with, every family resolves to null, `GetFontHeight()` takes
its `NOTREACHED()`, every line box is zero-height and an `<input>` collapses to
padding around nothing. The note calling it "a real loss" was right about the
cost and wrong about where to look next: none of the fontconfig work above --
the aliases, the cache directory, the last-resort list, the Skia probe -- could
ever have mattered, because none of that code runs.

The exclusion lives in patch
`0079-guard-font-service-and-discardable-memory-on-haiku.patch`, which also
carries an unrelated and still-wanted guard on
`DiscardableSharedMemoryManager::Get()` returning null. Only the font half is
in question.

And the crash it names does not reproduce. `/boot/home/fctest2` is the
standalone harness built on 2026-09-11 from Chromium's own `libfontconfig.a`
plus its freetype, harfbuzz, libxml, ICU, zlib, libpng and brotli objects,
entering fontconfig by the same `FcInit()` chain the crash backtrace showed.
Run today against the current `rchromium-fonts.conf`:

	fontconfig version: 21391
	FcInit OK
	system font set: 116 fonts
	fonts carrying a langset: 116 of 116
	done

116 of 116 fonts get a langset -- which is `FcFreeTypeLangSet`, the frame the
comment blames -- and nothing faults. That is consistent with what
`docs/browser-ui-plan.md` recorded under "fontconfig was never the problem" on
2026-09-11, and with the fact that `/boot/home/rchromium-fonts.conf` exists at
all: the minimal config was written precisely because pointing the bundled
fontconfig at Haiku's own `/boot/system/settings/fonts/fonts.conf` crashes in
`FcCharSetSubtractCount`. The exclusion looks like a decision taken before
that config existed and never revisited.

So the measurement to make is simply to restore the bind and see.

### Restored, and that was the whole fault (2026-09-22)

Patch 0079's font half is gone; its `DiscardableSharedMemoryManager` guard
stays. With the receiver bound, on `file:///boot/home/typetest.html`:

	[RCH] binding FontService receiver
	legacy typeface for "Noto Sans"  -> ok
	legacy typeface for "sans-serif" -> ok
	legacy typeface for "monospace"  -> ok
	legacy typeface for "Arial"      -> NULL

	input=[hello] textarea=[] ce=[contenteditable] active=i
	input box 414x40

and on `fonttest.html`, four paragraphs in four families measuring
`784x26 | 784x30 | 784x30 | 784x30` with the `<input>` at `353x40`. Arial,
Helvetica, Times and Courier still come back null, which is correct -- Haiku
has none of them, and `MatchFont()` refusing a substitute is what sends Blink
on to the next family in the CSS list. The probe shows it working:

	fc sort  family=monospace   set=0xb205950 n=116
	fc match family=monospace   postcfg=Noto Sans Mono  match=0xb3ad660
	fc match family=Courier New postcfg=Courier New     match=(nil)

`computed_style.cc`'s `NOTREACHED()` fires **zero** times where it fired
eleven on the same page an hour earlier. No crash in `FcCharSetSubtractCount`
or anywhere else.

**Both of the "two things still wrong" above are fixed by this one change**,
and neither was what it looked like. Text form controls took no typed text
because their inner editor had zero height, not because the keypress default
action was going astray; nothing painted in an editable element for the same
reason.

On the real page, `https://x.com/i/flow/login`: the login card renders
completely -- the X mark, the heading, the three provider buttons with their
Google and Apple glyphs, the disabled Continue button, the terms text -- and
`rainygirl` typed into "Email or username" appears in the field with its
caret, with the floating label animating up the way it does anywhere else. No
crash over a ninety-second load.

Keep `fctest2`: it turns a font hypothesis from a forty-minute
rebuild-and-link into three seconds. And keep the shape of this mistake in
mind. Two days went into fontconfig, Skia and the input method because the
symptom was "typing does not work"; the probe that settled it did so by
printing **nothing**, which was only readable as an answer because another
probe in the same run printed something. An instrument that can only say yes
cannot tell silence from absence.

One thing found while reading for it, worth fixing on its own account:
`third_party/fontconfig/include/config.h` is a Linux x86-64 configure result
and says `SIZEOF_VOID_P 8`, `ALIGNOF_VOID_P 8`, `ALIGNOF_DOUBLE 8` in a
32-bit build. `fcarch.h` turns that into `FC_ARCHITECTURE "le64"`, which is
why this port's cache files are named `...-le64.cache-7` -- a 32-bit process
claiming a 64-bit architecture tag. `src/src/fcarch.c`, whose entire contents
are static assertions that would catch this
(`0x08 + 2*SIZEOF_VOID_P == sizeof (FcCharSet)`), is not in Chromium's source
list, so nothing checks it. It is consistent with itself as long as only this
build writes that directory, so it is not obviously the crash -- but a
fontconfig that is wrong about its own pointer size is not a good place to
start looking for corrupt `FcCharSet` internals.

Two notes on method, both learned expensively here. Instrument with
`LOG(ERROR)`, not `fprintf`: a raw `fprintf` from Blink does not reach the
same stream as the `[RCH]` prints, which all come from the browser-side ozone
code, and reading "no output" as "the function is never called" cost two build
cycles. And read every return path in a function before putting a probe at the
bottom of it -- `CreateTypeface()` returns early in a branch that was never
read, so a probe on its last line printed nothing and was misread the same way.

## Injecting keystrokes for a real test (2026-09-20)

Verifying "can you type into the page" needs the keys to travel the road a
real keyboard's do: input_server -> app_server -> the focused BWindow -> its
focus view. Posting a BMessage straight at the window skips most of that and
never reaches the page, which is why an earlier attempt proved nothing.

An input_server *device* add-on enqueues events at the top of that road, so
the browser cannot tell them from typing. Build it against
`<add-ons/input_server/InputServerDevice.h>`, install into
`~/config/non-packaged/add-ons/input_server/devices/`, restart input_server,
and have a watcher thread read a file and `EnqueueMessage()` a `B_KEY_DOWN`
and `B_KEY_UP` per character. The message needs `when`, `key` (the raw US
keymap code), `modifiers`, `raw_char`, `byte`, `bytes` and `states`.
Confirmed by typing `abc.x_9@y` into StyledEdit -- dots, underscores and `@`
included.

Killing input_server to reload the add-on also takes Deskbar and Tracker's
window decorations with it; a reboot restores them.

## The GCC GC wrapper keeps `-j2` alive, and it made the build faster (2026-08-24)

Page exhaustion, not the quota deadlock, is what was panicking the machine at
`-j2` after the kernel fix went in. The third panic was `unlock_memory_etc`
<- `lock_memory_etc` <- `IOBuffer::LockMemory(1890 pages)` on a BFS read for
`cc1plus`, with the syslog flooded by `low resource pages: critical -> ...`.
Swap was ruled out as the cure: it sat 99% free through every critical, so the
limit is the 1.8" disk's speed, not space.

The mitigation is a pair of PATH wrappers, `bin-lowmem/{gcc,g++}-x86`, that add
`--param ggc-min-expand=10 --param ggc-min-heapsize=32768` to compiles only.
They are wrappers rather than GN cflags on purpose: ninja hashes the command
string, so putting the flags in `cflags` would invalidate and rebuild all
~10,000 objects already done. The wrapper leaves the string as `g++-x86`.
Copies live in `scripts/lowmem-toolchain/`, and
`scripts/continue_native_content_shell87.sh` prepends the directory to PATH.

The prediction was that this would trade speed for memory. It did the opposite.
Through the heaviest section of Blink core, with both compilers wrapped:

| unit | without wrapper | with wrapper |
|---|---|---|
| `core_generated_jumbo_12` | 24.29 min | 17.94 min |
| `css_jumbo_1` | 12.08 min | 9.82 min |
| `css_jumbo_2` | 8.35 min | 7.69 min |
| `css_jumbo_3` | 3.25 min | 2.90 min |

Tighter GC costs CPU, but these units had been paying far more than that in
paging. Removing the pressure more than paid for the collections. Swap use
stayed at 11-22 MB of 2,037, free memory at ~1.75 GB, and `low resource pages:
critical ->` did not advance past 63 once through the section that had panicked
the day before.

Two things to know when reading the health scripts. The `-> critical` count is
not a usable measure: `low_resource_manager.cpp:174` logs `old -> new`, yet the
inbound count reads 0 while the outbound reads 63, so trust the absence of
panic, assert and wedge plus the swap figure instead. And the `-j` comment in
`continue_native_content_shell87.sh` still describes the wedge as the reason for
`RCHROMIUM_JOBS=1`; that wedge was the modified-page quota deadlock and is fixed
in the kernel. The comment is stale, and was left alone deliberately -- `sh`
reads a running script incrementally, so editing it mid-build risks corrupting
execution. Fix it once the build is done.

## ninja can stop collecting finished work while staying alive (2026-08-24)

A failure mode distinct from the jumbo-TU wedge documented above, and one that
none of the existing checks caught.

At 23:06 ninja stopped making progress. What it looked like:

* `svg_jumbo_12.o` (23:07) and `svg_jumbo_14.o` (23:10) were written to disk
  **after** the last `.ninja_log` entry at 23:06. The compilers ran, finished
  normally and produced their objects; ninja never collected the completions and
  so never started anything else.
* `strace` attached for 400 seconds captured **zero bytes**. ninja was not
  issuing syscalls at all -- blocked inside one it had already entered.
* Nothing in the syslog. Memory was fine (last `critical` at 20:52, 1.8 GB
  free). `/boot` was writable. No assert, no panic.

Contrast with the earlier wedge: there `cc1plus` stayed alive at 0% CPU,
survived SIGKILL, and every write to `/boot` blocked afterwards. Here the
compilers exited cleanly and the filesystem was healthy. The two need different
diagnoses and only one of them is a kernel problem.

### It went unreported for 33 minutes, and that was a tooling gap

`memcheck.sh` printed the last line of `native-content-shell.log` and the last
four durations from `.ninja_log`. Both look identical whether the build is
working or dead: the text log is truncated on every watchdog restart, and past
durations do not change. The only signal was that `.ninja_log` had stopped
growing, which nothing was reading.

The watchdog had the same blind spot from the other direction: its test was
`ninja_running || pipeline_running`, so an alive-but-wedged ninja put it to sleep
for 300 seconds at a time, indefinitely.

Both now check `.ninja_log`'s mtime. `memcheck.sh` prints
`progress: <n> edges, last <m> min ago [ok|STALLED|suspicious]`, and the
watchdog treats "ninja alive, `.ninja_log` idle 30+ min, no toolchain process
running" as a stall and recovers from it. Thirty minutes is deliberately
generous: the longest single unit measured is 24.29 min
(`core_generated_jumbo_12`), and the no-toolchain-process test is what keeps a
long final link from being mistaken for a stall.

### Killing ninja costs more than it looks

Recovering meant killing ninja, and `kill -9` truncated `.ninja_deps` from
15,214,540 to about 13.2 MB. Objects survive a truncated deps log; their
*records* do not, and ninja rebuilds anything whose record is missing:

```
ninja explain: deps for '.../css_jumbo_1.o' are missing
ninja explain: stored deps info out of date for '.../clipboard_types_jumbo_1.o'
```

180 blink core objects sit on disk and will be compiled again -- roughly 14
hours. Part of that is the truncation and part is the watchdog's `gn gen`
re-run changing command hashes; the two cannot be separated after the fact.

SIGTERM was sent first, but only four seconds were allowed before escalating,
which is nothing for a process blocked in a syscall. `stop_ninja()` in the
watchdog now copies `.ninja_deps` to `.ninja_deps.pre-kill` first, then walks
INT -> TERM -> KILL with a full minute at each step.

### If this happens again

1. `sh /boot/home/rchromium-native/memcheck.sh` -- the `[STALLED]` marker and the
   watchdog line are the first two things printed.
2. Confirm before acting: is `.ninja_log`'s mtime old, are objects newer than it,
   is `strace -f -s <ninja pid>` silent?
3. Let the watchdog handle it. It backs up the deps log and escalates signals
   properly. Starting a second ninja by hand is the one thing that makes this
   worse, and the watchdog is already trying to restart.

## Recurring: the wireless dies during long builds (2026-08-25)

Twice on 2026-08-25 -- around 08:11 and again around 11:00 -- the machine went
unreachable over both Tailscale and the LAN while a build was running. The user
identifies this as a known recurring wireless failure on this box, not a crash.
That reframes the morning incident: the 08:13 reboot was almost certainly a
manual recovery, not evidence of a kernel fault. Nothing in the syslog pointed
at one, which fits.

Two things follow. The build keeps running through it -- ninja does not care
about the network -- so an unreachable machine is not a stopped build, and the
first thing to check on reconnect is `.ninja_log`'s mtime, not the uptime.
And the loss of SSH is the loss of *observation*, which is why every safeguard
had to move onto the machine itself (the watchdog, the crash reports) rather
than living in a session that can be cut at any moment.

### Prime suspect: our own power-cycle escalation

`if_ath.c` in this patch set escalates repeated beacon-miss recovery to a full
PCI D3/D0 power cycle, gated to exactly this card:

```c
if (!associated && sc->sc_bmiss_count >= 3
    && pci_get_vendor(sc->sc_dev) == 0x168c
    && pci_get_device(sc->sc_dev) == 0x002a) {
	pci_set_powerstate(sc->sc_dev, PCI_POWERSTATE_D3);
```

The comment directly above it already states the risk: "an Atheros part that
needs its firmware reloaded after D3 may not come back at all until the machine
is rebooted". The same comment records the other half of the mechanism -- under
a native Chromium build at -j2, beacon misses arrive roughly every two to four
minutes for as long as the build runs.

Put together: during a long build the miss rate is high, so if the association
drops for any reason the counter reaches three quickly, the adapter is power
cycled, and it may never return until reboot. That matches the symptom exactly,
including why it only shows up during long builds.

This is a hypothesis, not a finding. What would settle it is a single line in
the syslog before the network goes:

```
repeated beacon miss recovery, power-cycling the adapter
```

Grep for it after the next occurrence. The morning's syslog.1 showed the
deauth (reason 3) and SCAN -> AUTH -> INIT churn but not that line -- though
that file had rotated at 512 KB and only covered 08:05-08:12, so its absence
proves nothing.

If confirmed, the escalation needs to be safer rather than removed: it exists
because this card otherwise stays deaf after repeated beacon loss, so both
branches end in dead wireless. Options worth weighing then: verify the adapter
actually came back after D0 and reset it if not; cap the escalation to once per
boot; or require the misses to span a longer window so build-load noise cannot
reach the threshold.

## `sh` dies inside `$( )` on this system, and it takes the watchdog with it (2026-08-26)

Fifty-five crash reports appeared on the Desktop in one day, all of
`/boot/system/bin/bash` running as `sh`, all at the same instruction. They are
not incidental: they arrived in bursts at 07:06, 07:12 and 07:17 -- exactly 300
seconds apart, which is the watchdog's poll interval -- and the watchdog was
dead afterwards. The supervisor was being killed by the machine state it exists
to survive, which is why stalls went unattended for hours on 8/25 and 8/26.

The stack is the same every time:

    command_substitute + 0x5aa        bash forks a child for $( ), child exits
      exit -> terminate_program
        call_term_functions(image_t*)     runtime_loader walks the fini list
          libhistory.so.8.2 _fini (0x8708)
            __do_global_dtors_aux (0x1d94)
              1dc7:  mov 0x114(%ebx),%eax     ; ebx = GOT base, so this is .data[0]
              1dcd:  cmpl $0x0,(%eax)         ; <-- SEGV

`.data[0]` is `dtor_idx` from gcc's crtstuff. Its file image is `08a50000`, an
`R_386_RELATIVE` relocation pointing at `.dtors + 4`, and `.dtors` itself is the
usual `ffffffff 00000000` terminator pair. Nothing about that is unusual --
`libreadline.so.8.2` on the same system has byte-identical structure, the same
relocation on its own `.data[0]`, and never crashes.

What settles it is the value that was actually in `dtor_idx`, read out of the
crash frame: `0x00178d30`, in every one of the reports. In that process
`libhistory` was mapped at `0x0048a000`, so `dtor_idx` should have held
`0x00494508`, inside `libhistory.so.8.2_seg1rw` (`0x00493000`-`0x00495000`).
`0x00178d30` is inside the area the report labels `heap` (`0x00178000`-
`0x00188000`).

So this is not a relocation that failed to apply and not a loader bug: a **heap
pointer was written over another image's `.data`**. Other reports from the same
day show `chunk info corrupted` raised from `malloc_bytes` -- the allocator
catching its own metadata damaged. Both are the same wild write, landing in
different places depending on the allocation pattern, which is why it only shows
up when memory is tight enough to change that pattern. Under 1.5 GB free it
never fired; the reports cluster at 1.29-1.98 GB used of 1.99 GB.

Narrowing it further needs a reproducer under a debugger, and 300 command
substitutions in a loop on an idle machine produce nothing. It is a fault in the
bash/readline/history stack as built for this system, it is upstream's, and per
the standing rule it is recorded here rather than reported.

**What was done instead.** The watchdog now takes one `ps` per pass into a
variable and answers with shell pattern matching, instead of running a
`ps | grep | grep` pipeline per question -- roughly twenty-five forks a pass down
to three (`scripts/watchdog_native_content_shell87.sh`, `take_snapshot` /
`in_snapshot`). That does not fix the bug; it takes most of the exposure away.
Before the change the watchdog died twice in one day; after it, none, and the
report count stopped growing.

The reports themselves are archived in `/boot/home/crash-reports/2026-08-26/`
rather than deleted -- they are the evidence for the paragraph above, and
`debug_server`'s `default_action report` stays on, because it is also what
caught ninja's own segfault the same day.

## Java is packaged for Haiku, and the build needs it once (2026-08-29)

`//mojo/public/js:bindings_lite` runs the closure compiler over Chromium's
JavaScript Mojo bindings, and the closure compiler is a jar. On a machine with
no Java the target fails with `java: command not found` -- which is easy to
dismiss, because nothing content_shell links depends on the .js it produces and
`ninja -k 20` walks straight past it.

Dismissing it was wrong on the facts, though: **OpenJDK is packaged for Haiku
x86_gcc2**, versions 7 through 17, and no build is involved.

    pkgman install --home openjdk11_x86_jre

`--home` matters. It installs into `~/config/packages` and leaves
`/boot/system/packages` untouched, so it cannot repeat the 2026-08-16 system
update that cost a day of package recovery. It pulled in three packages
(the JRE, dejavu, ca_root_certificates_java) and the system set stayed at 279.

Two things then had to be sorted out:

- The binary lands at `~/config/lib/x86/openjdk11/bin/java`, which is not on
  any PATH. The pipeline already searches `~/config/non-packaged/bin`, where
  `python` and `meson` are symlinked, so `java` goes there too.

- Default heap sizing breaks it. OpenJDK derives its heap from total RAM -- on
  this 2 GB box about 510 MB -- and with the build holding memory it refuses to
  start: "Could not reserve enough space for 522240KB object heap". So the entry
  in `non-packaged/bin` is not a symlink but `scripts/java-wrapper`, which caps
  the heap at 192 MB. That is far more than the closure compiler needs for a few
  hundred KB of JavaScript, and it leaves the two cc1plus processes their room.

`java -version` working and the jar reporting `Closure Compiler v20200517` was
not enough evidence, though -- both were run without the flags the real build
passes, and the actual `mojo_bindings_lite.js` edge kept failing with no
visible error (`js_binary.py` swallows stderr on the path this build takes, so
the ninja log showed only `b''`). Two more things had to be fixed, found by
reproducing the exact failing command line from the log:

- `third_party/closure_compiler/compiler.py`'s `_JAR_COMMAND` hardcodes
  `-Xms1024m` ahead of anything this wrapper appends, and the JVM honors the
  *last* `-Xms`/`-Xmx` it sees. Appending `-Xmx192m` after that made the
  initial heap larger than the maximum, which the JVM refuses outright:
  "Initial heap size set to a larger value than the maximum heap size". Fixed
  by having the wrapper strip any `-Xms*`/`-Xmx*`/`-Xss*` the caller passed,
  wherever they appear, rather than trying to out-order them.

- JVM options are only recognised *before* `-jar <file>`; the real call is
  `-jar -Xms1024m -client ... compiler.jar --js ...`, so appending heap flags
  at the end (as the first fix did) handed them to the closure compiler as
  program arguments, which it rejected: `"-Xms32m" is not a valid option`. JVM
  options do not need to sit next to `-jar`, only somewhere before it, so the
  wrapper now prepends its heap flags rather than appending them.

Verified by extracting the full failing command line from
`native-content-shell.log` and running it by hand: exit 0, and
`gen/mojo/public/js/mojo_bindings_lite.js` produced (19,979 bytes). The lesson
worth keeping is that `java -version` and even running the jar directly are
not the same test as running it with the caller's actual argv -- the caller's
own flags were the entire defect both times.

## The kernel VM defect that made the final link unreachable (2026-09-02)

The content_shell link died twice in thread "ld" while writing the ~190 MB
output, both times with the same panic:

    PANIC: VMArea::Unwire(0x5e1f8000, 0x2000, 0): no such range
      unlock_memory_etc + 0x3e1  <- lock_memory_etc + 0x692
      IOBuffer::LockMemory(2070, true)
      bfs_write / file_write / common_user_io(..., 0x28000 (163840), true)

`lock_memory_etc()` registers a wired range covering an area's whole portion
of the request *before* wiring any page, then wires pages one at a time. Under
memory pressure `vm_soft_fault()` fails part-way, and the cleanup only unwires
the range outright when it failed on the very first page; otherwise it leaves
the range registered at full size and calls `unlock_memory_etc()` for just
`[lockBaseAddress, nextAddress)`. `VMArea::Unwire()` matches on
(base, size, writable) exactly, finds nothing, panics. The arguments match: a
160 KB write wired 40 pages, failed after 2, and the revert asked to unwire
0x2000.

Fixed by shrinking the range to what was actually wired before dropping the
cache lock -- `haiku_kernel_patches/K0001-*.patch`, committed to the fork
(`rainygirl/haiku-private`, branch `patch-master-v2`).

**Build the kernel from the tree the machine is actually running.** The first
attempt built from a clone sitting at hrev99002+0 and would have silently
reverted 148 commits of kernel work -- `vm_page_writer.cpp` +128 lines,
`vm.cpp` +69, `vm_page.cpp` +84, including the modified-page-queue fixes this
port depends on. The running system is hrev99002+148+**dirty**, and that dirty
state lives in the Mac's `wt-new` worktree; reproduce it (`git diff HEAD
--binary`) on the build box before building. Pin the revision by writing
`generated/build/haiku-revision` and `last-built-revision` by hand, or
`haiku_devel`'s exact `requires: haiku==...` pin stops matching and the
development headers deactivate.

Verify the result carries the tree's changes rather than trusting the build
log: `_ReconcileAndNotify` (added by the dirty state) must appear in the
kernel's symbol table. All three kernels built during this episode were
exactly 2202761 bytes, so size proves nothing.

Two build-box workarounds, no sudo available there: Linux Mint 20.3 ships
libzstd.so.1 with no -dev symlink, and on its glibc 2.31 `pthread_once` is
still in libpthread, which the Haiku build-tool link line omits. Both are
supplied by making `~/lib/libzstd.so` a GNU ld linker script that INPUTs the
real libzstd and libpthread.so.0, with `LIBRARY_PATH=$HOME/lib`. Delete
`generated/objects/linux/lib/libbe_build.so` to force it to relink once the
script is in place.

## Deskbar's menu goes `<empty>` when `menu_entries` is missing (2026-09-02)

Not caused by anything in this port -- worth writing down because it looks
alarming and the obvious suspects are all innocent. The whole Deskbar menu,
including its hardcoded About/Restart items, rendered as `<empty>`.

`TBarWindow::MenusBeginning()` (src/apps/deskbar/BarWindow.cpp) resolves
`kDeskbarMenuEntriesFileName`, which is **`menu_entries`**, not `menu`
(DeskbarUtils.cpp:58): first `~/config/settings/deskbar/menu_entries`, else
`/boot/system/data/deskbar/menu_entries`. Whatever it finds goes to
`SetNavDir()`, and `BNavMenu::StartBuildingItemList()` iterates it as a
directory. On this machine the user-side entry was gone, so it fell back to
the system one -- which is a *text file* listing directories to merge. A file
cannot be iterated, the item list came back empty, and nothing was added at
all, hardcoded items included.

Fixed with `ln -s menu ~/config/settings/deskbar/menu_entries`. The user-side
`menu/` also had lost its system entries (3 apps instead of 35, and no Demos,
Desktop applets or Preferences), so it was repopulated from
`/boot/system/data/deskbar/menu` first; backup left at
`menu.backup-before-repopulate`. Both losses are consistent with the unclean
halts of 2026-09-01, which discarded hours of writes under `~/config`.

Checking the data was not enough here: package contents, file attributes, the
Deskbar binary's md5, the MIME database and the registrar were all verified
identical to the original and all innocent. Only reading the Deskbar source
for where the path comes from found it.

## The final link's undefined symbols come in one batch, so fix them in one (2026-09-02)

The first content_shell link that survived to completion (see the kernel VM
defect above) ended with 39 undefined references across 25 distinct symbols.
The temptation is to fix the first one, relink, see the next -- at ~8 minutes a
link plus ~3 for `gn gen`, that is a whole day of round trips.

Do not. **`ld` reports every unresolved symbol in one pass**, so the first
failing link already hands over the complete list. Read it, diagnose all of it,
patch all of it, then link once.

Most of the 25 shared one root cause worth recognising on sight: **GN treats
Haiku as `is_linux`, C++ does not.** `build/config/BUILDCONFIG.gn` sets
`is_linux = true` for this port, so GN puts Linux-only files in the build --
but `build/build_config.h` defines `OS_HAIKU`, never `OS_LINUX`, so a guard
like

    #if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)

compiles the file's entire body away. The object exists and contains nothing.
When a symbol is undefined but its `.cc` is clearly in the target, check the
internal guard before anything else.

The 25 sorted into six shapes, each with a different right answer:

- **A guard that already has a stub branch for other platforms** -- join it.
  `NativeEventObserver::{Register,Deregister}Observer` has empty bodies for
  OS_ANDROID/OS_FUCHSIA; Haiku wants exactly that (0064). Same for minizip's
  `fopen64` aliases, which already have a `__FreeBSD__` branch (0063).
- **A file dropped for Haiku with no replacement** -- write one.
  The HaikuPorts patchset removes `debug/proc_maps_linux.cc` (correctly, there
  is no /proc) but, unlike every other file it drops there, substitutes
  nothing, while `stack_trace_posix.cc` still calls it (0065).
- **A GN condition that is stricter than the code needs** -- relax it.
  `ui/touch_selection` hides its aura sources behind `use_aura &&
  toolkit_views`; neither file touches views, and this port sets
  `toolkit_views = false` (0066).
- **A target that is simply not in this graph** -- guard the caller.
  `SkOpts::Init_skx` is called behind a runtime AVX-512 check the Atom Z520 can
  never pass; QtWebEngine already guards it, so extend that guard (0067).
- **A half-finished earlier patch** -- finish it. 0060 dropped the crashpad
  *library* for Haiku but left the client that subclasses `CrashReporterClient`
  in the build, so its whole vtable went undefined (0068 + U0007).
- **A call site guarded only at runtime** -- guard it at compile time.
  `WebTestingSupport::InjectInternalsObject` is behind a command-line switch,
  which does not stop the linker looking for it (U0008).

Two traps met while writing those patches:

- Hand-counted diff hunk headers were wrong twice ("corrupt patch at line N").
  Apply the edit to the real file, run `diff -u`, and paste that; never count
  `@@ -a,b +c,d @@` by hand.
- `sources -= [...]` must go in the target that *owns* the source. Putting the
  crashpad-client removal in `content_shell_lib` (where the other `is_haiku`
  `deps -=` blocks live) made `gn gen` fail with "you were trying to remove ...
  but it wasn't there"; it belongs in `content_shell_app`.

Finally, the link surfaced a failure that is not a symbol at all:
`v8_context_snapshot_generator`, freshly built, is killed by the kernel when
run ("Kill Thread", exit 149) -- a 122 MB binary plus a V8 heap does not fit
here. `use_v8_context_snapshot = false` in `args.gn` skips it; `snapshot_blob.
bin` is the fallback and was already present. **Budget for the rebuild before
doing this**: that flag is on the compile command line of every target that
uses it, so ~178 objects, including the heavy content/browser jumbo units, are
invalidated. Generating the snapshot by hand and leaving the flag alone would
have been cheaper if the generator could be made to run at all.

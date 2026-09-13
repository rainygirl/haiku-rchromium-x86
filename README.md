# R Chromium native Haiku port

This directory is an independent, Qt-free port overlay. It does not modify or
push to Haiku's source tree. A Chromium checkout can continue to follow
upstream; `scripts/attach_overlay.sh` attaches this tree through Chromium's
official `ozone_extra_path` extension point.

## 프로젝트 목적과 한계

이 프로젝트의 주목적은 Chromium을 **32비트 Haiku(i386) 머신에서 가능한
최상의 성능으로 실행**하는 것입니다. 특히 구형 저사양 x86 장비에서 Qt
계층을 제거하고 Haiku API에 직접 연결하는 것에 우선순위를 둡니다.

이 프로젝트는 Chromium을 Haiku OS 전반에 제공하는 범용·공식 포트를
목표로 하지 않습니다. 다른 아키텍처, 다양한 하드웨어, 모든 Haiku 버전,
완전한 Chromium 기능 호환성은 우선 지원 범위가 아니며 i386 성능을 위해
일부 기능이나 이식성을 포기할 수 있습니다.

또한 소규모 비공식 포트이므로 Chromium 업스트림을 지속적으로 따라가는
동기화 작업과 보안 패치 적용이 늦어지거나 방치될 위험이 있습니다. 특히
Chromium은 보안 수정 주기가 빠르므로, 최신 업스트림 보안 패치가 적용되지
않은 빌드는 일반 웹 브라우징이나 민감한 계정에 안전하다고 간주해서는 안
됩니다. 사용자는 배포된 빌드의 기준 리비전과 마지막 보안 업데이트 날짜를
반드시 확인해야 합니다.

The production boundary is strict: code under `probe/` and
`chromium_overlay/` may use Chromium and Haiku APIs, but must not include or
link Qt. `make verify-no-qt` enforces that rule.

## Current milestone

The first milestone validates the risky 32-bit native boundary before pulling
in Chromium's multi-gigabyte source tree:

- Application Kit lifecycle (`BApplication` and `BWindow`)
- native buttons, text input, resize and mouse delivery
- a `B_RGB32` software frame presented directly by `BView::DrawBitmap`
- Chromium's out-of-tree Ozone registration file

The probe is not a web browser yet. It is the presentation and input path that
will replace Qt's window and scene-graph layer.

On Haiku:

```sh
make
make run
```

On 32-bit `x86_gcc2`, the Makefile invokes the modern `x86` secondary ABI with
`setarch x86`; the resulting port remains a 32-bit application.

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
must be launched with `--disable-gpu --in-process-gpu`; Skia raster pixels are
copied directly into a Haiku `BBitmap` and presented by `BView`.

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

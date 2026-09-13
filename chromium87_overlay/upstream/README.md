# Pinned upstream source supplements

QtWebEngine's Chromium 87 source distribution retains the Content Shell GN
targets but omits their implementation directories. R Chromium uses Content
Shell only as a temporary Qt-free embedder bootstrap, so the missing official
sources are vendored here without modification.

- Chromium version: `87.0.4280.144`
- Chromium revision: `38a74c624ca48a6acb2a6f427998be599b504eed`
- Source URL:
  `https://chromium.googlesource.com/chromium/src/+archive/38a74c624ca48a6acb2a6f427998be599b504eed/content/shell.tar.gz`
- Archive SHA-256:
  `cc10261322147315393991af1940e93af4a01999b0f8687772c3e9c0bc52e710`
- Extracted directories: `app`, `browser`, `gpu`, `renderer`, `utility`
- Extracted file count: 82

`scripts/attach_chromium87_overlay.sh` links these directories into the
checkout. Do not edit these files for Haiku-specific behavior; carry such
changes as ordered patches or separate R Chromium sources.

## Individually missing files

The same distribution also drops two loose files that `content/shell/BUILD.gn`
and `shell_resources.grd` still reference, in a directory the five directories
above do not cover: `content/shell/resources/`. The build did not reach that
target until 2026-08-29 (edge 58973 of the content_shell phase), which is why
this went unnoticed through every earlier gating pass.

- `content/shell/resources/brokenCanvas.png`
  SHA-256 `4be6ed6b65e224d5dc40a0c7d0dd62d876c80f245bcf19842a578393a8b6d8cf`
- `content/shell/resources/shell_devtools_discovery_page.html`
  SHA-256 `50b3234f4e7a961e977f312b1906b2dcf85ba7f6640e2e45ee46e5fc42ba1d7e`

Both fetched from the exact pinned revision above (not the version tag) and
byte-identical to the tag-fetched copies, checked before vendoring:

    https://chromium.googlesource.com/chromium/src/+/38a74c624ca48a6acb2a6f427998be599b504eed/content/shell/resources/<file>?format=TEXT

`attach_chromium87_overlay.sh`'s "missing_sources" step links these the same
way as the directories above -- by relative path under
`missing_sources/`, so `content/shell/resources/brokenCanvas.png` here becomes
a symlink at that same path in the checkout. Any other individually-missing
file the distribution turns up later belongs in `missing_sources/` the same
way, not copied directly into the checkout.

A related but different bug turned up one level further down the resource
graph: `components/resources/components_resources.grd` carries a
`<part file="safe_browsing_resources.grdp" />` line, commented out.
`components/safe_browsing/core/file_type_policies.cc` (a dependency of
`content/shell/content_shell.ninja`, not just chrome/-only test binaries)
expects `IDR_DOWNLOAD_FILE_TYPES_PB` from the header this part generates, so
the omission genuinely blocks content_shell once the build reaches that
target (2026-08-30).

Unlike the two files above, this is not a missing-file case: the grdp file
and the three web_ui resource files it includes (`safe_browsing.html/css/js`
under `components/safe_browsing/content/web_ui/resources/`) are all already
present in the checkout, byte-identical to the pinned revision (verified by
fetching each from `.../+/38a74c624ca48a6acb2a6f427998be599b504eed/...` and
comparing SHA-256). Only the `<part>` line referencing them was disabled.
The fix is the one-line uncomment in
`patches/0058-enable-safe-browsing-resources-grdp.patch`; nothing belongs in
`missing_sources/` for this one.

`components/resources/components_resources.grd` still carries two other
commented-out parts, `about_ui_resources.grdp` and `dom_distiller_resources.
grdp`, plus `translate_resources.grdp` -- none of these three have been shown
to be needed by content_shell yet and are left alone for now.

## Crashpad OS dispatch gaps

Two more content_shell-blocking failures turned up right after the one
above, both in third_party/crashpad -- but neither is a QtWebEngine omission;
both are genuine gaps or over-broad conditions in the pinned Chromium
revision itself, confirmed by fetching the same file from
`.../+/38a74c624ca48a6acb2a6f427998be599b504eed/...` and finding the gap
already present there.

- `patches/0059-crashpad-minidump-debug-string-os-haiku.patch`: 
  `minidump_misc_info_writer.cc`'s `MinidumpMiscInfoDebugBuildString()`
  enumerates OS_MAC/OS_IOS/OS_ANDROID/OS_LINUX/OS_CHROMEOS/OS_WIN/OS_FUCHSIA
  and `#error`s on anything else. Crashpad's own tree already carries real
  `OS_HAIKU` branches in several neighboring files (this one was simply never
  given one). One-line fix: add an `OS_HAIKU` branch with `kOS = "haiku"` --
  this only feeds a human-readable debug string, so no correctness constraint
  beyond being an honest platform name.

- `patches/0060-skip-crashpad-handler-data-dep-on-haiku.patch`:
  `content/shell/BUILD.gn` pulls in the separate crashpad_handler executable
  as an order-only data_dep of content_shell's link step, gated on
  `is_win || is_linux || is_chromeos`. Because Haiku is treated as
  `is_linux = true` (BUILDCONFIG.gn, not our patch -- see "Build
  configuration" in AGENTS.md), this pulls crashpad_handler in for Haiku too.
  `handler_main.cc` dispatches its actual implementation with
  `OS_LINUX || OS_CHROMEOS || OS_ANDROID` / `OS_APPLE` / `OS_WIN` branches;
  Haiku matches none of them (`OS_LINUX` and `OS_HAIKU` are mutually
  exclusive -- see build/build_config.h), so declarations meant to be
  supplied by exactly one of those branches are missing outright. Unlike the
  minidump fix above, the Linux branch here is a full ptrace/pipe-based
  out-of-process handler built on Linux-specific process APIs with no
  upstream Haiku equivalent to fall back on -- reusing it risked a large,
  unverified cascade into Linux-only process-attach code, and writing a real
  Haiku backend is out of scope for getting content_shell running at all.
  Fixed instead by excluding Haiku from the data_dep, using the `is_haiku`
  GN variable BUILDCONFIG.gn already defines (`build/config/BUILDCONFIG.gn:304`).
  content_shell never links `handler_main.o` into its own binary (confirmed
  absent from content_shell's link line) -- crashpad_handler is only ever
  launched by content_shell at runtime as a subprocess, so this means content_shell
  runs without an out-of-process crash handler on Haiku, an acceptable gap
  for a Qt-free embedder bootstrap.

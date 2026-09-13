# Browser UI plan: from content_shell to R Chromium

Written 2026-09-03, while the final link was still building, so that the UI
work can start the moment `content_shell` exists. Everything here is read off
the actual sources in this repo; nothing has been compiled or run yet, and the
skeleton that accompanies it (`shell_platform_delegate_haiku.cc`) is a starting
point, not tested code.

## What already exists

The hard part is done. `chromium87_overlay/ozone/haiku_window.{h,cc}` (458
lines) already creates a real `BWindow` with a `HaikuContentView : public BView`
inside it, and Chromium renders into that view. Input, cursor, clipboard and
screen are covered by their siblings in the same directory.

`HaikuWindowManager` hands out a `gfx::AcceleratedWidget` per window and keeps
the widget -> `HaikuWindow*` mapping under a lock, because the Chromium UI
thread and the BWindow looper threads both need it.

So the remaining work is chrome around the content, not rendering.

## What is missing

Against the goals in AGENTS.md:

| Goal | State |
|---|---|
| Google rendered in a Haiku window | ozone is written; needs the link to finish, then verification |
| Icon-only Back / Forward / Reload | nothing |
| Address field | nothing |
| Date-grouped searchable bookmarks | nothing -- storage and UI both |
| Desktop install with the blue Chromium icon | nothing -- needs an `.rdef` with a vector icon |

`probe/main.cpp` is the only place in the repo that uses BeAPI controls; it is
worth reading as a reference for BWindow/BView construction, but it is a
standalone probe, not part of the browser.

## Where the UI attaches

`ShellPlatformDelegate` (vendored, `upstream/content_shell/browser/
shell_platform_delegate.h`) is the extension point, and it already declares
exactly the hooks the goals need:

- `CreatePlatformWindow(Shell*, const gfx::Size&)`
- `SetContents(Shell*)`
- `EnableUIControl(Shell*, UIControl, bool)` -- Back/Forward enablement
- `SetAddressBarURL(Shell*, const GURL&)`
- `SetIsLoading(Shell*, bool)` -- Reload vs Stop
- `SetTitle(Shell*, const base::string16&)`
- `GetNativeWindow(Shell*)`

The other direction is `Shell`'s public API
(`upstream/content_shell/browser/shell.h`): `LoadURL`, `GoBackOrForward(int)`,
`Reload`, `Stop`, plus `UpdateNavigationControls`. `Shell::ActionPerformed(int)`
and `Shell::URLEntered(const std::string&)` exist precisely so a platform UI
can funnel button and address-bar events back in.

This build currently compiles `shell_platform_delegate_aura.cc`, which creates
an aura `WindowTreeHost` and no chrome at all -- that is why content_shell will
come up as a bare window. A `shell_platform_delegate_haiku.cc` replaces it.

## The design decision, and why

Keep aura. The aura delegate owns a `ShellPlatformDataAura` whose
`WindowTreeHost` is what ozone backs with the `BWindow`; throwing that away
would mean reimplementing the compositor and input plumbing. The Haiku delegate
should do what the aura one does *plus* add native chrome to the BWindow that
already exists underneath.

That needs one small, well-defined change in the ozone layer.
`HaikuContentView::AttachedToWindow()` currently does:

    MoveTo(0, 0);
    ResizeTo(window->Bounds().Width(), window->Bounds().Height());
    SetResizingMode(B_FOLLOW_ALL_SIDES);

i.e. the content view fills the whole window. A toolbar needs the content view
inset from the top instead. Give `HaikuWindow` a purely geometric "top inset"
(no knowledge of navigation -- ozone must stay a platform layer), and have the
delegate set it when it builds the toolbar. `B_FOLLOW_ALL_SIDES` keeps working:
with the view moved to `y = inset` and sized `height - inset`, a window resize
still tracks correctly.

Chromium's idea of the window size must then be the content area, not the whole
frame, or the page will be drawn under the toolbar. `HaikuWindow::GetBounds`
and the bounds it reports from the looper both need the inset subtracted.

## The threading constraint

**This is the part that will bite.** BeAPI controls deliver their messages on
the BWindow's looper thread. `Shell::LoadURL` and friends must be called on the
Chromium UI thread. Calling Shell directly from a `BButton`'s message handler
is a data race.

`HaikuWindow` already solves exactly this problem and its solution should be
copied rather than reinvented: looper-side code posts to the UI thread and
reaches the Chromium object through a `base::WeakPtr`, so events already in
flight when the window is torn down are dropped instead of landing on freed
memory (`OnEventFromLooper`, `OnBoundsFromLooper`, `weak_factory_`).

The same shape applies in reverse for `SetAddressBarURL` / `EnableUIControl` /
`SetIsLoading`, which arrive on the UI thread and must touch BeAPI controls:
those need to be posted to the looper, or the control must be locked
(`BWindow::LockLooper`) before touching it.

## Suggested order of work

1. **Verify the bare window first.** Once the link succeeds, run content_shell
   and confirm Google renders in the BWindow before adding anything. That
   isolates ozone bugs from UI bugs.
2. **Top inset in ozone.** Geometric only; verify the page still renders,
   shifted down, with a stub grey toolbar area.
3. **Toolbar controls.** Back/Forward/Reload as `BButton`s (icon-only, so
   `BBitmapButton`-style with vector icons from an `.rdef`), address field as a
   `BTextControl`. Wire them to `Shell` through the post-to-UI-thread pattern
   above.
4. **The delegate hooks.** `SetAddressBarURL`, `SetIsLoading`,
   `EnableUIControl`, `SetTitle` -- each posts to the looper.
5. **Bookmarks.** Storage first (a simple file under the app's settings
   directory, grouped by date), then a searchable list UI. This is the largest
   remaining piece and is independent of the rest.
6. **Desktop install.** `.rdef` with the blue Chromium vector icon, installed to
   the Desktop.

## Open questions to resolve when the code is in front of us

- Does `HaikuContentView` need to stop being `B_FOLLOW_ALL_SIDES` once a
  toolbar sits above it, or does the inset approach hold on every resize path?
- Where does `Shell::UpdateNavigationControls` get called from, and is it
  enough to drive Back/Forward enablement, or does the delegate need to observe
  the `NavigationController` directly?
- Icon-only buttons: does this Haiku version's `BButton` take a bitmap, or is a
  custom `BView` with `DrawBitmap` needed?

## A trap for the Desktop install step

From rworldradio-4c, who hit it re-baking an app's resources on this same
machine (2026-09-04), and worth knowing before step 6 above:

**`mimeset -f` does not update an existing `SYS:NAME` attribute.** If the
attribute is already present it is left alone, so a changed
`app_name_catalog_entry` in the `.rdef` shows up in the binary's resources but
not in the attribute Deskbar actually reads for the Applications menu. Deleting
the attribute and re-running `mimeset -F` did not restore it either; they had
to set it explicitly with `addattr`.

So after `rc` + `xres` on the R Chromium binary, verify with `listattr` that
`SYS:NAME` says what the `.rdef` says, and `addattr` it if not. The resource
being correct is not evidence that the attribute is.

## Two more things that bite at install time

From haiku-a5 (2026-09-04), both relevant to step 6:

**`AddFilesToPackage` does not look at ELF headers.** The R* applications used
to be wired into the Haiku image through `SYSTEM_APPS` in
`build/jam/images/definitions/minimum` and a `renkuApps` block in
`build/jam/packages/Haiku`, reading binaries from a single flat
`data/renku-apps` directory with no architecture condition on it. Only the
arm64 cross-build script ever populated that directory, so on a tree where an
arm64 build had run, **arm64 binaries were silently packaged into the x86
image**; on a fresh clone the x86 build instead stopped with "don't know how to
make RMemo". That wiring was removed in 63c19de6a5, but the underlying lesson
stands: nothing in the packaging path will notice if R Chromium's binary is
built for the wrong architecture. Check it explicitly.

**Apps are distributed through a package repository now, not the image.** The
arm64 one is at https://renku-repo.coroke.net/arm64 and is built by
`~/Workspace/renku-arm64/make-renku-repo.sh`. Repositories are per-architecture
because `repo.info` carries exactly one architecture, so an x86 repository is a
separate one built by the same script with `RENKU_REPO_ARCH` and
`RENKU_REPO_BINS` changed. That is the route for shipping R Chromium once the
binary exists, rather than adding it back into the image definitions.

**Resources are not attributes, and Deskbar reads attributes.** Also from
haiku-a5 (2026-09-04), and the same shape as the `SYS:NAME` trap above: when an
application is rolled into an `.hpkg`, an executable that carries its icon and
signature only as *resources* will not appear in the Deskbar menu. The
resources have to be promoted to attributes with `resattr`, and then
`mimeset -f --all --mimedb <output-mimedb> --mimedb <system-mimedb>` run to
create the `BEOS:TYPE` attribute and the matching `mime_db` entry. They lost
several hours to this. Do it as part of step 6, not after noticing R Chromium
is missing from the menu.

## The check that `requires:` does not perform

From rwebpositive-arm64-21 (2026-09-08), who installed a rebuilt
`haikuwebkit_x86` on this same machine, watched WebPositive fail to start, and
rolled it back eight minutes later:

    runtime_loader: Cannot open file libavif.so.13
      (needed by /boot/system/lib/x86/libWebKitLegacy.so.1.9.19)

Their sysroot had libavif 0.9.3 (soname 13); the machine has 1.4.2 (soname 16).
They had copied the `requires:` list from the official package without changing
a character and diffed it to be sure, and it still did not catch this: a
package's `requires:` is what the *package* declares, `DT_NEEDED` is what the
*binary* actually linked against, and nothing reconciles the two. The solver saw
`lib:libavif_x86>=16.4.2` satisfied and installed happily; the loader refused.

So before packaging R Chromium: pull the binary back out of the built hpkg and
compare `readelf -d` NEEDED entries against a known-good reference, one by one.
The `requires:` list agreeing proves nothing about whether the thing will start.

**And do not trust a zero exit code here.** Building after they swapped the avif
library gave `BUILD-EXIT=0` with no relink: ninja stats the symlink's target,
and the replacement's mtime (July 7) was older than the existing output
(September 7), so the output looked current. This is the same shape as the
build-side failure recorded above -- ninja exiting 0 having silently stopped 17
edges short of the link. On this machine the outcome is the artifact's mtime and
size, never the exit status.

## What actually stood between the link and a window (2026-09-10/11)

Three defects, found in this order, each hidden behind the one before it.

**1. Nothing showed the window.** `ShellPlatformDataAura::ShowWindow()` calls
`host_->Show()`, and no caller on this path invokes it. The aura delegate is
upstream's web-test configuration -- the host backs an offscreen compositor and
is never meant to be visible -- while every configuration that does show a
window goes through `shell_platform_delegate_views.cc`. Haiku builds with
`toolkit_views = false`, so the test delegate is what a real browser window
gets. One `platform_->aura->ShowWindow();` in `CreatePlatformWindow()` fixes it.

Diagnosed by logging in ozone: `HaikuWindow`'s constructor ran and reported
800x600, and `HaikuWindow::Show()` was never reached.

**2. The ozone overlay on the build machine was five weeks stale.** The Mac
repository had 19 files; `/boot/home/rchromium-native` had 11, missing
clipboard, cursor, event translation and screen, with `haiku_window.cc` at 173
lines against 496. Everything built and linked, so nothing complained. The
lesson is the one already in this project's notes: a repository file list is a
proxy. The authoritative signal was `out/obj/haiku_port/ozone/ozone/*.o`.

**3. RTTI.** Haiku builds libbe.so with RTTI and depends on it:
`BWindow::_FindView()` does a `dynamic_cast<BView*>` on the handler for every
message the window looper delivers. Chromium builds `-fno-rtti`, so a BView
subclass compiled in an ordinary Chromium target has no typeinfo for that cast,
and `__dynamic_cast` faults reading a null typeinfo pointer -- at offset 4,
which is exactly the `SEGV_MAPERR 00000004` seen. The window appeared and the
process died about a second later, on the thread named `w>R Chromium`.

Turning RTTI on for the whole ozone target fails the other way: its remaining
classes derive from `CursorFactory`, `PlatformScreen` and `SurfaceFactoryOzone`,
and emitting typeinfo for those subclasses leaves undefined references to base
typeinfo no `-fno-rtti` build provides. The BeAPI-derived classes have to be
split into their own `source_set` built with `-frtti`; that is
`haiku_beapi_views.cc` and the `beapi_views` target.

**Any future class deriving from a BeAPI type belongs in that target**, not in
the main ozone one. The toolbar work in this document adds several -- BButton
subclasses, a BTextControl subclass -- and each of them will be dynamic_cast by
BeAPI the same way.

### Two things that cost hours and were not the bug

`--gc-sections` was suspected for the same crash, on the theory that it had
discarded typeinfo. It had not. Testing that cost a link attempt, and the
attempt is worth recording for a different reason: **linking without
`--gc-sections` fails even on a freshly booted machine.** The output is
172,967,599 bytes rather than 141 MB intermediate, and ld is killed writing it.
So the flag is not optional here, and it is not the cause of anything.

The backtrace also misattributed the crash: Haiku's debug report named
`DiscardableSharedMemoryManager::Bind + 0x17`, and a guard placed there never
fired. With `--gc-sections` and `symbol_level = 0` the report resolves an
address to the nearest preceding symbol, which can be an unrelated function.
Trust the frame list's shape, not the innermost name.

## First pixels on screen (2026-09-11)

Chromium 87 rendered web content into a Haiku BWindow for the first time:
`data:text/html,<body style="background:#ff0000">` filled an 800x600 window
solid red. The binary is preserved at `preserved/20260911-201426/`.

The working path, end to end:

    renderer -> compositor -> SkSurface
      -> HaikuCanvas::PresentCanvas
      -> HaikuContentView::Present   (pixels copied into a BBitmap)
      -> BView::Draw                 (DrawBitmap on the looper thread)
      -> app_server

**It needs `--single-process` today.** With separate processes the renderer
dies on `SEGV_MAPERR 00000004` -- the same null-at-offset-4 shape as the RTTI
bug, so it is worth checking first whether some other BeAPI-derived class is
being dynamic_cast in the renderer. The browser survives that crash and keeps
its window, which is why the multi-process runs show an empty window rather
than an exit.

It also needs `--disable-gpu --in-process-gpu`, for the reason in the next
section.

### The widget does not survive the process boundary

Two defects stack here, and both are worked around rather than fixed.

`OzonePlatformHaiku::InitializeGPU()` creates a second `HaikuWindowManager`
when it runs in the GPU process -- logged as `InitializeGPU:
window_manager_=(nil)`. That registry has no windows in it, so the canvas can
never find a view. `--in-process-gpu` makes both halves share one manager.

Even then, viz asks for a canvas with `gfx::kNullAcceleratedWidget` rather than
the widget this port handed out (`CreateCanvasForWidget widget=0` against
`publishing widget=1`). `HaikuWindowManager::SoleWidget()` resolves a null
widget to the only registered window, which is correct for content_shell and
wrong the moment a second window exists -- it returns null rather than guess.

The real fix is to make the ozone widget a handle that means something in
another process. An index into a browser-process map cannot: the BView lives in
the browser and the GPU process cannot draw into it. Either the pixels travel
back over IPC, or the BBitmap is allocated in shared memory both processes map.
Until then the two flags above are required.

## Real pages load (2026-09-11, later)

`http://example.com` and `https://www.google.com` both reach the network, come
back, and paint into the BWindow. Text renders: a `data:` page reading "hello"
on a green background drew correctly with the font service enabled.

Working flags today:

    --single-process --disable-gpu --in-process-gpu
    --disable-in-process-stack-traces

### fontconfig was never the problem

Patch 0079 disabled the font service on Haiku because content_shell crashed in
`FcCharSetSubtractCount` while fontconfig scanned fonts, and the twelve-frame
backtrace was a coherent fontconfig chain. That patch has been reverted: with
the RTTI and RunProcess defects fixed, the font service works and text renders.

The evidence that settled it is a standalone harness, `/boot/home/fctest*` on
the build machine, built from Chromium's own
`libfontconfig.a` plus the freetype, harfbuzz, libxml, ICU, zlib, libpng and
brotli objects out of `out/rchromium_native/obj`. It scans all 112 Haiku fonts
through `FcInit()` -- the same entry point the crash backtrace showed -- and
builds a langset for every one of them, with and without `--gc-sections`.
Nothing crashes. Keep that harness: it turns a font hypothesis from a
forty-minute rebuild-and-link into a few seconds.

### The remaining crash is in signal-handler setup, not in what it reports

Without `--disable-in-process-stack-traces`, loading a real page dies with
`SEGV_MAPERR 00000004`; with it, the same page loads and paints. A `data:` URL
is fine either way, so this is not startup -- it happens during network or page
load.

What that flag actually gates is two things, both in content/app:
`SetupSignalHandlers()` (which resets twelve signals to SIG_DFL and clears the
signal mask) and `base::debug::EnableInProcessStackDumping()` (which installs
`StackDumpSignalHandler` for SIGILL/SIGABRT/SIGFPE/SIGBUS/SIGSEGV, and SIGSYS
too since that exclusion is written `#if !defined(OS_LINUX)`).

The important asymmetry: with the flag, no Chromium handler is installed, so a
genuine SIGSEGV would reach debug_server and leave a crash report. None appears
and the process keeps running. So there is no genuine fault -- the handler
infrastructure is what produces one. That points at the setup itself rather
than at whatever it would have reported, and it is the next thing to chase.
It may well be the same defect as the renderer dying in multi-process mode.

### Correction: the stack-dumping flag is not what decides it

The section above overstates its case. Measured afterwards: six runs of a
`data:` URL crashed zero times with `--disable-in-process-stack-traces` and
zero times without it, against an earlier sample of four `http://example.com`
runs that crashed twice. `RCH_NO_SIGRESET` and `RCH_NO_STACKDUMP` were added to
`content/app/content_main.cc` and `base/debug/stack_trace_posix.cc` so one
binary could test each half of what the flag gates; all three conditions
passed. So `SEGV_MAPERR 00000004` is intermittent and the flag is at most a
correlation, not the cause. Four runs is not a sample.

That is the whole lesson of this crash so far: guessing at an intermittent
fault without a stack trace burns a forty-minute rebuild per guess. Make the
crash report itself usable first.

### Haiku can produce real stack traces (patch 0080)

Every crash printed `[end of stack trace]` with nothing above it, because
HaikuPorts excluded Haiku from all five backtrace paths in
`base/debug/stack_trace_posix.cc` -- `CollectStackTrace()` returns 0 on this
platform. That exclusion is no longer warranted:

  * Haiku ships `backtrace()` and `backtrace_symbols()` in
    `/boot/system/lib/x86/libexecinfo.so`, and `-lexecinfo` was **already** on
    the content_shell link line (v8/BUILD.gn adds it). It simply never appeared
    in `readelf -d` output, because `-Wl,--as-needed` drops a library nothing
    references -- which is exactly what "content_shell does not link
    libexecinfo" looked like from the outside. Check the link line, not the
    ELF header, before concluding a library is absent.
  * `libexecinfo_devel` is not installed, so there is no `<execinfo.h>`. The
    patch declares the two prototypes `extern "C"` rather than making the port
    depend on an optional package.
  * The build compiles with `-fno-omit-frame-pointer`, so Haiku's
    frame-pointer-based `backtrace()` has a chain to walk. Verify this before
    trusting the result of any future flag change.

One subtlety. `base/BUILD.gn` defines `USE_SYMBOLIZE` under
`if (is_linux || is_chromeos)`, and GN sets `is_linux` for Haiku -- the same
root pattern as every other defect in this port. But `google::Symbolize()`
finds the ELF image for an address by reading `/proc/self/maps`, which Haiku
does not have (`base/debug/proc_maps_linux.cc` is stubbed by patch 0065).
Haiku's own `backtrace_symbols()` resolves through the kernel image API and
returns real function names, so the patch `#undef`s `USE_SYMBOLIZE` for this
one translation unit.

`base` is a jumbo target, so that `#undef` outlives the file it is written in:
`merge_for_jumbo.py` puts `base/debug/debugger_posix.cc` -- the only other user
of `USE_SYMBOLIZE` in base -- ahead of `stack_trace_posix.cc` in
`gen/base/base_jumbo_34.cc`, so nothing breaks today, but the patch restores
the define at the end of the file rather than depend on that ordering. Any
preprocessor state changed in a jumbo target needs the same treatment.

### The link stopped being probabilistic

Eighteen consecutive content_shell links died at exactly 141,614,983 bytes of
output -- the same byte count with and without `--no-keep-memory`. A number
that repeats to the byte is not memory pressure behaving randomly, so the next
step was measurement rather than another flag guess. `scripts/linkprobe.sh`
runs one attempt and samples every ten seconds:

    04:21:11  ld starts, 1.43 GB free
    04:22:26  17 MB free
    04:25:29  output reaches 141,614,983 bytes, 47 MB free
    04:26:24  33 MB free, killed with signal 21

So it is genuine exhaustion -- ld peaks around 1.7 GB against 179.8 MB of
inputs, because `-ffunction-sections`/`-fdata-sections` split the program into
hundreds of thousands of sections and both `--gc-sections` and `-Wl,-O2` build
a whole-program structure over all of them. Two changes made the link succeed
on the first attempt (188,504,316 bytes) at nine hours of uptime, where the
previous eighteen had failed:

  * `RCHROMIUM_NO_BUILDID=1`, new in the wrapper: drops `-Wl,--build-id`, which
    makes ld hold the finished ~188 MB image to hash it. Nothing in this port
    reads a build id -- crashpad is disabled (patch 0060) and there is no
    symbol server.
  * `RCHROMIUM_GC_NO_O2=1`, present but never measured until now: drops
    `-Wl,-O2`, a whole-program string tail-merge pass, and keeps
    `--gc-sections`, which is what holds the output down to 188 MB rather than
    173 MB of un-collected sections.

Also worth 72 MB: a stale `Debugger --save-report --team NNNN` left over from
an earlier content_shell crash. Haiku starts one per crash and it sits there
holding the reported team's memory. Check for it before a link.

`scripts/linkretry_slim.sh` is the loop that uses both flags.

### The intermittent SEGV was a second RTTI defect, and it was not intermittent

`SEGV_MAPERR 00000004` looked random: two crashes in four runs one hour, none
in twelve the next. It correlated with nothing in the flags. What it did
correlate with was **taking a screenshot** -- every crashing run had called
`screenshot`, and none of the surviving runs had. Tested directly: three runs
out of three died on the *first* screenshot, twelve runs that never took one
survived.

With patch 0080 in place the stack is readable, and it says everything:

    __dynamic_cast+0xc3
    BApplication::_WindowAt(unsigned long, bool)
    BApplication::WindowAt(long)
    BLooper::resolve_specifier(BHandler*, BMessage*)
    BLooper::task_looper()
    BApplication::Run()
    ui::HaikuApplication::Run()

`screenshot` sends the application a scripting message. `resolve_specifier()`
answers it through `BApplication::WindowAt()`, and `_WindowAt()` walks Haiku's
global looper list calling `dynamic_cast<BWindow*>` on every entry. One entry
was `HaikuClipboard::ChangeWatcher`, a `BLooper` subclass still compiled in the
ordinary `-fno-rtti` ozone target -- so its vtable's typeinfo pointer is null,
and `__dynamic_cast` faults reading offset 4 of it. Exactly the fault the
`BrowserNativeWindow` split fixed for windows, in a class that split missed.

It is now in `haiku_beapi_views.cc` behind `CreateClipboardChangeWatcher()` /
`StopClipboardChangeWatcher()`, declared in the new `haiku_beapi_looper.h`.

**The rule this makes explicit: every class deriving from BLooper, BHandler,
BWindow or BView must be compiled in the `beapi_views` RTTI target, not only
the ones whose own messages are dispatched.** A BLooper anywhere in the process
is reachable by any dynamic_cast libbe performs over the global list, whether
or not that looper has anything to do with the message being answered.

### NSS root certificates were never broken

`nss_util.cc(283) After loading Root Certs, loaded==false: NSS error code:
-8018` is harmless. -8018 is `SEC_ERROR_UNKNOWN_PKCS11_ERROR`, and a standalone
harness (`/boot/home/nsstest2.c` and `certcount.c` on the build machine)
reproduces it in twenty lines:

  * `~/.pki/nssdb/pkcs11.txt` already lists a module named "Root Certs" with
    `library=/boot/home/.pki/nssdb/libnssckbi.so` -- a path that does not
    exist, though Haiku's runtime_loader finds the real
    `/boot/system/lib/x86/libnssckbi.so` by basename anyway.
  * `NSS_InitReadWrite()` therefore registers it, `loaded=1 slots=1`, before
    crypto/nss_util.cc runs.
  * `SECMOD_LoadUserModule("Root Certs", ...)` then fails on the duplicate
    name, which is all the error says.

`PK11_ListCerts(PK11CertListCA)` returns **172 CA certificates**, so the trust
store is fully populated. Drop this from the open list; no patch is needed.

### What the link actually needed: a reboot, and why

`nice -n 15` on ld looked like the fix -- it linked on the first attempt after
twenty-four failures -- but it is not one. Six more attempts with it failed at
the same 141,651,847 bytes, and a wrapper test showed the flags were being
applied all along. What settled it:

| uptime | link result |
| --- | --- |
| freshly booted | attempt 1, twice in a row |
| 10 hours | 1-6 attempts |
| 14-16 hours | 30 consecutive failures |

Idle free memory is the same 1.76 GB either way, and ld needs the same ~1.7 GB
either way. What changes is how much of the page cache is dirty: ld dirties
pages faster than Haiku's page writer flushes them, free pages cross the
low-resource threshold, and the killer fires. A reboot empties the cache. That
is the whole mechanism, and it is the only lever that reliably works.

So: **link right after a reboot.** `scripts/after_reboot_finish.sh` does the
whole sequence -- link, check Qt is absent, install to the Desktop, then run
with and without the toolbar and screenshot both -- so one reboot buys one
complete verification pass rather than one link.

Two things learned the hard way alongside it:

  * ld truncates its output to zero at the start of every attempt, so a
    failing retry loop **destroys the last binary that worked**. That happened
    here: a good build was overwritten by six failures, leaving nothing to run
    on a machine that could no longer link. Every retry script now copies the
    result to `/boot/home/content_shell.last-good` the moment it exists.
  * The retry scripts run ninja, ninja re-runs gn when a BUILD.gn changes, and
    gn asks pkg-config for nss. Without `PKG_CONFIG_PATH` pointing at
    `/boot/system/develop/lib/x86/pkgconfig` that fails with "Package nss was
    not found" and the loop scores it as a link failure that never reached ld.

### The toolbar broke painting, and the bug was a transient window size

With the toolbar attached the browser came up with chrome drawn and the web
content permanently blank: exactly one `PresentCanvas` in the log and not one
`computed_style` message after it, meaning Blink never ran layout at all. The
A/B switch (`RCH_NO_TOOLBAR=1`, still in `shell_platform_delegate_aura.cc`)
showed painting was fine without it, and the logging added at the same time
showed why:

    [RCH] OnBoundsFromLooper 800x600
    [RCH] OnBoundsFromLooper 800x630     <- this
    [RCH] OnBoundsFromLooper 800x600

`HaikuWindow::SetTopInset()` grows the BWindow by the toolbar height and then
shrinks the content view back. The content view was `B_FOLLOW_ALL_SIDES`, so
it followed the window up to 630 first, and BView reported that intermediate
size through `FrameResized()` -> `OnBoundsChanged()`. aura saw three bounds
changes before the first frame, and the renderer sat waiting on visual
properties for a surface size that no longer existed.

The fix is to pin the view for the duration (`SetResizingMode(B_FOLLOW_NONE)`
around the resize). Its size then never actually changes -- 800x600 before and
after -- so BView issues no `FrameResized()` and aura sees no bounds change,
which is the truth: the content area only moved down.

The general shape is worth remembering: **a BView that follows its window will
report every intermediate size the window passes through, and anything
downstream of `OnBoundsChanged()` sees them as real.**

### Desktop install

`scripts/install_to_desktop.sh` puts the application in `~/RChromium` and a
launcher script named "R Chromium" on the Desktop. The binary cannot go on the
Desktop itself: `PathProviderHaiku` (patch 0078) derives `DIR_ASSETS` from the
executable's own path, so it has to stay beside `content_shell.pak`,
`icudtl.dat` and `locales/`.

Tracker draws a file's own `BEOS:ICON` in preference to its MIME type's icon,
so a shell script shows the Chromium icon exactly as an application would --
which is why there is no `.rdef` to compile here. The icon itself is
`assets/rchromium.hvif`, 291 bytes of HVIF ("ncif" magic, #8FC4E7 over
#539CCF), extracted with `catattr -r BEOS:ICON` from the earlier Desktop entry
rather than re-authored.

`addattr -t VICN` does not work -- "attribute type VICN is not valid". A
four-character type ID goes in through `-c`, and `-f` feeds the raw HVIF bytes
so nothing has to hex-encode a binary blob through a shell argument:

    addattr -f rchromium.hvif -c VICN BEOS:ICON "/boot/home/Desktop/R Chromium"

Tracker does not notice a new Desktop entry's attributes until it is
restarted.

### Rendering starts about a third of the time, and nothing on our side differs

The browser renders real pages -- news.naver.com in full Korean layout, Google
News, google.com -- and navigating from the address bar path works. It just
does not do it every launch. Measured on one machine over one evening, same
binary each time:

| condition | rendered |
| --- | --- |
| default flags | 3 / 6 |
| `RCH_NO_TOOLBAR=1` | 3 / 6 |
| `--num-raster-threads=4` | 1 / 6 |
| `--num-raster-threads=1 --renderer-process-limit=1` | 0 / 6 |
| 12 s between runs | 0 / 4 |

A failed launch is unmistakable: Blink resolves **zero** styles
(`computed_style.cc` NOTREACHED count 0, against ~2900 on a run that renders),
exactly one `PresentCanvas` for the life of the process, one
`cert_verify_proc_builtin` message where a rendering run has four, and no
CONSOLE output at all -- so the main document's TLS completes and nothing
downstream of it ever happens. The process is not idle while this goes on:
`top` shows its threads burning ~76% of both Atom cores.

What is *not* different is our platform layer. Reducing the `[RCH]` log to a
multiset of event kinds, a failing run and a succeeding run are identical --
same InitializeUI/InitializeGPU/CreatePlatformWindow/publishing
widget/CreateCanvasForWidget/ResizeCanvas events, in orders that vary
independently of the outcome. Only the repeat counts differ: 1 GetCanvas and 1
PresentCanvas versus 3 and 26. Ozone does its whole job either way.

Ruled out by measurement, each with a rebuild or an A/B run:

  * the native toolbar (identical rate with it suppressed);
  * `MAP_NORESERVE` on PROT_NONE reservations in PartitionAlloc -- Haiku has
    no overcommit and V8 reserves large ranges, which is a real difference
    from Linux, but adding it changed nothing here (verified in the build by
    comparing the jumbo object's mtime against the header's);
  * `WebContents::WasShown()` versus `RenderWidgetHostView::Show()`. The view
    is already showing every time it is checked. Replacing WasShown() with
    Show() made five consecutive launches render nothing, so the WasShown()
    call stays and the Show() experiment is gone;
  * uptime on its own -- the first test after a reboot, at five minutes, also
    failed, while runs at half an hour succeeded;
  * raster thread count and the gap between launches, both of which only
    moved the rate around.

Multi-process is worse and fails differently: 0/4, with the *renderer* process
dying on `font_cache.cc(472) Check failed: false` -- font fallback exhausted
-- after the page has loaded and run script. The browser process is fine, so
this is the renderer not getting a usable fontconfig; Chromium on Linux
proxies font matching to the browser through the sandbox support interface,
which this port does not implement. Fonts themselves are present:
`NotoSansCJKjp-VF.otf` covers Hangul, which is why Korean renders when it
renders at all.

So the next step is renderer-side instrumentation: find why Blink never starts
on a launch where the document arrives. Nothing further can be learned from
the ozone side, and flag roulette has been exhausted.

### The intermittent stall was the GPU-channel handshake; the fix is one flag

The "renders about a third of the time" stall, chased across three sessions,
was not memory, not the toolbar, not MAP_NORESERVE, not visibility. It was the
renderer blocking at startup in `RenderThreadImpl::EstablishGpuChannelSync()`
-- a synchronous wait for an in-process GPU channel -- and losing a startup
race on a 2-core Atom often enough to fail two launches in three.

How it was found, since guessing had failed for three sessions:

  * `profile` kernel-panics on a binary this size (see the memory note), and
    there is no gdb for x86 Haiku. So a live stack walker was written on the
    user-space debugger API: `install_team_debugger` -> `debug_thread` ->
    `debug_get_instruction_pointer` -> walk the frame-pointer chain with
    `debug_get_stack_frame`, printing ` [0x..]` lines to feed
    `scripts/resolve_haiku_stack.py` against a same-run `listimage`. It is
    `scripts/stackwalk.c`; keep it, it is the only way to see a live thread
    here.
  * `PlatformThread::SetName` was an empty stub on Haiku, so every thread
    showed as "content_shell pthread". Wiring it to `rename_thread` (patch
    0083) made the stacks legible -- the spinning threads were
    `Chrome_InProcRendererThread` in `RequestNewLayerTreeFrameSink` and
    `CrBrowserMain` cycling message pipes.

The renderer already has the escape: `RequestNewLayerTreeFrameSink()` checks
`is_gpu_compositing_disabled_` and, when set, calls
`frame_sink_provider_->CreateForWidget()` directly with no GPU channel. That
flag is set by `--disable-gpu-compositing`. This port is a pure software
backend (`HaikuSurfaceFactory` reports `kGLImplementationDisabled`), so the
GPU channel was never anything but overhead and a race to lose. Adding the
flag: **6/6 launches render, `EstablishGpuChannel` called 0 times.** It is now
in `scripts/run_native_content_shell87.sh` and the Desktop launcher.

`--disable-gpu` alone is not enough: it disables GPU rasterization but the
compositor still tries to establish a channel for the command buffer until
compositing itself is switched to software.

Two supporting fixes landed alongside, each real but neither sufficient on its
own (both measured to change nothing by themselves, kept because they are
correct):

  * **patch 0082** buffers the kernel RNG. Haiku's `/dev/urandom` costs ~2 ms
    per read once its pool is drained by rapid reads (2000 sequential
    16-byte reads = 4.08 s; `getentropy` is identical, same source), and
    Chromium draws a RandUint64 for every Mojo pipe id -- thousands per page
    load. `base/rand_util_posix.cc` now reads 8 KiB blocks and hands out
    slices, pid-guarded for fork.
  * **patch 0084** queues `HostFrameSinkManager::CreateCompositorFrameSink`
    requests that arrive before viz is connected, instead of dropping the
    receiver. A dropped receiver makes the renderer report
    `DidLoseLayerTreeFrameSink` and re-request with no backoff, which added
    load to the same startup race.

Reliability: 6/6 launches of news.naver.com render fully (Korean layout,
tabs, article cards). First paint is ~1.0 s. Alternating navigation still has
a separate crash on the third load -- tracked next, independent of the stall.

### Two more fixes and a hard requirement: link and test on a fresh boot

After --disable-gpu-compositing gave 6/6 at low uptime, two things surfaced,
and disentangling them cost several rebuilds because results are meaningless
at high uptime -- see the requirement at the end.

**patch 0086 (frame-delete race).** Re-navigating crashed on the 3rd-4th load
in `render_frame_impl.cc(2559)`, `CHECK(!in_frame_tree_)` in the
kSpeculativeMainFrameForNavigationCancelled case (crbug.com/838348: the
browser cancels a speculative main-frame navigation as the renderer swaps it
in). Upstream disables this CHECK on Android because slow devices hit the race
easily; the 2-core Atom is such a device. First attempt copied Android exactly
-- skip the CHECK, fall through to `frame_->Detach()` -- and that REGRESSED it
to SEGV_ACCERR on every real page, because Detaching an already-swapped-in
main frame leaves the bad pointers the code's own comment warns of. The
localisation that caught it: `data:` URLs (no navigation) rendered fine while
every http(s) URL faulted. The correct fix mirrors the
kSpeculativeMainFrameForShutdown case in the same function: when
`in_frame_tree_`, `return` and ignore the delete. Inconsistent (the browser
thinks it has a remote main frame) but survivable, and neither a crash nor a
use-after-free.

**patch 0085 (MAP_NORESERVE), reconsidered.** Earlier this was applied, judged
to change nothing, and reverted -- but that judgement was made while the
GPU-compositing stall still masked everything. With the stall gone, the
remaining high-uptime failure was V8 executing a CodeRange that was never
backed: SEGV_ACCERR / ILL_PRVOPC inside V8 builtins on heavy pages, while
`data:` stayed fine. Haiku has no overcommit, so V8's large PROT_NONE
reservations are charged in full unless MAP_NORESERVE is set; under
accumulated commit charge the mprotect of a code page fails and executing it
faults. Re-applied to `page_allocator_internals_posix.h`. The transform
scripts are in `chromium87_overlay/patch-scripts/` (unified diffs pending a
machine reconnect to regenerate).

**The requirement.** A launch renders reliably only within roughly the first
few hours after a reboot. At 10-11 hours uptime, after dozens of content_shell
launches, even the fixed binary fails 0/6 with V8 code-execution faults on
heavy pages -- the kernel's memory subsystem is exhausted and no amount of
MAP_NORESERVE fully compensates on 2 GB. So: **link and measure on a freshly
booted machine.** Every inconsistent result today (6/6 vs 0/6 of the same
binary) traced to uptime, not to code. `scripts/validate_rng.sh` prints the
uptime at the top of its report for exactly this reason -- read it before
trusting the numbers below it.

**Final result, fresh boot (uptime 16 min):** 6/6 launches of news.naver.com
render fully; alternating news.naver.com <-> news.google.co.kr five times
crashes 0 times and keeps rendering (first paint 1-27 s, heavy first loads
slower); google.co.kr and google.com render complete Korean pages. Verified by
screenshot.

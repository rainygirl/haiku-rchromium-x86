# Chromium 114 port (x86, Qt-free)

A continuation of `chromium108_port/`, not a restart. The scripts and the
Haiku source files carry over; what does not carry over is where every patch
anchors, because 114 moved the code around.

## Why 114 and not 110

108 runs on this machine -- it builds, it links, Haiku's loader accepts it, a
window opens, text renders, TLS works, and **top-level await runs**, which is
what the whole port was for. What it does not do is render x.com, and the
reason is that x.com now asks for JavaScript newer than 108:

	TypeError: n.filter(...).map(...).toSorted is not a function
	  at abs.twimg.com/x-web/x-web/assets/locale-SttZuWL1.js

`Array.prototype.toSorted` is ES2023 and shipped in Chrome 110. V8 10.8
already has it, behind `--harmony-change-array-by-copy`, and turning that on
made `[].toSorted` appear -- and the page still did not mount, so there is
more behind it.

110 would buy exactly that one feature. The cost of moving is the same
whatever the target, because the work is re-anchoring ~242 patches, so the
target should be the highest version the constraints allow:

| constraint | where it bites |
| --- | --- |
| Atom Z520 is 32-bit only | not a problem: V8 keeps its ia32 backend |
| built with gcc, not clang | gets worse with every milestone |
| no Rust toolchain for Haiku x86 | `enable_rust = false` is the default **through 114**; from 119 skia's `bridge_rust_side` makes Rust impossible to switch off |

So 114 is the last milestone where Rust is off by default, and that is the
line this port stops at while it is still a gcc build.

## What carries over

| | |
| --- | --- |
| `port/port-target-guards.py` | 66 gn edits |
| `port/port-haiku-platform.py` | 162 source edits |
| `port/port-compile-fixes.py` | 8 edits |
| `port/port-os-detection.py` | 6 edits |
| `files/` | 39 whole files this port adds |

Every edit carries its own "already applied" test and prints
`PATTERN NOT FOUND` when its anchor is gone, so the first run against 114 is
itself the worklist.

## What is already known to need looking at

- the gn args: some were renamed or removed between 108 and 114
- `v8_snapshot_toolchain` still needs a 32-bit mksnapshot; the i686 Linux
  cross toolchain and qemu arrangement should carry over unchanged
- the Ozone/BeAPI backend, which moved once already from 87 to 108
- `K0002` (Haiku's libnetwork closing fd 0) is a system bug, not a Chromium
  one, and applies here too

## It builds, it links, it runs (2026-09-27)

	content_shell   275 MB, ELF32 i386
	ninja exit=0, failed edges 0

And on renku, first try:

	[RCH] OzonePlatformHaiku::InitializeUI
	[RCH] CreatePlatformWindow called
	[RCH] HaikuWindow ctor bounds=800x600
	[RCH] HaikuWindow::Show inactive=0
	DevTools listening on ws://127.0.0.1:47956/...

	title    = top-level await works
	body     = top-level await works | toSorted: 1,2,3
	UA       = Mozilla/5.0 (Haiku; Haiku BePC) ... Chrome/114.0.5735.199
	toSorted = present

The four things that each took days on 108 -- the loader refusing the
image over DF_STATIC_TLS, the renderer dying on a snapshot built by a
64-bit mksnapshot, the window never being shown, and Haiku's libnetwork
closing standard input -- all passed on the first run here, because
their answers came along in the port scripts. That is what the 108 work
bought.

`Array.prototype.toSorted` is present without a flag, which is what 110
shipped and what 108 could not do.

## x.com gets further and still does not render

![x.com stopped at the loading spinner](x-com-loading.png)

	ready     = complete
	URL       = https://x.com/i/jf/onboarding/web?...&mode=login
	title     = X - The Everything App / X
	nodes     = 74
	exceptions = 0
	resources = 250, none with a 4xx or 5xx

250 resources arrive -- entry-client, rolldown-runtime, i18n,
sentry-filter, authorize, react -- and nothing throws. The body is

	SCRIPT[0]  DIV[0, sr-only]  DIV[4 children]

and that last div is `id="loading-x-anim-0"`, the X logo spinner. So the
app boots, renders its loading state, and stops there. 108 never got
past the server-sent shell; this is further in and a different failure.

The window stays white, so the spinner in the DOM is not reaching the
screen either. Whether the app is waiting on something or the compositor
is not presenting is the next thing to find out, and those are different
problems.

## Three walls on the way to x.com (2026-09-28)

None of them was in this port's own code. All three were Chromium doing
the right thing everywhere except here, because Haiku was missing from a
list or getting a default that does not fit.

### 424 requests against a limit of 256

x.com's app fetches a module per route -- 424 of them at once. 247 came
back `net::ERR_INSUFFICIENT_RESOURCES` and the app sat on its loading
spinner waiting for code that was never going to arrive. No exception,
no failed HTTP status: silence.

Haiku starts a team with `RLIMIT_NOFILE` at 256 and allows 8192. Every
socket is a descriptor. Chromium already raises this, in
`BrowserMainLoop::PostCreateThreads`, and the comment above the call
could have been written for this machine -- "the default limit on Apple
is low (256), so bump it up". The call sits inside
`#if IS_APPLE || IS_LINUX || IS_CHROMEOS || IS_ANDROID`.

### A null video capture factory

With the modules arriving, the app ran its fingerprinting script and
asked `navigator.mediaDevices.enumerateDevices()` what cameras exist.
`CreatePlatformSpecificVideoCaptureDeviceFactory()` ends in

	#else
	  NOTIMPLEMENTED();
	  return nullptr;

and `VideoCaptureSystemImpl::GetDeviceInfosAsync()` calls through it
without checking. iOS answers the same question with the fake factory,
which reports no devices; "no camera" is true here anyway. Haiku has a
media_kit with video input, so this is a stub with a real answer behind
it, not a permanent one.

### A stack that was never big enough

Then the renderer died in `Builtins_IncHandler`, which looks like a V8
bug and is not one. From the crash report:

	pthread_19057_stack   0x70401000..0x70446000   276 KB
	faulting esp          0x70405040               16 KB above the bottom

Haiku gives a pthread 256 KB. V8 assumes about 984 KB and lets
JavaScript recurse until it reaches that. x.com's code is deep enough to
find the difference. `GetDefaultThreadStackSize()` in this port returned
0 -- "whatever pthreads gives you" -- under a comment claiming Haiku
grows it to 16 MB on demand. That is true of the main thread's stack and
not of a pthread's. It returns 4 MB now.

![x.com renders its login modal](x-com-modal.png)

DOM nodes went 74 -> 290, the crash is gone, and the window shows the
rounded white card and the spinner instead of nothing.

## x.com renders (2026-09-28)

![x.com's sign-in screen on Haiku](x-com-login.png)

The X mark, "See what's happening", the phone, Google and Apple buttons,
the email field with its focus ring, Continue, and the terms links. The
whole sign-in screen.

The spinner was not a fourth wall. DOM nodes went 290 -> 461 while I was
looking at it, 168 of them inside the dialog, and IndexedDB opened when
asked. Nothing was stuck: a 1.33 GHz Atom was working through x.com's
JavaScript, and it needed a few more minutes than I had given it. Worth
recording, because three real walls in a row makes the fourth pause look
like another one.

This is what the port was for. The app that Chromium 87 answers with

	SyntaxError: Unexpected reserved word

runs on Haiku x86.
